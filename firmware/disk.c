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

static inline void put_u8(uint8_t **ptr, uint8_t value)
{
    *(*ptr)++ = value;
}

static inline void put_u16(uint8_t **ptr, uint16_t value)
{
    COMPILER_BARRIER(); // Workaround for compiler bug
    *(*((uint16_t **)ptr))++ = value;
}

static void put_u8_times(uint8_t **ptr, uint8_t value, size_t size)
{
    while (size--)
    {
        put_u8(ptr, value);
    }
}

static void put_string(uint8_t **ptr, const char* str)
{
    while (*str)
    {
        put_u8(ptr, *str++);
    }
}

static void put_diskname(uint8_t **ptr, char* str)
{
    put_u8(ptr, '"');
    str[16] = '"';
    str[17] = ' ';

    for (uint8_t i=0; i<23; i++)
    {
        char c = *str++;
        if (c == 0xa0)
        {
            c = (i != 22) ? ' ' : '1';
        }

        put_u8(ptr, c);
    }
}

static void put_filename(uint8_t **ptr, const char* str)
{
    put_u8(ptr, '"');
    char end_char = '"';

    for (uint8_t i=0; i<16; i++)
    {
        char c = *str++;
        if (end_char != '"')
        {
            // Allow characters after the end-quote, like the 1541
            c &= 0x7f;
        }
        else if (c == 0xa0 || c == '"')
        {
            end_char = ' ';
            c = '"';
        }

        put_u8(ptr, c);
    }

    put_u8(ptr, end_char);
}

static void put_dir_entry(uint8_t **ptr, D64_DIR_ENTRY *entry)
{
    put_u16(ptr, entry->blocks);

    uint8_t pad = 0;
    if (entry->blocks < 10)
    {
        pad = 3;
    }
    else if (entry->blocks < 100)
    {
        pad = 2;
    }
    else if (entry->blocks < 1000)
    {
        pad = 1;
    }
    put_u8_times(ptr, ' ', pad);
    put_filename(ptr, entry->filename);

    char c = (entry->type & 0x80) ? ' ' : '*';
    put_u8(ptr, c);
    put_string(ptr, d64_types[entry->type & 7]);
    c = (entry->type & 0x40) ? '<' : ' ';
    put_u8(ptr, c);

    put_u8_times(ptr, ' ', 4 - pad);
}

static bool disk_filename_match(D64_DIR_ENTRY *entry, const char *filename)
{
    bool found = true;
    for (uint8_t i=0; i<16; i++)
    {
        char f = filename[i];
        char e = entry->filename[i];

        if (f == '*')
        {
            break;
        }
        else if (f == '?' && e)
        {
            continue;
        }
        else if (!f && e == 0xa0)
        {
            break;
        }
        else if (f == e)
        {
            if (!f)
            {
                break;
            }
        }
        else
        {
            found = false;
            break;
        }
    }

    return found;
}

static uint16_t disk_create_dir_prg(D64 *d64, uint8_t **ptr, const char *filename)
{
    uint16_t start_addr = 0x0401;       // Start of BASIC (for the PET)
    const uint16_t link_addr = 0x0101;

    if (!d64_read_bam(d64))
    {
        return start_addr;
    }

    put_u16(ptr, link_addr);
    put_u16(ptr, 0);                    // drive number
    put_u8(ptr, 0x12);                  // reverse on
    put_diskname(ptr, d64->diskname);
    put_u8(ptr, 0);                     // end of line

    uint16_t blocks_free = d64_blocks_free(d64);

    while (d64_read_dir(d64))
    {
        if (disk_filename_match(d64->entry, filename))
        {
            put_u16(ptr, link_addr);
            put_dir_entry(ptr, d64->entry);
            put_u8(ptr, 0);                 // end of line
        }
    }

    put_u16(ptr, link_addr);
    put_u16(ptr, blocks_free);
    put_string(ptr, "BLOCKS FREE.");
    put_u8_times(ptr, ' ', 13);
    put_u8(ptr, 0);                     // end of line
    put_u16(ptr, 0);                    // end of program

    return start_addr;
}

static bool disk_find_prg(D64 *d64, const char *filename)
{
    d64_rewind_dir(d64);
    while (d64_read_dir(d64))
    {
        if ((d64->entry->type & 7) != D64_FILE_PRG)
        {
            continue;
        }

        if (disk_filename_match(d64->entry, filename))
        {
            return true;
        }
    }

    return false;
}

