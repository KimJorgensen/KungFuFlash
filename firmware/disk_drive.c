/*
 * Copyright (c) 2019-2022 Kim Jørgensen
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

#include "fs_drive.c"

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

    char c = (entry->type & D64_FILE_NO_SPLAT) ? ' ' : '*';
    put_u8(ptr, c);
    put_string(ptr, d64_types[entry->type & 7]);
    c = (entry->type & D64_FILE_LOCKED) ? '<' : ' ';
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

static char * disk_get_diskname(DISK_CHANNEL *channel)
{
    if (dat_file.disk.mode)
    {
        return d64_get_diskname(&channel->d64);
    }

    return fs_get_diskname(channel);
}

static void disk_rewind_dir(DISK_CHANNEL *channel)
{
    if (dat_file.disk.mode)
    {
        d64_rewind_dir(&channel->d64);
        return;
    }

    fs_rewind_dir(channel);
}

static D64_DIR_ENTRY * disk_read_dir(DISK_CHANNEL *channel)
{
    if (dat_file.disk.mode)
    {
        return d64_read_dir(&channel->d64);
    }

    return fs_read_dir(channel);
}

static u16 disk_get_blocks_free(DISK_CHANNEL *channel)
{
    if (dat_file.disk.mode)
    {
        return d64_get_blocks_free(&channel->d64);
    }

    return fs_get_blocks_free();
}

static void disk_open_file_read(DISK_CHANNEL *channel, D64_DIR_ENTRY *entry)
{
    channel->use_buf = false;
    if (dat_file.disk.mode)
    {
        d64_open_file_read(&channel->d64, entry);
        return;
    }

    fs_open_file_read(channel, entry);
}

static void disk_open_dir_read(DISK_CHANNEL *channel)
{
    if (dat_file.disk.mode)
    {
        channel->use_buf = false;
        d64_open_dir_read(&channel->d64);
        return;
    }

    // Not supported
    channel->data_len = 0;
    channel->use_buf = true;
}

static size_t disk_read_data(DISK_CHANNEL *channel, u8 *buf, size_t buf_size)
{
    if (channel->use_buf)
    {
        size_t read_bytes = 0;
        while (read_bytes < buf_size && channel->data_ptr < channel->data_len)
        {
            *buf++ = channel->buf[channel->data_ptr++];
            read_bytes++;
        }

        return read_bytes;
    }

    if (dat_file.disk.mode)
    {
        return d64_read_data(&channel->d64, buf, buf_size);
    }

    return fs_read_data(channel, buf, buf_size);
}

static bool disk_bytes_left(DISK_CHANNEL *channel)
{
    if (channel->use_buf)
    {
        return channel->data_ptr < channel->data_len;
    }

    if (dat_file.disk.mode)
    {
        return d64_bytes_left(&channel->d64) != 0;
    }

    return fs_bytes_left(channel);
}

static bool disk_create_file(DISK_CHANNEL *channel, const char *filename,
                             u8 file_type, D64_DIR_ENTRY *existing_file)
{
    if (dat_file.disk.mode)
    {
        return d64_create_file(&channel->d64, filename, file_type, existing_file);
    }

    return fs_create_file(channel, filename, file_type, existing_file);
}

static size_t disk_write_data(DISK_CHANNEL *channel, u8 *buf, size_t buf_size)
{
    if (dat_file.disk.mode)
    {
        return d64_write_data(&channel->d64, buf, buf_size);
    }

    return fs_write_data(channel, buf, buf_size);
}

static bool disk_write_finalize(DISK_CHANNEL *channel)
{
    if (dat_file.disk.mode)
    {
        return d64_write_finalize(&channel->d64);
    }

    return fs_write_finalize(channel);
}

static void disk_put_dir_header(DISK_CHANNEL *channel, u8 **ptr)
{
    put_u16(ptr, dir_link_addr);
    put_u16(ptr, 0);                    // drive number
    put_u8(ptr, 0x12);                  // reverse on
    char *diskname = disk_get_diskname(channel);
    put_diskname(ptr, diskname);
    put_u8(ptr, 0);                     // end of line

    disk_rewind_dir(channel);
}

static bool disk_put_dir_entries(DISK_CHANNEL *channel, u8 **ptr,
                                 const char *filename, u32 max_entries)
{
    D64_DIR_ENTRY *entry = NULL;
    while (max_entries && (entry = disk_read_dir(channel)))
    {
        if (disk_filename_match(entry, filename))
        {
            put_u16(ptr, dir_link_addr);
            put_dir_entry(ptr, entry);
            put_u8(ptr, 0);             // end of line
            max_entries--;
        }
    }

    return entry != 0;
}

static void disk_put_dir_footer(DISK_CHANNEL *channel, u8 **ptr)
{
    put_u16(ptr, dir_link_addr);
    u16 blocks_free = disk_get_blocks_free(channel);
    put_u16(ptr, blocks_free);
    put_string(ptr, "BLOCKS FREE.");
    put_u8_times(ptr, ' ', 13);
    put_u8(ptr, 0);                     // end of line
    put_u16(ptr, 0);                    // end of program
}

static void disk_create_dir_prg(DISK_CHANNEL *channel, u8 **ptr,
                                const char *filename)
{
    disk_put_dir_header(channel, ptr);
    disk_put_dir_entries(channel, ptr, filename, 1000);
    disk_put_dir_footer(channel, ptr);
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

static D64_DIR_ENTRY * disk_find_file(DISK_CHANNEL *channel,
                                      const char *filename, u8 file_type)
{
    disk_rewind_dir(channel);

    D64_DIR_ENTRY *entry;
    while ((entry = disk_read_dir(channel)))
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

static bool disk_open_read(DISK_CHANNEL *channel, const char *filename,
                           u8 file_type)
{
    D64_DIR_ENTRY *entry = disk_find_file(channel, filename, file_type);
    if (entry)
    {
        disk_open_file_read(channel, entry);
        return true;
    }

    return false;
}

static void disk_handle_load_prg(DISK_CHANNEL *channel, char *filename)
{
    PARSED_FILENAME parsed;
    if (!disk_is_file_supported(filename, &parsed))
    {
        c64_send_reply(REPLY_NO_DRIVE);     // Try serial device (if any)
        return;
    }

    if (!disk_open_read(channel, parsed.name, parsed.type))
    {
        c64_send_reply(REPLY_NOT_FOUND);
        return;
    }

    u16 start_addr;
    if (disk_read_data(channel, (u8 *)&start_addr, sizeof(start_addr)) !=
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
        if ((bank_size = disk_read_data(channel, channel->buf, 16*1024)) == 16*1024)
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

static bool disk_parse_dir(DISK_CHANNEL *channel, char **filename_ptr)
{
    char *filename = *filename_ptr;

    // Check drive number
    if (filename[0] >= '0' && filename[0] <= '9' &&
        (filename[1] == 0 || filename[1] == ':'))
    {
        if (filename[0] != '0')
        {
            return false;
        }

        if (filename[1] == 0)
        {
            filename[0] = '*';
        }
    }
    else if (filename[0] == 0)
    {
        filename[0] = '*';
        filename[1] = 0;
    }

    // Scan for filename
    char c;
    while ((c = *filename))
    {
        filename++;
        if (c == ':')
        {
            *filename_ptr = filename;
            break;
        }
    }

    // TODO: Also support '=' to filter on filetype

    return true;
}

static void disk_handle_load_dir(DISK_CHANNEL *channel, char *filename)
{
    if (!disk_parse_dir(channel, &filename))
    {
        c64_send_reply(REPLY_NO_DRIVE); // Try serial device (if any)
        return;
    }

    u16 start_addr = dir_start_addr;
    u8 *ptr = channel->buf;
    disk_put_dir_header(channel, &ptr);

    int i;
    for (i=0;; i++)
    {
        // Limit dir to 2 banks (~32k)
        if(!disk_put_dir_entries(channel, &ptr, filename, 500) || i >= 1)
        {
            break;
        }

        u16 prg_size = ptr - channel->buf;
        c64_send_reply(REPLY_OK);
        c64_send_data(&prg_size, 2);
        if (i == 0)
        {
            c64_send_data(&start_addr, 2);
        }

        c64_receive_byte(); // Wait for C64
        ptr = channel->buf;
    }

    disk_put_dir_footer(channel, &ptr);

    u16 prg_size = ptr - channel->buf;
    c64_send_reply(REPLY_END_OF_FILE);
    c64_send_data(&prg_size, 2);
    if (i == 0)
    {
        c64_send_data(&start_addr, 2);
    }
}

static void disk_close_channel(DISK_CHANNEL *channel)
{
    channel->use_buf = false;
    channel->data_len = 0;
    d64_init(channel->d64.image, &channel->d64);
}

static void disk_init_all_channels(D64_IMAGE *image, DISK_CHANNEL *channels)
{
    for (u8 i=0; i<16; i++)
    {
        DISK_CHANNEL *channel = channels + i;
        channel->number = i;
        channel->d64.image = image;
        disk_close_channel(channel);
    }
}

static void disk_handle_load(DISK_CHANNEL *channel, char *filename)
{
    if (filename[0] == '$') // directory
    {
        disk_handle_load_dir(channel, filename + 1);
    }
    else
    {
        disk_handle_load_prg(channel, filename);
    }

    disk_close_channel(channel);
}

static void disk_send_done(u8 ret_val)
{
    c64_interface(true);
    c64_send_data("DONE", 4);
    c64_send_reply(ret_val);
}

static u8 disk_save_file(DISK_CHANNEL *channel, PARSED_FILENAME *parsed, u8 *buf)
{
    u16 start_addr;
    c64_receive_data(&start_addr, 2);
    c64_interface(false);

    D64_DIR_ENTRY *existing = disk_find_file(channel, parsed->name, 0);

    if ((existing && (!parsed->overwrite ||
                      !d64_is_file_type(existing, parsed->type))) ||
        (!existing && parsed->wildcard))
    {
        return REPLY_DISK_ERROR;
    }
    else if (!disk_create_file(channel, parsed->name, parsed->type, existing))
    {
        return REPLY_DISK_ERROR;
    }

    if (disk_write_data(channel, (u8 *)&start_addr, sizeof(start_addr)) !=
        sizeof(start_addr))
    {
        return REPLY_DISK_ERROR;
    }
    disk_send_done(REPLY_OK);

    u8 recv_size;
    while ((recv_size = c64_receive_byte()))
    {
        c64_interface(false);
        if (disk_write_data(channel, buf, recv_size) != recv_size)
        {
            return REPLY_DISK_ERROR;
        }

        disk_send_done(REPLY_OK);
    }

    c64_interface(false);
    if (!disk_write_finalize(channel))
    {
        return REPLY_DISK_ERROR;
    }

    return REPLY_OK;
}

static void disk_handle_save(DISK_CHANNEL *channel, char *filename)
{
    PARSED_FILENAME parsed;
    if (!disk_is_file_supported(filename, &parsed))
    {
        c64_send_reply(REPLY_NO_DRIVE); // Try serial device (if any)
        return;
    }
    c64_send_reply(REPLY_OK);

    u8 ret_val = disk_save_file(channel, &parsed, channel->buf);
    disk_send_done(ret_val);

    disk_close_channel(channel);
}

static bool disk_read_status(DISK_CHANNEL *channel, char *filename)
{
    const char *status;
    if (filename[0] == 'M') // Memory command
    {
        status = "";    // Not supported
    }
    else if (filename[0] == 'C' && filename[1] == 'D')  // Directory command
    {
        filename += 2;
        // Check drive number
        if (filename[0] >= '0' && filename[0] <= '9')
        {
            if (*filename++ != '0')
            {
                return false;
            }
        }

        if (fs_change_dir(channel, filename))
        {
            status = DISK_STATUS_OK;
        }
        else
        {
            status = DISK_STATUS_NOT_FOUND;
        }
    }
    else if (filename[0] == 'U' && filename[1] == 'I')  // Soft reset
    {
        status = DISK_STATUS_INIT;
    }
    else
    {
        // TODO: Return last error
        status = DISK_STATUS_OK;
    }

    u8 len;
    for (len=0; *status; len++)
    {
        channel->buf[len] = *status++;
    }
    channel->buf[len++] = '\r';

    channel->data_len = len;
    channel->data_ptr = 0;
    channel->use_buf = true;

    return true;
}

static void disk_handle_open_dir(DISK_CHANNEL *channel, char *filename)
{
    if (channel->number)    // Channel 1-14
    {
        disk_open_dir_read(channel);
        disk_read_data(channel, NULL, 0);
        if (!disk_bytes_left(channel))
        {
            c64_send_reply(REPLY_END_OF_FILE);
            return;
        }
    }
    else                    // Channel 0
    {
        if (!disk_parse_dir(channel, &filename))
        {
            c64_send_reply(REPLY_NO_DRIVE); // Try serial device (if any)
            return;
        }

        *(u16 *)channel->buf = dir_start_addr;
        u8 *ptr = channel->buf + 2;
        disk_create_dir_prg(channel, &ptr, filename);
        channel->data_len = ptr - channel->buf;
        channel->data_ptr = 0;
        channel->use_buf = true;
    }

    c64_send_reply(REPLY_OK);
}

static void disk_handle_open_prg(DISK_CHANNEL *channel, char *filename)
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

    if (!disk_open_read(channel, parsed.name, parsed.type))
    {
        c64_send_reply(REPLY_NOT_FOUND);
        return;
    }

    disk_read_data(channel, NULL, 0);
    if (disk_bytes_left(channel))
    {
        c64_send_reply(REPLY_OK);
    }
    else
    {
        c64_send_reply(REPLY_END_OF_FILE);
    }
}

static void disk_handle_open(DISK_CHANNEL *channel, char *filename)
{
    if (channel->number == 15)      // Command channel
    {
        if (disk_read_status(channel, filename))
        {
            c64_send_reply(REPLY_OK);
        }
        else
        {
            c64_send_reply(REPLY_NO_DRIVE);
        }
    }
    else if (filename[0] == '$')    // Directory
    {
        disk_handle_open_dir(channel, filename + 1);
    }
    else
    {
        disk_handle_open_prg(channel, filename);
    }
}

static void disk_handle_close(DISK_CHANNEL *channel, DISK_CHANNEL *channels)
{
    if (channel->number == 15)  // Command channel
    {
        disk_init_all_channels(channel->d64.image, channels);
    }
    else
    {
        disk_close_channel(channel);
    }

    c64_send_reply(REPLY_OK);
}

static void disk_handle_get_byte(DISK_CHANNEL *channel)
{
    u8 data;
    if (!channel || !disk_read_data(channel, &data, 1))
    {
        c64_send_reply(REPLY_DISK_ERROR);
        return;
    }

    u8 reply;
    if (!disk_bytes_left(channel))
    {
        if (channel->number == 15)
        {
            disk_read_status(channel, NULL);
        }

        reply = REPLY_END_OF_FILE;
    }
    else
    {
        reply = REPLY_OK;
    }

    c64_send_reply(reply);
    c64_send_byte(data);
}

static DISK_CHANNEL * disk_receive_channel(DISK_CHANNEL *channels)
{
    u8 channel = c64_receive_byte();
    channel &= 0x0f;

    return channels + channel;
}

static void disk_receive_filename(char *filename)
{
    u8 size = c64_receive_byte();

    for (; size; size--)
    {
        u8 c = c64_receive_byte();
        if (c == 0xff)
        {
            c = '~';
        }
        *filename++ = c;
    }
    *filename = 0;
}

static void disk_loop(void)
{
    D64_IMAGE *image = &d64_state.image;    // Reuse memory from menu
    DISK_CHANNEL *channel = NULL, *channels = (DISK_CHANNEL *)crt_ram_banks[1];
    memset(channels, 0, sizeof(DISK_CHANNEL) * 16);
    disk_init_all_channels(image, channels);

    u8 *load_buf = crt_banks[1];
    channels[0].buf = load_buf;

    u8 *save_buf = &CRT_RAM_BUF[SAVE_BUFFER_OFFSET];
    channels[1].buf = save_buf;

    u8 *cmd_buf = channels[15].d64.sector.data;
    channels[15].buf = cmd_buf;
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
                channel = channels + 0;
                disk_receive_filename(filename);
                dbg("Got LOAD command for: %s\n", filename);
                disk_handle_load(channel, filename);
                break;

            case CMD_SAVE:
                // Channel 1 will be used as save buffer - just as on 1541
                channel = channels + 1;
                disk_receive_filename(filename);
                dbg("Got SAVE command for: %s\n", filename);
                disk_handle_save(channel, filename);
                break;

            case CMD_OPEN:
                channel = disk_receive_channel(channels);
                disk_receive_filename(filename);
                dbg("Got OPEN command for channel %d for: %s\n",
                    channel->number, filename);
                disk_handle_open(channel, filename);
                break;

            case CMD_CLOSE:
                channel = disk_receive_channel(channels);
                dbg("Got CLOSE command for channel %d\n", channel->number);
                disk_handle_close(channel, channels);
                break;

            case CMD_TALK:
                channel = disk_receive_channel(channels);
                c64_send_reply(REPLY_OK);
                break;

            case CMD_UNTALK:
                channel = NULL;
                c64_send_reply(REPLY_OK);
                break;

            case CMD_GET_BYTE:
                disk_handle_get_byte(channel);
                break;

            default:
                wrn("Got unknown disk command: %02x\n", command);
                c64_send_reply(REPLY_OK);
                break;
        }
    }
}