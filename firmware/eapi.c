/*
 * Copyright (c) 2019-2022 Kim Jørgensen
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

static void eapi_open_dat(FIL *file)
{
    if (!file_open(file, DAT_FILENAME, FA_READ|FA_WRITE)
        || f_size(file) != (sizeof(dat_file) + sizeof(dat_buffer)))
    {
        err(DAT_FILENAME " file not found or invalid");
        restart_to_menu();
    }
}

static void eapi_save_header(FIL *file)
{
    if (!file_seek(file, 0) ||
        file_write(file, &dat_file, sizeof(dat_file)) != sizeof(dat_file) ||
        !file_sync(file))
    {
        err("Failed to write dat header");
        restart_to_menu();
    }
}

static void eapi_load_buffer(FIL *file)
{
    if (!file_seek(file, sizeof(dat_file)) ||
        file_read(file, &dat_buffer, sizeof(dat_buffer)) != sizeof(dat_buffer))
    {
        err("Failed to load dat buffer");
        restart_to_menu();
    }
}

static void eapi_save_buffer(FIL *file)
{
    if (!file_seek(file, sizeof(dat_file)) ||
        file_write(file, &dat_buffer, sizeof(dat_buffer)) != sizeof(dat_buffer) ||
        !file_sync(file))
    {
        err("Failed to save dat buffer");
        restart_to_menu();
    }
}

static void eapi_save_buffer_byte(FIL *file, u16 pos, u8 byte, bool sync)
{
    if (!file_seek(file, sizeof(dat_file) + pos) ||
        file_write(file, &byte, 1) != 1 ||
        (sync && !file_sync(file)))
    {
        err("Failed to save dat buffer byte");
        restart_to_menu();
    }
}

static void eapi_disable_interface(void)
{
    ef3_send_reply(REPLY_WRITE_WAIT);
    ef3_receive_byte(); // Wait for C64
    c64_interface(false);
}

static void eapi_enable_interface(void)
{
    c64_interface(true);
    ef3_send_data("DONE", 4);
}

static void eapi_handle_write_flash(FIL *file, u16 addr, u8 value)
{
    u8 *dest = crt_ptr + (addr & 0x3fff);
    if (crt_ptr >= crt_banks[0] && crt_ptr <= crt_banks[3])
    {
        *dest &= value;
        u16 pos = (u16)(dest - crt_banks[0]);

        eapi_disable_interface();
        if (!(dat_file.crt.flags & CRT_FLAG_UPDATED))
        {
            dat_file.crt.flags |= CRT_FLAG_UPDATED;
            eapi_save_header(file);
        }

        // Only sync to file at end of bank low/high.
        // This is much faster but doesn't allow random access
        bool sync = (addr & 0x1fff) == 0x1fff;
        eapi_save_buffer_byte(file, pos, *dest, sync);
        eapi_enable_interface();
    }
    else
    {
        if (!(dat_file.crt.flags & CRT_FLAG_UPDATED))
        {
            dat_file.crt.flags |= CRT_FLAG_UPDATED;
            eapi_disable_interface();
            eapi_save_header(file);
            eapi_enable_interface();
        }

        flash_program_byte(dest, value);
    }

    u8 result = REPLY_EAPI_OK;
    if (*dest != value)
    {
        wrn("Flash write failed at $%04x (%x)", addr, crt_ptr);
        result = REPLY_WRITE_ERROR;
    }

    ef3_send_reply(result);
}

static void eapi_handle_erase_sector(FIL *file, u8 bank, u16 addr)
{
    if (bank >= 64 || (bank % 8))
    {
        wrn("Got invalid sector to erase: bank %u", bank);
        ef3_send_reply(REPLY_WRITE_ERROR);
        return;
    }

    u8 sector = bank / 8;
    s8 sector_to_erase = sector + 4;
    u16 offset = (addr & 0xff00) == 0x8000 ? 0 : 8*1024;
    u16 other_offset = offset ? 0 : 8*1024;

    eapi_disable_interface();
    if (!(dat_file.crt.flags & CRT_FLAG_UPDATED) ||
        (bank + 8) > dat_file.crt.banks)
    {
        dat_file.crt.flags |= CRT_FLAG_UPDATED;
        if ((bank + 8) > dat_file.crt.banks)
        {
            dat_file.crt.banks = (bank + 8);
        }

        eapi_save_header(file);
    }

    if (!sector)
    {
        // Backup other 32k of 64k flash sector in dat_buffer
        for (u8 i=0; i<4; i++)
        {
            memcpy(crt_banks[i] + offset,
                   crt_banks[i + 4] + other_offset, 8*1024);
        }

        // Erase 64k flash sector, restore other 32k and
        // erase 32k dat_buffer sector
        for (u8 i=0; i<4; i++)
        {
            flash_sector_program(sector_to_erase,
                                 crt_banks[i + 4] + other_offset,
                                 crt_banks[i] + offset, 8*1024);
            sector_to_erase = -1;

            memset(crt_banks[i] + offset, 0xff, 8*1024);
        }

        eapi_save_buffer(file);
    }
    else
    {
        // Backup other 64k of 128k flash sector in dat_buffer
        for (u8 i=0; i<8; i++)
        {
            memcpy(dat_buffer + (i * 8*1024),
                   crt_banks[bank + i] + other_offset, 8*1024);
        }

        // Erase 128k sector and restore other 64k
        for (u8 i=0; i<8; i++)
        {
            flash_sector_program(sector_to_erase,
                                 crt_banks[bank + i] + other_offset,
                                 dat_buffer + (i * 8*1024), 8*1024);
            sector_to_erase = -1;
        }

        // Restore dat_buffer
        eapi_load_buffer(file);
    }

    eapi_enable_interface();
    ef3_send_reply(REPLY_EAPI_OK);
}

static void eapi_loop(void)
{
    FIL file;
    u16 addr;
    u8 value;

    eapi_open_dat(&file);
    while (true)
    {
        u8 command = ef3_receive_command();
        switch (command)
        {
            case CMD_EAPI_NONE:
            break;

            case CMD_EAPI_INIT:
            {
                dbg("Got EAPI_INIT command");
                ef3_send_reply(REPLY_EAPI_OK);
            }
            break;

            case CMD_WRITE_FLASH:
            {
                ef3_receive_data(&addr, 2);
                value = ef3_receive_byte();
                eapi_handle_write_flash(&file, addr, value);
            }
            break;

            case CMD_ERASE_SECTOR:
            {
                ef3_receive_data(&addr, 2);
                value = ef3_receive_byte();
                dbg("Got ERASE_SECTOR command (%u:$%04x)", value, addr);
                eapi_handle_erase_sector(&file, value, addr);
            }
            break;

            default:
            {
                wrn("Got unknown EAPI command: %x", command);
                ef3_send_reply(REPLY_EAPI_OK);
            }
            break;
        }
    }
}