static void disk_send_prg(uint16_t prg_size)
{
    c64_send_reply(REPLY_READ_DONE);
    c64_send_data(&prg_size, 2);
}

static void disk_send_prg_bank(uint16_t bank_size)
{
    c64_send_reply(REPLY_READ_BANK);
    c64_send_data(&bank_size, 2);
}

static void disk_handle_load_next_bank(D64 *d64, uint8_t *ptr)
{
    uint16_t prg_size = 0;
    uint8_t sector_len;

    while ((prg_size + D64_SECTOR_DATA_LEN) <= 16*1024 &&
            (sector_len = d64_read_prg_sector(d64, ptr)))
    {
        prg_size += sector_len;
        ptr += sector_len;
    }

    dbg("Sending PRG bank size: %d\n", prg_size);
    if (prg_size + D64_SECTOR_DATA_LEN > 16*1024)
    {
        // Send bank
        disk_send_prg_bank(prg_size);
    }
    else
    {
        // All done
        disk_send_prg(prg_size);
    }
}

static void disk_handle_load_prg(D64 *d64, char *filename, uint8_t *ptr)
{
    if (disk_find_prg(d64, filename))
    {
        if (!d64_read_prg_start(d64))
        {
            c64_send_reply(REPLY_READ_ERROR);
            return;
        }

        uint16_t prg_size = 0;
        uint16_t start_addr = 0;
        uint8_t sector_len;

        while ((prg_size + D64_SECTOR_DATA_LEN) <= 16*1024 &&
                (sector_len = d64_read_prg_sector(d64, ptr)))
        {
            if (!prg_size)
            {
                if (sector_len > 2)
                {
                    sector_len -= 2;
                    start_addr = *((uint16_t *)ptr);
                    memmove(ptr, ptr + 2, sector_len);
                }
                else
                {
                    break;
                }
            }

            prg_size += sector_len;
            ptr += sector_len;
        }

        if (!prg_size)
        {
            c64_send_reply(REPLY_READ_ERROR);
            return;
        }

        dbg("Sending PRG size: %d start addr: $%x\n", prg_size, start_addr);
        if (prg_size + D64_SECTOR_DATA_LEN > 16*1024)
        {
            // Send first bank
            disk_send_prg_bank(prg_size);
        }
        else
        {
            // All done - send prg
            disk_send_prg(prg_size);
        }

        c64_send_data(&start_addr, 2);
    }
    else
    {
        c64_send_reply(REPLY_NOT_FOUND);
    }
}

static void disk_handle_load_dir(D64 *d64, char *filename, uint8_t *ptr)
{
    // Check drive number
    if (filename[1] >= '0' && filename[1] <= '9' && filename[2] == 0)
    {
        if (filename[1] != '0')
        {
            // Try serial drive (if any)
            c64_send_reply(REPLY_NOT_FOUND);
            return;
        }
        else
        {
            filename[1] = '*';
        }
    }

    if (filename[1] == 0)
    {
        filename[1] = '*';
        filename[2] = 0;
    }

    uint8_t *start_ptr = ptr;
    uint16_t start_addr = disk_create_dir_prg(d64, &ptr, filename + 1);
    uint16_t prg_size = ptr - start_ptr;

    if (prg_size)
    {
        disk_send_prg(prg_size);
        c64_send_data(&start_addr, 2);
    }
    else
    {
        c64_send_reply(REPLY_READ_ERROR);
    }
}

static void disk_handle_load(D64 *d64, char *filename, uint8_t *ptr)
{
    if (filename[0] == '$') // directory
    {
        disk_handle_load_dir(d64, filename, ptr);
    }
    else
    {
        disk_handle_load_prg(d64, filename, ptr);
    }
}

static void disk_loop(void)
{
    D64 *d64 = &d64_state.d64;  // Reuse memory
    uint8_t *ptr = crt_banks[1];

    uint8_t data;
    char filename[17] = {};

    while (true)
    {
        uint8_t command = c64_receive_command();
        switch (command)
        {
            case CMD_NONE:
                break;

            case CMD_LOAD:
                c64_receive_data(&data, 1);
                c64_receive_data(filename, data);
                filename[data] = 0;

                dbg("Got LOAD command for: %s\n", filename);
                disk_handle_load(d64, filename, ptr);
                break;

            case CMD_LOAD_NEXT_BANK:
                dbg("Got LOAD NEXT BANK command\n");
                disk_handle_load_next_bank(d64, ptr);
                break;

            default:
                wrn("Got unknown disk command: %02x\n", command);
                c64_send_reply(REPLY_OK);
                break;
        }
    }
}