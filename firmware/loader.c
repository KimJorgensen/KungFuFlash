/*
 * Copyright (c) 2019-2023 Kim Jørgensen
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
#define UPD_FILENAME ("/KungFuFlash_v" VERSION ".upd")

#define CRT_C64_SIGNATURE  "C64 CARTRIDGE   "
#define CRT_C128_SIGNATURE "C128 CARTRIDGE  "
#define CRT_CHIP_SIGNATURE "CHIP"
#define CRT_VERSION_1_0 0x100
#define CRT_VERSION_2_0 0x200

#define EAPI_OFFSET 0x3800
#define EAPI_SIZE   0x300

static bool prg_load_file(FIL *file)
{
    u16 len = file_read(file, dat_buffer, sizeof(dat_buffer));
    dbg("Loaded PRG size: %u", len);

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
    u16 len = file_read(file, &header, sizeof(P00_HEADER));

    if (len != sizeof(P00_HEADER) ||
        memcmp("C64File", header.signature, sizeof(header.signature)) != 0)
    {
        wrn("Unsupported P00 header");
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

static u16 rom_load_file(FIL *file)
{
    memset(dat_buffer, 0xff, sizeof(dat_buffer));

    u16 len = file_read(file, dat_buffer, sizeof(dat_buffer));
    return len;
}

static u16 txt_load_file(FIL *file)
{
    memset(dat_buffer, 0x00, sizeof(dat_buffer));

    u16 len = file_read(file, &dat_buffer[DIR_NAME_LENGTH+2],
                        sizeof(dat_buffer) - (DIR_NAME_LENGTH+3));
    return len;
}

static bool crt_load_header(FIL *file, CRT_HEADER *header)
{
    u32 len = file_read(file, header, sizeof(CRT_HEADER));

    if (len != sizeof(CRT_HEADER))
    {
        wrn("Unsupported CRT header");
        return false;
    }

    u16 crt_type_flag = 0;
    if (memcmp(CRT_C128_SIGNATURE, header->signature,
               sizeof(header->signature)) == 0)
    {
        crt_type_flag = CRT_C128_CARTRIDGE;
    }
    else if (memcmp(CRT_C64_SIGNATURE, header->signature,
                    sizeof(header->signature)) != 0)
    {
        wrn("Unsupported CRT signature");
        return false;
    }

    header->header_length = __REV(header->header_length);
    header->version = __REV16(header->version);
    header->cartridge_type = __REV16(header->cartridge_type) | crt_type_flag;

    if (header->header_length != sizeof(CRT_HEADER))
    {
        if (header->header_length != 0x20)
        {
            wrn("Unsupported CRT header length: %u", header->header_length);
            return false;
        }
        else
        {
            log("Ignoring non-standard CRT header length: %u",
                header->header_length);
        }
    }

    if (header->version < CRT_VERSION_1_0 || header->version > CRT_VERSION_2_0)
    {
        wrn("Unsupported CRT version: %x", header->version);
        return false;
    }

    return true;
}

static bool crt_write_header(FIL *file, u16 type, u8 exrom, u8 game, const char *name)
{
    CRT_HEADER header;
    memcpy(header.signature, CRT_C64_SIGNATURE, sizeof(header.signature));
    header.header_length = __REV(sizeof(CRT_HEADER));
    header.version = __REV16(CRT_VERSION_1_0);
    header.cartridge_type = __REV16(type);
    header.exrom = exrom;
    header.game = game;
    header.hardware_revision = 0;
    memset(header.reserved, 0, sizeof(header.reserved));

    for (u8 i=0; i<sizeof(header.cartridge_name); i++)
    {
        char c = *name;
        if (c)
        {
            name++;
        }

        header.cartridge_name[i] = c;
    }

    u32 len = file_write(file, &header, sizeof(CRT_HEADER));
    return len == sizeof(CRT_HEADER);
}

static bool crt_load_chip_header(FIL *file, CRT_CHIP_HEADER *header)
{
    u32 len = file_read(file, header, sizeof(CRT_CHIP_HEADER));

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

static bool crt_write_chip_header(FIL *file, u8 type, u8 bank, u16 address, u16 size)
{
    CRT_CHIP_HEADER header;
    memcpy(header.signature, CRT_CHIP_SIGNATURE, sizeof(header.signature));
    header.packet_length = __REV(size + sizeof(CRT_CHIP_HEADER));
    header.chip_type = __REV16(type);
    header.bank = __REV16(bank);
    header.start_address = __REV16(address);
    header.image_size = __REV16(size);

    u32 len = file_write(file, &header, sizeof(CRT_CHIP_HEADER));
    return len == sizeof(CRT_CHIP_HEADER);
}

static s32 crt_get_offset(CRT_CHIP_HEADER *header, u16 cartridge_type)
{
    s32 offset = -1;

    // ROML bank (and ROMH for >8k images)
    if (header->start_address == 0x8000 && header->image_size <= 16*1024)
    {
        // Suport ROML only cartridges with more than 64 banks
        if (header->image_size <= 8*1024 &&
            (cartridge_type == CRT_FUN_PLAY_POWER_PLAY ||
             cartridge_type == CRT_MAGIC_DESK_DOMARK_HES_AUSTRALIA))
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
            if (cartridge_type & CRT_C128_CARTRIDGE)
            {
                header->bank *= 2;
            }
            offset = header->bank * 16*1024;
        }
    }
    // ROMH bank
    else if ((header->start_address == 0xa000 || header->start_address == 0xe000) &&
              header->image_size <= 8*1024)
    {
        offset = header->bank * 16*1024 + 8*1024;
    }
    // ROMH bank (C128)
    else if (header->start_address == 0xc000 && header->image_size <= 16*1024)
    {
        header->bank = (header->bank * 2) + 1;
        offset = header->bank * 16*1024;
    }
    // ROMH bank (4k Ultimax)
    else if (header->start_address == 0xf000 && header->image_size == 4*1024)
    {
        offset = header->bank * 16*1024 + 12*1024;
    }

    return offset;
}

static u8 crt_program_file(FIL *crt_file, u16 cartridge_type)
{
    u8 *flash_buffer = (u8 *)FLASH_BASE;
    u8 banks_in_use = 0;
    bool sector_erased[8] = {0};

    memset(dat_buffer, 0xff, sizeof(dat_buffer));

    while (!f_eof(crt_file))
    {
        CRT_CHIP_HEADER header;
        if (!crt_load_chip_header(crt_file, &header))
        {
            err("Failed to read CRT chip header");
            banks_in_use = 0;
            break;
        }

        s32 offset = crt_get_offset(&header, cartridge_type);
        if (offset == -1)
        {
            wrn("Unsupported CRT chip bank %u at $%x. Size %u",
                header.bank, header.start_address, header.image_size);
            banks_in_use = 0;
            break;
        }

        // Place image in KungFuFlash.dat file or flash?
        u8 *read_buf = header.bank < 4 ? dat_buffer + offset :
                       (u8 *)scratch_buf;

        if (file_read(crt_file, read_buf, header.image_size) != header.image_size)
        {
            err("Failed to read CRT chip image. Bank %u at $%x",
                header.bank, header.start_address);
            banks_in_use = 0;
            break;
        }

        // Place image in flash
        if (header.bank >= 4 && header.bank < 64)
        {
            u8 *flash_ptr = flash_buffer + offset;
            s8 sector_to_erase = -1;

            u8 sector = header.bank / 8;
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
            wrn("No room for CRT chip bank %u at $%x",
                header.bank, header.start_address);
            continue;
        }

        if (banks_in_use < (header.bank + 1))
        {
            banks_in_use = header.bank + 1;
        }
    }

    // Erase any gaps to minimize generated CRT file
    for (u8 sector=0; sector<banks_in_use/8; sector++)
    {
        if (!sector_erased[sector])
        {
            u8 sector_to_erase = sector + 4;
            led_toggle();
            flash_sector_program(sector_to_erase, 0, 0, 0);
        }
    }

    led_on();
    return banks_in_use;
}

static bool crt_write_chip(FIL *file, u8 bank, u16 address, u16 size, void *buf)
{
    if (!crt_write_chip_header(file, CRT_CHIP_FLASH, bank, address, size))
    {
        return false;
    }

    return file_write(file, buf, size) == size;
}

static bool crt_bank_empty(u8 *buf, u16 size)
{
    u32 *buf32 = (u32 *)buf;
    for (u16 i=0; i<size/4; i++)
    {
        if (buf32[i] != 0xffffffff)
        {
            return false;
        }
    }

    return true;
}

static bool crt_write_file(FIL *crt_file, u8 banks)
{
    const u16 chip_size = 8*1024;
    for (u8 bank=0; bank<banks; bank++)
    {
        u8 *buf;
        if (bank < 4)
        {
            buf = dat_buffer + (16*1024 * bank);
        }
        else
        {
            buf = (u8 *)FLASH_BASE + (16*1024 * bank);
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

static u32 crt_calc_flash_crc(u8 crt_banks)
{
    crc_reset();
    if (crt_banks > 4)
    {
        u8 *flash_buffer = (u8 *)FLASH_BASE + 16*1024 * 4;
        size_t flash_used = (crt_banks-4) * 16*1024;
        crc_calc(flash_buffer, flash_used);
    }

    return crc_get();
}

static bool upd_load(FIL *file, char *firmware_name)
{
    u32 len = file_read(file, dat_buffer, sizeof(dat_buffer));
    dbg("Loaded UPD size: %u", len);

    if (len == sizeof(dat_buffer))
    {
        const u8 *firmware_ver = &dat_buffer[FIRMWARE_SIZE];
        convert_to_ascii(firmware_name, firmware_ver, FW_NAME_SIZE);

        if (memcmp(firmware_name, "Kung Fu Flash", 13) == 0)
        {
            // Don't allow downgrade to a PAL only version on a NTSC C64
            if (c64_is_ntsc() && memcmp(&firmware_name[13], " v1.", 4) == 0 &&
                (firmware_name[17] == '0' || firmware_name[17] == '1'))
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
    for (s8 sector=0; sector<4; sector++)
    {
        led_toggle();
        u32 offset = 16*1024 * sector;
        flash_sector_program(sector, (u8 *)FLASH_BASE + offset,
                             dat_buffer + offset, 16*1024);
    }
}

static bool mount_sd_card(void)
{
    if (!filesystem_mount())
    {
        return false;
    }

    log("SD Card successfully mounted");
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
        wrn(DAT_FILENAME " file not found or invalid");
        memset(&dat_file, 0, sizeof(dat_file));
        memcpy(dat_file.signature, DAT_SIGNATURE, sizeof(dat_file.signature));
        result = false;
    }

    file_close(&file);
    return result;
}

static bool auto_boot(void)
{
    bool result = false;

    load_dat();
    if (menu_signature() || menu_button_pressed())
    {
        invalidate_menu_signature();

        u32 i = 0;
        while (menu_button_pressed())
        {
            // Menu button long press will start diagnostic
            if (++i > 100)
            {
                dat_file.boot_type = DAT_DIAG;
                result = true;
                break;
            }
        }
    }
    else
    {
        result = true;
    }

    if (dat_file.boot_type != DAT_DIAG)
    {
        menu_button_enable();
    }

    return result;
}

static bool save_dat(void)
{
    dbg("Saving " DAT_FILENAME " file");

    FIL file;
    if (!file_open(&file, DAT_FILENAME, FA_WRITE|FA_CREATE_ALWAYS))
    {
        wrn("Could not open " DAT_FILENAME " for writing");
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

static u8 get_device_number(u8 flags)
{
    u8 offset = flags & DAT_FLAG_DEVICE_D64_MSK;
    return (offset >> DAT_FLAG_DEVICE_D64_POS) + 8;
}

static void set_device_number(u8 *flags, u8 device)
{
    u8 offset = ((device - 8) << DAT_FLAG_DEVICE_D64_POS) &
                DAT_FLAG_DEVICE_D64_MSK;

    MODIFY_REG(*flags, DAT_FLAG_DEVICE_D64_MSK, offset);
}

static inline u8 device_number_d64(void)
{
    return get_device_number(dat_file.flags);
}

static char * basic_get_filename(FILINFO *file_info)
{
    char *filename = file_info->fname;
    bool comma_found = false;

    size_t len;
    for (len=0; len<=16 && filename[len]; len++)
    {
        char c = ff_wtoupper(filename[len]);
        filename[len] = c;

        if (c == ',')
        {
            comma_found = true;
        }
    }

    if ((len > 16 || comma_found) && file_info->altname[0])
    {
        filename = file_info->altname;
    }

    return filename;
}

static void basic_load(const char *filename)
{
    u8 device = device_number_d64();

    // BASIC commands to run at start-up
    sprint((char *)dat_buffer, "%cLOAD\"%s\",%u,1%cRUN\r%c", device,
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
    char *dest = (char *)CRT_RAM_BUF + LOADING_OFFSET;
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

static void sanitize_sd_filename(char *dest, const char *src, u8 size)
{
    for (u8 i=0; i<size && *src; i++)
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
    if (!c64_send_prg(dat_buffer, dat_file.prg.size))
    {
        system_restart();
    }
}

static bool load_disk(void)
{
    if (!chdir_last())
    {
        return false;
    }

    if (dat_file.disk.mode == DISK_MODE_D64)
    {
        if (!d64_open(&d64_state.image, dat_file.file))
        {
            return false;
        }
    }
    else if (dat_file.file[0])  // DISK_MODE_FS
    {
        FILINFO file_info;
        if (!file_stat(dat_file.file, &file_info))
        {
            return false;
        }

        u8 file_type = get_file_type(&file_info);
        if (file_type == FILE_DIR && !dir_change(dat_file.file))
        {
            return false;
        }
    }

    return true;
}

static void start_text_reader(void)
{
    format_path(scratch_buf, true);
    c64_send_data(scratch_buf, DIR_NAME_LENGTH);
    c64_send_text_reader((char *)&dat_buffer[DIR_NAME_LENGTH+2],
                         sizeof(dat_buffer) - (DIR_NAME_LENGTH+3));
}

static void c64_launcher_mode(void)
{
    crt_ptr = CRT_LAUNCHER_BANK;
    kff_init();
    C64_INSTALL_HANDLER(kff_handler);
}

static void c64_ef3_mode_disable(void)
{
    c64_disable();
    ef_init();
    C64_INSTALL_HANDLER(ef3_handler);
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
                c64_ef3_mode_disable();
                c64_enable();
                send_prg();
            }
        }
        break;

        case DAT_CRT:
        {
            if (!crt_is_supported(dat_file.crt.type))
            {
                break;
            }

            if (!dat_file.crt.banks)
            {
                break;
            }

            u32 flash_hash = crt_calc_flash_crc(dat_file.crt.banks);
            if (flash_hash != dat_file.crt.flash_hash &&
                !(dat_file.crt.flags & CRT_FLAG_UPDATED))
            {
                break;
            }

            if (!c64_is_reset())
            {
                // Disable VIC-II output if C64 has been started (needed for FC3)
                c64_interface_sync();
                c64_send_command(CMD_WAIT_RESET);
                c64_disable();
            }

            crt_install_handler(&dat_file.crt);
            // Try prevent triggering bug in H.E.R.O. No effect at power-on though
            c64_sync_with_vic();
            c64_enable();
            result = true;
        }
        break;

        case DAT_USB:
        {
            c64_ef3_mode_disable();
            c64_enable();

            basic_loading("FROM USB");
            result = true;
        }
        break;

        case DAT_DISK:
        {
            if (!load_disk())
            {
                break;
            }

            c64_disable();
            kff_init();
            c64_enable();
            result = true;
        }
        break;

        case DAT_TXT:
        {
            c64_disable();
            kff_init();
            c64_enable();
            result = true;
        }
        break;

        case DAT_BASIC:
        {
            c64_ef3_mode_disable();
            // Unstoppable reset! - https://www.c64-wiki.com/wiki/Reset_Button
            C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_8K);
            c64_enable();
            delay_ms(300);
            C64_CRT_CONTROL(CRT_PORT_NONE);
            result = true;
        }
        break;

        case DAT_KILL:
        {
            c64_disable();
            // Also unstoppable!
            C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_8K);
            c64_reset(false);
            delay_ms(300);
            C64_CRT_CONTROL(CRT_PORT_NONE);
            result = true;
        }
        break;

        case DAT_KILL_C128:
        {
            c64_disable();
            C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_NONE);
            c64_reset(false);
            result = true;
        }
        break;

        case DAT_DIAG:
        {
            c64_disable();
            kff_init();
            result = true;
        }
    }

    return result;
}
