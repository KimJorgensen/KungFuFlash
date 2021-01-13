/*
 * Copyright (c) 2019-2020 Kim Jørgensen
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#define DAT_FILENAME "/KungFuFlash.dat"
#define FW_NAME_SIZE 20

#define CRT_SIGNATURE "C64 CARTRIDGE   "
#define CRT_CHIP_SIGNATURE "CHIP"
#define CRT_VERSION 0x100

#define EAPI_OFFSET 0x3800
#define EAPI_SIZE   0x300

static inline bool prg_size_valid(uint16_t size)
{
    // PRG should at least have a 2 byte load address and 1 byte of data
    return size > 2;
}

static bool prg_load_file(FIL *file)
{
    uint16_t len = file_read(file, dat_buffer, sizeof(dat_buffer));
    dbg("Loaded PRG size: %u\n", len);

    if (prg_size_valid(len))
    {
        dat_file.prg.size = len;
        return true;
    }

    dat_file.prg.size = 0;
    return false;
}

static bool p00_load_file(FIL *file)
{
    P00_HEADER header;
    uint16_t len = file_read(file, &header, sizeof(P00_HEADER));

    if (len != sizeof(P00_HEADER) ||
        memcmp("C64File", header.signature, sizeof(header.signature)) != 0)
    {
        wrn("Unsupported P00 header\n");
        return false;
    }

    if (prg_load_file(file))
    {
        memcpy(dat_file.prg.name, header.filename, sizeof(header.filename));
        dat_file.prg.name[16] = 0; // Null terminate
        return true;
    }

    return false;
}

static bool crt_load_header(FIL *file, CRT_HEADER *header)
{
    uint32_t len = file_read(file, header, sizeof(CRT_HEADER));

    if (len != sizeof(CRT_HEADER) ||
        memcmp(CRT_SIGNATURE, header->signature, sizeof(header->signature)) != 0)
    {
        wrn("Unsupported CRT header\n");
        return false;
    }

    header->header_length = __REV(header->header_length);
    header->version = __REV16(header->version);
    header->cartridge_type = __REV16(header->cartridge_type);

    if (header->header_length != sizeof(CRT_HEADER))
    {
        if (header->header_length != 0x20)
        {
            wrn("Unsupported CRT header length: %u\n", header->header_length);
            return false;
        }
        else
        {
            log("Ignoring non-standard CRT header length: %u\n",
                header->header_length);
        }
    }

    if (header->version != CRT_VERSION)
    {
        wrn("Unsupported CRT version: %x\n", header->version);
        return false;
    }

    return true;
}

static bool crt_write_header(FIL *file, uint16_t type, uint8_t exrom, uint8_t game, const char *name)
{
    CRT_HEADER header;
    memcpy(header.signature, CRT_SIGNATURE, sizeof(header.signature));
    header.header_length = __REV(sizeof(CRT_HEADER));
    header.version = __REV16(CRT_VERSION);
    header.cartridge_type = __REV16(type);
    header.exrom = exrom;
    header.game = game;
    memset(header.reserved, 0, sizeof(header.reserved));

    for (uint8_t i=0; i<sizeof(header.cartridge_name); i++)
    {
        char c = *name;
        if (c)
        {
            name++;
        }

        header.cartridge_name[i] = c;
    }

    uint32_t len = file_write(file, &header, sizeof(CRT_HEADER));
    return len == sizeof(CRT_HEADER);
}

static bool crt_load_chip_header(FIL *file, CRT_CHIP_HEADER *header)
{
    uint32_t len = file_read(file, header, sizeof(CRT_CHIP_HEADER));

    if (len != sizeof(CRT_CHIP_HEADER) ||
        memcmp(CRT_CHIP_SIGNATURE, header->signature, sizeof(header->signature)) != 0)
    {
        return false;
    }

    header->packet_length = __REV(header->packet_length);
    header->chip_type = __REV16(header->chip_type);
    header->bank = __REV16(header->bank);
    header->start_address = __REV16(header->start_address);
    header->image_size = __REV16(header->image_size);

    // Note: packet length > image size + chip header in "Expert Cartridge" CRT file
    if (header->packet_length < (header->image_size + sizeof(CRT_CHIP_HEADER)))
    {
        return false;
    }

    return true;
}

static bool crt_write_chip_header(FIL *file, uint8_t type, uint8_t bank, uint16_t address, uint16_t size)
{
    CRT_CHIP_HEADER header;
    memcpy(header.signature, CRT_CHIP_SIGNATURE, sizeof(header.signature));
    header.packet_length = __REV(size + sizeof(CRT_CHIP_HEADER));
    header.chip_type = __REV16(type);
    header.bank = __REV16(bank);
    header.start_address = __REV16(address);
    header.image_size = __REV16(size);

    uint32_t len = file_write(file, &header, sizeof(CRT_CHIP_HEADER));
    return len == sizeof(CRT_CHIP_HEADER);
}

static int32_t crt_get_offset(CRT_CHIP_HEADER *header, uint16_t cartridge_type)
{
    int32_t offset = -1;

    // ROML bank (and ROMH for >8k images)
    if (header->start_address == 0x8000 && header->image_size <= 16*1024)
    {
        // Suport ROML only cartridges with more than 64 banks
        if(header->image_size <= 8*1024 &&
           cartridge_type == CRT_MAGIC_DESK_DOMARK_HES_AUSTRALIA)
        {
            bool odd_bank = header->bank & 1;
            header->bank >>= 1;
            offset = header->bank * 16*1024;

            // Use ROMH bank location for odd banks
            if (odd_bank)
            {
                offset += 8*1024;
            }
        }
        else
        {
            offset = header->bank * 16*1024;
        }
    }
    // ROMH bank
    else if ((header->start_address == 0xa000 || header->start_address == 0xe000) &&
              header->image_size <= 8*1024)
    {
        offset = header->bank * 16*1024 + 8*1024;
    }
    // ROMH bank (4k Ultimax)
    else if (header->start_address == 0xf000 && header->image_size == 4*1024)
    {
        offset = header->bank * 16*1024 + 12*1024;
    }

    return offset;
}

static uint8_t crt_program_file(FIL *crt_file, uint16_t cartridge_type)
{
    uint8_t *flash_buffer = (uint8_t *)FLASH_BASE;
    uint8_t banks_in_use = 0;
    bool sector_erased[8] = {0};

    memset(dat_buffer, 0xff, sizeof(dat_buffer));

    while (!f_eof(crt_file))
    {
        CRT_CHIP_HEADER header;
        if(!crt_load_chip_header(crt_file, &header))
        {
            err("Failed to read CRT chip header\n");
            banks_in_use = 0;
            break;
        }

        int32_t offset = crt_get_offset(&header, cartridge_type);
        if (offset == -1)
        {
            wrn("Unsupported CRT chip bank %u at $%x. Size %u\n",
                header.bank, header.start_address, header.image_size);
            banks_in_use = 0;
            break;
        }

        // Place image in KungFuFlash.dat file or flash?
        uint8_t *read_buf = header.bank < 4 ? dat_buffer + offset :
                            (uint8_t *)scratch_buf;

        if (file_read(crt_file, read_buf, header.image_size) != header.image_size)
        {
            err("Failed to read CRT chip image. Bank %u at $%x\n",
                header.bank, header.start_address);
            banks_in_use = 0;
            break;
        }

        // Place image in flash
        if (header.bank >= 4 && header.bank < 64)
        {
            uint8_t *flash_ptr = flash_buffer + offset;
            int8_t sector_to_erase = -1;

            uint8_t sector = header.bank / 8;
            if (!sector_erased[sector])
            {
                sector_erased[sector] = true;
                sector_to_erase = sector + 4;
                led_off();
            }

            flash_sector_program(sector_to_erase, flash_ptr, scratch_buf,
                                 header.image_size);
        }
        // Skip image
        else if (header.bank >= 64)
        {
            wrn("No room for CRT chip bank %u at $%x\n",
                header.bank, header.start_address);
            continue;
        }

        if (banks_in_use < (header.bank + 1))
        {
            banks_in_use = header.bank + 1;
        }
    }

    // Erase any gaps to minimize generated CRT file
    for (uint8_t sector=0; sector<banks_in_use/8; sector++)
    {
        if (!sector_erased[sector])
        {
            uint8_t sector_to_erase = sector + 4;
            led_toggle();
            flash_sector_program(sector_to_erase, 0, 0, 0);
        }
    }

    led_on();
    return banks_in_use;
}

static bool crt_write_chip(FIL *file, uint8_t bank, uint16_t address, uint16_t size, void *buf)
{
    if (!crt_write_chip_header(file, CRT_CHIP_FLASH, bank, address, size))
    {
        return false;
    }

    return file_write(file, buf, size) == size;
}

static bool crt_bank_empty(uint8_t *buf, uint16_t size)
{
    uint32_t *buf32 = (uint32_t *)buf;
    for (uint16_t i=0; i<size/4; i++)
    {
        if (buf32[i] != 0xffffffff)
        {
            return false;
        }
    }

    return true;
}

static bool crt_write_file(FIL *crt_file, uint8_t banks)
{
    const uint16_t chip_size = 8*1024;
    for (uint8_t bank=0; bank<banks; bank++)
    {
        uint8_t *buf;
        if (bank < 4)
        {
            buf = dat_buffer + (16*1024 * bank);
        }
        else
        {
            buf = (uint8_t *)FLASH_BASE + (16*1024 * bank);
        }

        if (!crt_bank_empty(buf, chip_size) &&
            !crt_write_chip(crt_file, bank, 0x8000, chip_size, buf))
        {
            return false;
        }

        if ((!bank || !crt_bank_empty(buf + chip_size, chip_size)) &&
            !crt_write_chip(crt_file, bank, 0xa000, chip_size, buf + chip_size))
        {
            return false;
        }
    }

    return true;
}

static uint32_t crt_calc_flash_crc(uint8_t crt_banks)
{
    crc_reset();
    if (crt_banks > 4)
    {
        uint8_t *flash_buffer = (uint8_t *)FLASH_BASE + 16*1024 * 4;
        size_t flash_used = (crt_banks-4) * 16*1024;
        crc_calc(flash_buffer, flash_used);
    }

    return crc_get();
}

static bool upd_load(FIL *file, char *firmware_name)
{
    uint32_t len = file_read(file, dat_buffer, sizeof(dat_buffer));
    dbg("Loaded UPD size: %u\n", len);

    if (len == sizeof(dat_buffer))
    {
        const uint8_t *firmware_ver = &dat_buffer[48*1024];
        convert_to_ascii(firmware_name, firmware_ver, FW_NAME_SIZE);

        if (memcmp(firmware_name, "Kung Fu Flash", 13) == 0)
        {
            // Don't allow downgrade to a PAL only version on a NTSC C64
            if (c64_is_ntsc() && firmware_name[14] == 'v')
            {
                return false;
            }
            return true;
        }
    }

    return false;
}

static void upd_program(void)
{
    for (int8_t sector=0; sector<4; sector++)
    {
        led_toggle();
        uint32_t offset = 16*1024 * sector;
        flash_sector_program(sector, (uint8_t *)FLASH_BASE + offset,
                             dat_buffer + offset, 16*1024);
    }
}

static bool mount_sd_card(void)
{
    if (!filesystem_mount())
    {
        return false;
    }

    log("SD Card successfully mounted\n");
    return dir_change("/");
}

static bool load_dat(void)
{
    bool result = true;
    FIL file;
    if (!file_open(&file, DAT_FILENAME, FA_READ) ||
        file_read(&file, &dat_file, sizeof(dat_file)) != sizeof(dat_file) ||
        file_read(&file, dat_buffer, sizeof(dat_buffer)) != sizeof(dat_buffer) ||
        memcmp(DAT_SIGNATURE, dat_file.signature, sizeof(dat_file.signature)) != 0)
    {
        wrn(DAT_FILENAME " file not found or invalid\n");
        memset(&dat_file, 0, sizeof(dat_file));
        memcpy(dat_file.signature, DAT_SIGNATURE, sizeof(dat_file.signature));
        result = false;
    }

    file_close(&file);
    return result;
}

static bool auto_boot(void)
{
    load_dat();
    if (menu_signature() || menu_button())
    {
        menu_button_wait_release();
        invalidate_menu_signature();
        return false;
    }

    return true;
}

static bool save_dat(void)
{
    dbg("Saving " DAT_FILENAME " file\n");

    FIL file;
    if (!file_open(&file, DAT_FILENAME, FA_WRITE|FA_CREATE_ALWAYS))
    {
        wrn("Could not open " DAT_FILENAME " for writing\n");
        return false;
    }

    bool file_saved = false;
    if (file_write(&file, &dat_file, sizeof(dat_file)) == sizeof(dat_file) &&
        file_write(&file, &dat_buffer, sizeof(dat_buffer)) == sizeof(dat_buffer))
    {
        file_saved = true;
    }

    file_close(&file);
    return file_saved;
}

static inline bool persist_basic_selection(void)
{
    return (dat_file.flags & DAT_FLAG_PERSIST_BASIC) != 0;
}

static inline bool autostart_d64(void)
{
    return (dat_file.flags & DAT_FLAG_AUTOSTART_D64) != 0;
}

static uint8_t get_device_number(uint8_t flags)
{
    uint8_t offset = flags & DAT_FLAG_DEVICE_D64_MSK;
    return (offset >> DAT_FLAG_DEVICE_D64_POS) + 8;
}

static void set_device_number(uint8_t *flags, uint8_t device)
{
    uint8_t offset = ((device - 8) << DAT_FLAG_DEVICE_D64_POS) &
                     DAT_FLAG_DEVICE_D64_MSK;

    MODIFY_REG(*flags, DAT_FLAG_DEVICE_D64_MSK, offset);
}

static inline uint8_t device_number_d64(void)
{
    return get_device_number(dat_file.flags);
}

static void basic_load(const char *filename)
{
    uint8_t device = device_number_d64();

    // BASIC commands to run at start-up
    sprint((char *)dat_buffer, "%cLOAD\"%s\",%d,1%cRUN%c", device,
           filename, device, 0, 0);
}

static void basic_no_commands(void)
{
    // No BASIC commands at start-up
    sprint((char *)dat_buffer, "%c%c", device_number_d64(), 0);
}

static void basic_loading(const char *filename)
{
    // Setup string to print at BASIC start-up
    char *dest = (char *)crt_ram_buf + LOADING_OFFSET;
    dest = convert_to_screen_code(dest, "LOADING ");
    dest = convert_to_screen_code(dest, filename);
    *dest = 0;
}

static bool chdir_last(void)
{
    bool res = false;

    // Change to last selected dir if any
    if (dat_file.path[0])
    {
        res = dir_change(dat_file.path);
        if (!res)
        {
            dat_file.path[0] = 0;
            dat_file.file[0] = 0;
        }
    }
    else
    {
        dat_file.file[0] = 0;
    }

    return res;
}

static void sanitize_sd_filename(char *dest, const char *src, uint8_t size)
{
    for(uint8_t i=0; i<size && *src; i++)
    {
        char c = sanitize_char(*src++);
        *dest++ = ff_wtoupper(c);
    }

    *dest = 0;
}

static void send_prg(void)
{
    const char *name;
    if (dat_file.prg.name[0])
    {
        name = dat_file.prg.name;
    }
    else
    {
        // Limit filename to one line on the screen
        sanitize_sd_filename(scratch_buf, dat_file.file, 32);
        name = scratch_buf;
    }

    basic_loading(name);
    if(!c64_send_prg(dat_buffer, dat_file.prg.size))
    {
        system_restart();
    }
}

static bool load_d64(void)
{
    if (!chdir_last())
    {
        return false;
    }

    if (!d64_open(&d64_state.image, dat_file.file))
    {
        return false;
    }

    return true;
}

static void c64_launcher_mode(void)
{
    crt_ptr = CRT_LAUNCHER_BANK;
    crt_install_handler(CRT_EASYFLASH, CRT_FLAG_NONE);
}

static bool c64_set_mode(void)
{
    bool result = false;

    switch (dat_file.boot_type)
    {
        case DAT_PRG:
        {
            result = prg_size_valid(dat_file.prg.size);
            if (result)
            {
                c64_enable();
                send_prg();
            }
        }
        break;

        case DAT_CRT:
        {
            if (!c64_fw_supports_crt())
            {
                break;
            }

            if (!crt_is_supported(dat_file.crt.type))
            {
                break;
            }

            if (!dat_file.crt.banks)
            {
                break;
            }

            uint32_t flash_hash = crt_calc_flash_crc(dat_file.crt.banks);
            if (flash_hash != dat_file.crt.flash_hash &&
                !(dat_file.crt.flags & CRT_FLAG_UPDATED))
            {
                break;
            }

            if (!c64_is_reset())
            {
                // Disable VIC-II output if C64 has been started (needed for FC3)
                c64_interface(true);
                c64_send_wait_for_reset();
                c64_disable();
            }

            crt_ptr = crt_banks[0];

            uint32_t state = STATUS_LED_ON;
            if (dat_file.crt.exrom)
            {
                state |= C64_EXROM_HIGH;
            }
            else
            {
                state |= C64_EXROM_LOW;
            }

            if (dat_file.crt.game)
            {
                state |= C64_GAME_HIGH;
            }
            else
            {
                state |= C64_GAME_LOW;
            }

            c64_crt_control(state);
            // Try prevent triggering bug in H.E.R.O. No effect at power-on though
            c64_sync_with_vic();
            crt_install_handler(dat_file.crt.type, dat_file.crt.flags);
            c64_enable();
            result = true;
        }
        break;

        case DAT_USB:
        {
            c64_disable();
            ef_init();
            c64_enable();

            basic_loading("FROM USB");
            c64_send_message("Communicating with USB...");
            result = true;
        }
        break;

        case DAT_DISK:
        {
            if (!load_d64())
            {
                break;
            }

            c64_disable();
            ef_init();

            // Copy Launcher to memory to allow bank switching in EasyFlash emulation
            // BASIC commands to run are placed at the start of flash ($8000)
            uint32_t offset = BASIC_CMD_BUF_SIZE;
            memcpy(crt_banks[0] + offset, CRT_LAUNCHER_BANK + offset, 16*1024 - offset);
            crt_ptr = crt_banks[0];

            c64_enable();
            if (!c64_send_mount_disk())
            {
                system_restart();
            }
            result = true;
        }
        break;

        case DAT_BASIC:
        {
            c64_disable();
            // Unstoppable reset! - https://www.c64-wiki.com/wiki/Reset_Button
            c64_crt_control(STATUS_LED_ON|CRT_PORT_8K);
            c64_enable();
            delay_ms(300);
            c64_crt_control(CRT_PORT_NONE);
            result = true;
        }
        break;

        case DAT_KILL:
	{
		uint16_t expansion;

		if((dat_file.flags&DAT_FLAG_MEMEXPANSION_MSK)==0)	// no expansion is selected
		{
		    c64_disable();
		    // Also unstoppable!
		    c64_crt_control(STATUS_LED_OFF|CRT_PORT_8K);
		    c64_reset(false);
		    delay_ms(300);
		    c64_crt_control(CRT_PORT_NONE);
		    result = true;
		}
		else
		{
		    if (!c64_is_reset())
		    {
		        // Disable VIC-II output if C64 has been started (needed for FC3)
		        c64_interface(true);
		        c64_send_wait_for_reset();
		        c64_disable();
		    }

		    expansion=0xFFF0+((dat_file.flags&DAT_FLAG_MEMEXPANSION_MSK)>>DAT_FLAG_MEMEXPANSION_POS);
		    //expansion=0xFFF0+((dat_file.flags&0x30)>>4);

		    c64_crt_control(STATUS_LED_ON|CRT_PORT_NONE);
		    // Try prevent triggering bug in H.E.R.O. No effect at power-on though
		    c64_sync_with_vic();
		    crt_install_handler(expansion, CRT_FLAG_NONE);
		    c64_enable();
		    result = true;
		}
	}
	break;

        case DAT_KILL_C128:
        {
            c64_disable();
            c64_crt_control(STATUS_LED_OFF|CRT_PORT_NONE);
            c64_reset(false);
            result = true;
        }
        break;
    }

    return result;
}
