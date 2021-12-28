/*
 * Copyright (c) 2019-2021 Kim Jørgensen
 * Copyright (c) 2020 Sandor Vass
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

static inline void put_u8(u8 **ptr, u8 value)
{
    *(*ptr)++ = value;
}

static inline void put_u16(u8 **ptr, u16 value)
{
    *(*ptr)++ = (u8)value;
    *(*ptr)++ = (u8)(value >> 8);
}

static void put_u8_times(u8 **ptr, u8 value, size_t size)
{
    while (size--)
    {
        put_u8(ptr, value);
    }
}

static void put_string(u8 **ptr, const char* str)
{
    while (*str)
    {
        put_u8(ptr, *str++);
    }
}

static void put_diskname(u8 **ptr, char* str)
{
    put_u8(ptr, '"');
    str[16] = '"';
    str[17] = ' ';

    for (u8 i=0; i<23; i++)
    {
        char c = *str++;
        if (c == 0xa0)
        {
            c = (i != 22) ? ' ' : '1';
        }

        put_u8(ptr, c);
    }
}

static void put_filename(u8 **ptr, const char* str)
{
    put_u8(ptr, '"');
    char end_char = '"';

    for (u8 i=0; i<16; i++)
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

static void put_dir_entry(u8 **ptr, D64_DIR_ENTRY *entry)
{
    put_u16(ptr, entry->blocks);

    u8 pad = 0;
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
    for (u8 i=0; i<16; i++)
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

static u16 disk_create_dir_prg(D64 *d64, u8 **ptr,
                                    const char *filename)
{
    const u16 start_addr = 0x0401;      // Start of BASIC (for the PET)
    const u16 link_addr = 0x0101;

    put_u16(ptr, link_addr);
    put_u16(ptr, 0);                    // drive number
    put_u8(ptr, 0x12);                  // reverse on
    char *diskname = d64_get_diskname(d64);
    put_diskname(ptr, diskname);
    put_u8(ptr, 0);                     // end of line

    d64_rewind_dir(d64);
    D64_DIR_ENTRY *entry;
    while ((entry = d64_read_dir(d64)))
    {
        if (disk_filename_match(entry, filename))
        {
            put_u16(ptr, link_addr);
            put_dir_entry(ptr, entry);
            put_u8(ptr, 0);             // end of line
        }
    }

    put_u16(ptr, link_addr);
    u16 blocks_free = d64_get_blocks_free(d64);
    put_u16(ptr, blocks_free);
    put_string(ptr, "BLOCKS FREE.");
    put_u8_times(ptr, ' ', 13);
    put_u8(ptr, 0);                     // end of line
    put_u16(ptr, 0);                    // end of program

    return start_addr;
}

static void disk_parse_filename(char *filename, PARSED_FILENAME *parsed)
{
    char *f_ptr = filename;
    char c, last_c = 0;
    parsed->overwrite = false;
    parsed->drive = 0;

    // Scan for drive number and overwrite flag
    while ((c = *f_ptr++))
    {
        if (c == ':')
        {
            parsed->overwrite = (*filename == '@');
            if (last_c >= '0' && last_c <= '9')
            {
                parsed->drive = last_c - '0';
            }

            filename = f_ptr;
            break;
        }

        last_c = c;
    }

    f_ptr = filename;
    parsed->name = filename;
    parsed->wildcard = false;
    parsed->type = 0;
    parsed->mode = 0;
    bool end_of_filename = false;

    // Scan for wildcard, type and mode
    while (*f_ptr)
    {
        if (*f_ptr == ',')
        {
            *f_ptr++ = 0;   // Null terminate filename
            end_of_filename = true;

            if (!parsed->type)
            {
                switch (*f_ptr)
                {
                    case 'S':
                        parsed->type = D64_FILE_SEQ;
                        break;

                    case 'U':
                        parsed->type = D64_FILE_USR;
                        break;

                    case 'L':
                        parsed->type = D64_FILE_REL;
                        break;

                    default:
                        parsed->type = D64_FILE_PRG;
                        break;
                }
            }
            else if (!parsed->mode)
            {
                parsed->mode = *f_ptr;
                break;
            }
        }
        else
        {
            if (!end_of_filename && (*f_ptr == '*' || *f_ptr == '?'))
            {
                parsed->wildcard = true;
            }

            f_ptr++;
        }
    }
}

static bool disk_is_file_supported(char *filename, PARSED_FILENAME *parsed)
{
    disk_parse_filename(filename, parsed);

    if (parsed->drive)
    {
        // this is a 'single drive system'
        return false;
    }

    if (!parsed->type)
    {
        parsed->type = D64_FILE_PRG;
    }
    else if (parsed->type == D64_FILE_REL)
    {
        // REL files are not supported yet
        return false;
    }

    return true;
}

static D64_DIR_ENTRY * disk_find_file(D64 *d64, const char *filename,
                                      u8 file_type)
{
    d64_rewind_dir(d64);

    D64_DIR_ENTRY *entry;
    while ((entry = d64_read_dir(d64)))
    {
        if (file_type && !d64_is_file_type(entry, file_type))
        {
            continue;
        }

        if (disk_filename_match(entry, filename))
        {
            return entry;
        }
    }

    return NULL;
}

static bool disk_open_file_read(D64 *d64, const char *filename, u8 file_type)
{
    D64_DIR_ENTRY *entry = disk_find_file(d64, filename, file_type);
    if (entry)
    {
        d64_open_file_read(d64, entry);
        return true;
    }

    return false;
}

static void disk_handle_load_prg(D64 *d64, char *filename, u8 *buf)
{
    PARSED_FILENAME parsed;
    if (!disk_is_file_supported(filename, &parsed))
    {
        c64_send_reply(REPLY_NO_DRIVE);     // Try serial device (if any)
        return;
    }

    if (!disk_open_file_read(d64, parsed.name, parsed.type))
    {
        c64_send_reply(REPLY_NOT_FOUND);
        return;
    }

    u16 start_addr;
    if (d64_read_data(d64, (u8 *)&start_addr, sizeof(start_addr)) !=
        sizeof(start_addr))
    {
        c64_send_reply(REPLY_DISK_ERROR);
        return;
    }

    bool send_start_addr = true;
    while (true)
    {
        u8 reply;
        u16 bank_size;
        if ((bank_size = d64_read_data(d64, buf, 16*1024)) == 16*1024)
        {
            reply = REPLY_OK;           // Send bank
        }
        else
        {
            reply = REPLY_END_OF_FILE;  // All done
        }

        c64_send_reply(reply);
        c64_send_data(&bank_size, 2);
        if (send_start_addr)
        {
            send_start_addr = false;
            dbg("Sending PRG start addr: $%x\n", start_addr);
            c64_send_data(&start_addr, 2);
        }

        dbg("Sending PRG bank size: %d\n", bank_size);
        if (reply == REPLY_OK)
        {
            c64_receive_byte();         // Wait for C64
        }
        else
        {
            break;
        }
    }
}

static void disk_handle_load_dir(D64 *d64, char *filename, u8 *buf)
{
    // Check drive number
    if (filename[0] >= '0' && filename[0] <= '9')
    {
        if (filename[0] != '0')
        {
            c64_send_reply(REPLY_NO_DRIVE); // Try serial device (if any)
            return;
        }

        if (filename[1] == 0)
        {
            filename[0] = '*';
        }
        else
        {
            // Scan for filename
            char c;
            while ((c = *filename))
            {
                filename++;
                if (c == ':')
                {
                    break;
                }
            }
        }
    }
    else if (filename[0] == 0)
    {
        filename[0] = '*';
        filename[1] = 0;
    }

    // TODO: Also support '=' to filter on filetype

    u8 *ptr = buf;
    u16 start_addr = disk_create_dir_prg(d64, &ptr, filename);
    u16 prg_size = ptr - buf;

    if (prg_size)
    {
        c64_send_reply(REPLY_END_OF_FILE);
        c64_send_data(&prg_size, 2);
        c64_send_data(&start_addr, 2);
    }
    else
    {
        c64_send_reply(REPLY_DISK_ERROR);
    }
}

static void disk_init_channel(D64_IMAGE *image, D64 *d64)
{
    d64->image = image;
    d64->data_len = d64->data_ptr = 0;
    d64->sector.next.track = 0;
}

static void disk_init_all_channels(D64_IMAGE *image, D64 *channels)
{
    for (u8 i=0; i<16; i++)
    {
        D64 *d64 = channels + i;
        d64->channel = i;
        disk_init_channel(image, d64);
    }
}

static void disk_handle_load(D64 *d64, char *filename, u8 *buf)
{
    if (filename[0] == '$') // directory
    {
        disk_handle_load_dir(d64, filename + 1, buf);
    }
    else
    {
        disk_handle_load_prg(d64, filename, buf);
    }

    disk_init_channel(d64->image, d64); // Close channel
}

static void disk_send_done(u8 ret_val)
{
    c64_interface(true);
    c64_send_data("DONE", 4);
    c64_send_reply(ret_val);
}

static u8 disk_save_file(D64 *d64, PARSED_FILENAME *parsed, u8 *buf)
{
    u16 start_addr;
    c64_receive_data(&start_addr, 2);
    c64_interface(false);

    D64_DIR_ENTRY *existing = disk_find_file(d64, parsed->name, 0);

    if ((existing && (!parsed->overwrite ||
                      !d64_is_file_type(existing, parsed->type))) ||
        (!existing && parsed->wildcard))
    {
        return REPLY_DISK_ERROR;
    }
    else if (!d64_create_file(d64, parsed->name, parsed->type, existing))
    {
        return REPLY_DISK_ERROR;
    }
    disk_send_done(REPLY_OK);

    *(u16 *)d64->sector.data = start_addr;
    d64->data_ptr = 2;

    u8 recv_size;
    while ((recv_size = c64_receive_byte()))
    {
        c64_interface(false);
        if (d64_write_data(d64, buf, recv_size) != recv_size)
        {
            return REPLY_DISK_ERROR;
        }

        disk_send_done(REPLY_OK);
    }

    c64_interface(false);
    if (!d64_write_finalize(d64))
    {
        return REPLY_DISK_ERROR;
    }

    return REPLY_OK;
}

static void disk_handle_save(D64 *d64, char *filename, u8 *buf)
{
    PARSED_FILENAME parsed;
    if (!disk_is_file_supported(filename, &parsed))
    {
        c64_send_reply(REPLY_NO_DRIVE); // Try serial device (if any)
        return;
    }
    c64_send_reply(REPLY_OK);

    u8 ret_val = disk_save_file(d64, &parsed, buf);
    disk_send_done(ret_val);

    disk_init_channel(d64->image, d64); // Close channel
}

static void disk_read_status(D64 *d64, const char *filename)
{
    d64->data_ptr = 0;
    d64->sector.next.track = 0;

    const char *status;
    if (filename[0] == 'U' && filename[1] == 'I')
    {
        status = "73,KUNG FU FLASH V" VERSION ",00,00\r";
    }
    else
    {
        // TODO: Return last error
        status = "00, OK,00,00\r";
    }

    u8 len;
    for (len=0; *status; len++)
    {
        d64->sector.data[len] = *status++;
    }
    d64->data_len = len;
}

static void disk_handle_open(D64 *d64, char *filename)
{
    PARSED_FILENAME parsed;
    disk_parse_filename(filename, &parsed);

    if (parsed.drive || parsed.type == D64_FILE_REL)
    {
        c64_send_reply(REPLY_NO_DRIVE);     // Try serial device (if any)
        return;
    }

    if (parsed.mode == 'W')
    {
        c64_send_reply(REPLY_DISK_ERROR);   // Not supported yet
        return;
    }

    if (d64->channel == 15)
    {
        disk_read_status(d64, parsed.name);
    }
    else if (disk_open_file_read(d64, parsed.name, parsed.type))
    {
        d64_read_data(d64, NULL, 0);
    }
    else
    {
        c64_send_reply(REPLY_NOT_FOUND);
        return;
    }

    if (d64_bytes_left(d64))
    {
        c64_send_reply(REPLY_OK);
    }
    else
    {
        c64_send_reply(REPLY_END_OF_FILE);
    }
}

static void disk_handle_close(D64_IMAGE *image, D64 *channels, D64 *d64)
{
    if (d64->channel == 15)
    {
        disk_init_all_channels(image, channels);
    }
    else
    {
        disk_init_channel(image, d64);
    }

    c64_send_reply(REPLY_OK);
}

static void disk_handle_get_byte(D64 *d64)
{
    u8 data;
    if (!d64 || !d64_read_data(d64, &data, 1))
    {
        c64_send_reply(REPLY_DISK_ERROR);
        return;
    }

    u8 reply;
    if (!d64_bytes_left(d64))
    {
        if (d64->channel == 15)
        {
            disk_read_status(d64, NULL);
        }

        reply = REPLY_END_OF_FILE;
    }
    else
    {
        reply = REPLY_OK;
    }

    c64_send_reply(reply);
    c64_send_data(&data, 1);
}

static D64 * disk_receive_channel(D64 *channels)
{
    u8 channel = c64_receive_byte();
    channel &= 0x0f;

    return channels + channel;
}

static inline void disk_receive_filename(char *filename)
{
    c64_receive_string(filename);
}

static void disk_loop(void)
{
    D64_IMAGE *image = &d64_state.image;    // Reuse memory from menu
    D64 *d64 = NULL, *channels = (D64 *)crt_ram_banks[1];
    disk_init_all_channels(image, channels);

    u8 *load_buf = crt_banks[1];
    u8 *save_buf = &CRT_RAM_BUF[SAVE_BUFFER_OFFSET];
    char filename[42] = {0};

    while (true)
    {
        u8 command = c64_receive_command();
        switch (command)
        {
            case CMD_NONE:
                break;

            case CMD_LOAD:
                // Channel 0 will be used as load buffer - just as on 1541
                d64 = channels + 0;
                disk_receive_filename(filename);
                dbg("Got LOAD command for: %s\n", filename);
                disk_handle_load(d64, filename, load_buf);
                break;

            case CMD_SAVE:
                // Channel 1 will be used as save buffer - just as on 1541
                d64 = channels + 1;
                disk_receive_filename(filename);
                dbg("Got SAVE command for: %s\n", filename);
                disk_handle_save(d64, filename, save_buf);
                break;

            case CMD_OPEN:
                d64 = disk_receive_channel(channels);
                disk_receive_filename(filename);
                dbg("Got OPEN command for channel %d for: %s\n",
                    d64->channel, filename);
                disk_handle_open(d64, filename);
                break;

            case CMD_CLOSE:
                d64 = disk_receive_channel(channels);
                dbg("Got CLOSE command for channel %d\n", d64->channel);
                disk_handle_close(image, channels, d64);
                break;

            case CMD_TALK:
                d64 = disk_receive_channel(channels);
                c64_send_reply(REPLY_OK);
                break;

            case CMD_UNTALK:
                d64 = NULL;
                c64_send_reply(REPLY_OK);
                break;

            case CMD_GET_BYTE:
                disk_handle_get_byte(d64);
                break;

            default:
                wrn("Got unknown disk command: %02x\n", command);
                c64_send_reply(REPLY_OK);
                break;
        }
    }
}