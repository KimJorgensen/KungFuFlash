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

    for (u32 i=0; i<23; i++)
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

    for (u32 i=0; i<16; i++)
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
    for (u32 i=0; i<16; i++)
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
    channel->buf_mode = DISK_BUF_NONE;
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
        channel->buf_mode = DISK_BUF_NONE;
        d64_open_dir_read(&channel->d64);
        return;
    }

    // Not supported
    channel->buf_len = 0;
    channel->buf_mode = DISK_BUF_USE;
}

static size_t disk_read_data(DISK_CHANNEL *channel, u8 *buf, size_t buf_size)
{
    if (channel->buf_mode)
    {
        size_t read_bytes = 0;
        while (read_bytes < buf_size && channel->buf_ptr < channel->buf_len)
        {
            *buf++ = channel->buf[channel->buf_ptr++];
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
    if (channel->buf_mode)
    {
        return channel->buf_ptr < channel->buf_len;
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

static bool disk_delete_file(DISK_CHANNEL *channel, D64_DIR_ENTRY *entry)
{
    if (dat_file.disk.mode)
    {
        return d64_delete_file(&channel->d64, entry);
    }

    return fs_delete_file(channel, entry);
}

static void disk_put_dir_header(DISK_CHANNEL *channel, u8 **ptr)
{
    put_u16(ptr, dir_start_addr);       // start address

    put_u16(ptr, dir_link_addr);
    put_u16(ptr, 0);                    // drive number
    put_u8(ptr, 0x12);                  // reverse on
    char *diskname = disk_get_diskname(channel);
    put_diskname(ptr, diskname);
    put_u8(ptr, 0);                     // end of line

    disk_rewind_dir(channel);
}

static bool disk_put_dir_entry(DISK_CHANNEL *channel, u8 **ptr)
{
    D64_DIR_ENTRY *entry = NULL;

    while ((entry = disk_read_dir(channel)))
    {
        if (disk_filename_match(entry, channel->filename_dir))
        {
            put_u16(ptr, dir_link_addr);
            put_dir_entry(ptr, entry);
            put_u8(ptr, 0);             // end of line
            return true;
        }
    }

    return false;
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

static void disk_create_dir_prg(DISK_CHANNEL *channel, u8 **ptr)
{
    disk_put_dir_header(channel, ptr);
    // Limit dir to 1000 entries (~32k)
    for (u32 i=0; i<1000 && disk_put_dir_entry(channel, ptr); i++);
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
        disk_last_error = DISK_STATUS_OK;
        return true;
    }

    disk_last_error = DISK_STATUS_NOT_FOUND;
    return false;
}

static u8 disk_handle_load_prg(DISK_CHANNEL *channel)
{
    PARSED_FILENAME parsed;
    if (!disk_is_file_supported(channel->filename, &parsed))
    {
        return CMD_NO_DRIVE;    // Try serial device (if any)
    }

    if (!disk_open_read(channel, parsed.name, parsed.type))
    {
        return CMD_NOT_FOUND;
    }

    u8 *ptr = KFF_BUF;
    u16 prg_size = disk_read_data(channel, ptr + 2, 64*1024 - 2);
    if (prg_size < 2)
    {
        return CMD_DISK_ERROR;
    }

    *(u16 *)ptr = prg_size  - 2;

    dbg("Sending PRG. Start $%x size %u", *(u16 *)(ptr + 2), prg_size);

    return CMD_NONE;
}

static bool disk_parse_dir(DISK_CHANNEL *channel)
{
    char *filename = channel->filename_dir;

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
            channel->filename_dir = filename;
            break;
        }
    }

    // TODO: Also support '=' to filter on filetype

    return true;
}

static u8 disk_handle_load_dir(DISK_CHANNEL *channel)
{
    if (!disk_parse_dir(channel))
    {
        return CMD_NO_DRIVE;    // Try serial device (if any)
    }

    u8 *ptr = KFF_BUF + 2;
    disk_create_dir_prg(channel, &ptr);

    u16 prg_size = (ptr - KFF_BUF) - 4;
    *(u16 *)KFF_BUF = prg_size;

    disk_last_error = DISK_STATUS_OK;
    return CMD_NONE;
}

static void disk_receive_data(u8 *buf)
{
    u8 size = c64_receive_byte();
    *buf++ = size;

    while (size--)
    {
        *buf++ = c64_receive_byte();
    }
}

static void disk_send_data(u8 *buf)
{
    u8 size = *buf++;
    c64_send_byte(size);

    while (size--)
    {
        c64_send_byte(*buf++);
    }
}

static void disk_close_channel(DISK_CHANNEL *channel)
{
    if (channel->buf_mode == DISK_BUF_SAVE)
    {
        c64_send_command(CMD_WAIT_SYNC);
        c64_interface(false);
        disk_receive_data(channel->buf2);   // Save temp data

        if (channel->buf_ptr)
        {
            disk_write_data(channel, channel->buf, channel->buf_ptr);
        }

        disk_write_finalize(channel);

        disk_send_data(channel->buf2);   // Send temp data back
        c64_interface_sync();
    }

    channel->buf_mode = DISK_BUF_USE;
    channel->buf_ptr = 0;
    channel->buf_len = 0;
    channel->buf2_ptr = 0;
    d64_init(channel->d64.image, &channel->d64);
}

static void disk_init_all_channels(D64_IMAGE *image, DISK_CHANNEL *channels)
{
    for (u32 i=0; i<16; i++)
    {
        DISK_CHANNEL *channel = channels + i;
        channel->number = i;
        channel->d64.image = image;
        disk_close_channel(channel);
    }
}

static u8 disk_handle_load(DISK_CHANNEL *channel)
{
    u8 cmd;
    if (channel->filename[0] == '$')    // directory
    {
        channel->filename_dir = channel->filename + 1;
        cmd = disk_handle_load_dir(channel);
    }
    else
    {
        cmd = disk_handle_load_prg(channel);
    }

    disk_close_channel(channel);
    return cmd;
}

static u8 disk_save_file(DISK_CHANNEL *channel, PARSED_FILENAME *parsed,
                         D64_DIR_ENTRY *existing)
{
    if (!disk_create_file(channel, parsed->name, parsed->type, existing))
    {
        return CMD_DISK_ERROR;
    }

    u8 *ptr = KFF_BUF;
    u16 prg_size = (*(u16 *)ptr) + 2;

    if (disk_write_data(channel, ptr + 2, prg_size) != prg_size)
    {
        return CMD_DISK_ERROR;
    }

    if (!disk_write_finalize(channel))
    {
        return CMD_DISK_ERROR;
    }

    return CMD_NONE;
}

static u8 disk_handle_save(DISK_CHANNEL *channel)
{
    char *filename = channel->filename;

    PARSED_FILENAME parsed;
    if (!disk_is_file_supported(filename, &parsed))
    {
        return CMD_NO_DRIVE;    // Try serial device (if any)
    }

    D64_DIR_ENTRY *existing = disk_find_file(channel, parsed.name, 0);
    if ((existing && (!parsed.overwrite ||
                      !d64_is_file_type(existing, parsed.type))) ||
        (!existing && parsed.wildcard))
    {
        disk_last_error = DISK_STATUS_EXISTS;
        return CMD_DISK_ERROR;
    }

    disk_receive_data(channel->buf);    // Save temp data

    disk_last_error = DISK_STATUS_OK;
    c64_send_command(CMD_NONE);
    c64_interface(false);

    u8 cmd = disk_save_file(channel, &parsed, existing);

    disk_send_data(channel->buf);   // Send temp data back
    c64_interface_sync();

    disk_close_channel(channel);
    return cmd;
}

static u32 disk_write_status(DISK_CHANNEL *channel, u8 status, u8 track, u8 sector)
{
    const char *status_text;
    switch (status)
    {
        case DISK_STATUS_SCRATCHED:
            status_text = "FILES SCRATCHED";
            break;

        case DISK_STATUS_NOT_FOUND:
            status_text = "FILE NOT FOUND";
            break;

        case DISK_STATUS_EXISTS:
            status_text = "FILE EXISTS";
            break;

        case DISK_STATUS_INIT:
            status_text = "KUNG FU FLASH V" VERSION;
            break;

        case DISK_STATUS_UNSUPPORTED:
            return 0;

        default:
            status_text = " OK";
            break;
    }

    sprint((char *)channel->buf, "%2u,%s,%2u,%2u",
            status, status_text, track, sector);

    u32 len = strlen((char *)channel->buf);
    return len;
}

static u8 disk_parse_number(char **ptr, u8 max_digits)
{
    while (**ptr && !isdigit(**ptr))    // Skip non-digits
    {
        (*ptr)++;
    }

    u32 digits = 0;
    while (isdigit(**ptr))  // Get number of digits
    {
        digits++;
        (*ptr)++;
    }

    if (digits > max_digits)
    {
        digits = max_digits;
    }

    u32 result = 0;
    u32 mul = 1;

    for (u32 i=1; i<=digits; i++)
    {
        result += (*(*ptr - i) - '0') * mul;
        mul *= 10;
    }

    return result;
}

static bool disk_handle_command(DISK_CHANNEL *channel, char *filename)
{
    u8 status = DISK_STATUS_OK;
    u8 track = 0, sector = 0;

    if (!filename[0])   // No command
    {
        return true;
    }

    if (filename[0] == 'M') // Memory command
    {
        status = DISK_STATUS_UNSUPPORTED;
    }
    else if (filename[0] == 'S')    // Scratch command
    {
        // Scan for drive number and start of filename
        while (*filename)
        {
            if (filename[1] == ':')
            {
                // Check drive number
                if (filename[0] >= '1' && filename[0] <= '9')
                {
                    return false;
                }

                filename += 2;
                break;
            }

            filename++;
        }

        c64_send_command(CMD_WAIT_SYNC);
        c64_interface(false);
        disk_receive_data(channel->buf);   // Save temp data

        disk_rewind_dir(channel);

        D64_DIR_ENTRY *entry;
        while ((entry = disk_read_dir(channel)))
        {
            if (disk_filename_match(entry, filename) &&
                disk_delete_file(channel, entry))
            {
                disk_rewind_dir(channel);
                track++;
            }
        }

        disk_send_data(channel->buf);  // Send temp data back
        c64_interface_sync();

        status = DISK_STATUS_SCRATCHED;
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

        if (!fs_change_dir(channel, filename))
        {
            status = DISK_STATUS_NOT_FOUND;
        }
    }
    else if (filename[0] == 'B' && filename[1] == '-' &&
             filename[2] == 'P')    // Buffer pointer
    {
        filename += 3;
        u8 channel_no = disk_parse_number(&filename, 2);
        u8 location = disk_parse_number(&filename, 3);

        DISK_CHANNEL *buf_channel = channel - (15 - channel_no);
        if (dat_file.disk.mode && channel_no >= 2 && channel_no <= 14 &&
            buf_channel->buf_mode == DISK_BUF_USE)
        {
            buf_channel->buf_ptr = location;
        }
        else
        {
            status = DISK_STATUS_NOT_FOUND;
        }
    }
    else if (filename[0] == 'U' &&
             (filename[1] == '1' || filename[1] == 'A' ||   // Block read
              filename[1] == '2' || filename[1] == 'B'))    // Block write
    {
        bool read = filename[1] == '1' || filename[1] == 'A';
        filename += 2;

        u8 channel_no = disk_parse_number(&filename, 2);
        u8 drive = disk_parse_number(&filename, 1);
        track = disk_parse_number(&filename, 2);
        sector = disk_parse_number(&filename, 2);

        DISK_CHANNEL *buf_channel = channel - (15 - channel_no);
        if (dat_file.disk.mode && channel_no >= 2 && channel_no <= 14 &&
            buf_channel->buf_mode == DISK_BUF_USE && !drive && track)
        {
            D64_TS ts = {track, sector};
            if (read)
            {
                d64_read_sector(&buf_channel->d64, buf_channel->buf, ts);
                buf_channel->buf_ptr = 0;
            }
            else
            {
                c64_send_command(CMD_WAIT_SYNC);
                c64_interface(false);
                disk_receive_data(channel->buf);    // Save temp data

                d64_write_sector(&buf_channel->d64, buf_channel->buf, ts);

                disk_send_data(channel->buf);   // Send temp data back
                c64_interface_sync();
            }
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
    else if (filename[0] == 'I')    // Initialize
    {
        // Check drive number
        if (isdigit(filename[1]) && filename[1] != '0')
        {
            return false;
        }
    }
    else if (disk_last_error)
    {
        status = disk_last_error;
    }

    disk_last_error = DISK_STATUS_OK;

    u32 len = disk_write_status(channel, status, track, sector);
    channel->buf[len++] = (u8)'\r';
    channel->buf_len = len;
    channel->buf_ptr = 0;
    channel->buf2_ptr = 0;

    return true;
}

static u8 disk_handle_open_dir(DISK_CHANNEL *channel)
{
    if (channel->number)    // Channel 1-14
    {
        disk_open_dir_read(channel);
        disk_read_data(channel, NULL, 0);
        if (!disk_bytes_left(channel))
        {
            disk_last_error = DISK_STATUS_OK;
            return CMD_END_OF_FILE;
        }
    }
    else                    // Channel 0
    {
        if (!disk_parse_dir(channel))
        {
            return CMD_NO_DRIVE;    // Try serial device (if any)
        }

        u8 *ptr = channel->buf;
        disk_put_dir_header(channel, &ptr);
        channel->buf_len = ptr - channel->buf;
        channel->buf_ptr = 0;
        channel->buf_mode = DISK_BUF_DIR;
    }

    disk_last_error = DISK_STATUS_OK;
    return CMD_NONE;
}

static u8 disk_handle_open_write(DISK_CHANNEL *channel, PARSED_FILENAME *parsed)
{
    D64_DIR_ENTRY *existing = disk_find_file(channel, parsed->name, 0);
    if ((existing && (!parsed->overwrite ||
                      !d64_is_file_type(existing, parsed->type))) ||
        (!existing && parsed->wildcard))
    {
        disk_last_error = DISK_STATUS_EXISTS;
        return CMD_NONE;
    }

    disk_last_error = DISK_STATUS_OK;
    c64_send_command(CMD_WAIT_SYNC);
    c64_interface(false);
    disk_receive_data(channel->buf);    // Save temp data

    u8 cmd;
    if (disk_create_file(channel, parsed->name, parsed->type, existing))
    {
        channel->buf_len = 0;
        channel->buf_ptr = 0;
        channel->buf_mode = DISK_BUF_SAVE;
        cmd = CMD_NONE;
    }
    else
    {
        cmd = CMD_DISK_ERROR;
    }

    disk_send_data(channel->buf);   // Send temp data back
    c64_interface_sync();

    return cmd;
}

static u8 disk_handle_open_read(DISK_CHANNEL *channel, PARSED_FILENAME *parsed)
{
    if (!disk_open_read(channel, parsed->name, parsed->type))
    {
        channel->buf_len = 0;
        channel->buf_mode = DISK_BUF_USE;
        return CMD_NONE;
    }

    disk_read_data(channel, NULL, 0);
    if (!disk_bytes_left(channel))
    {
        return CMD_END_OF_FILE;
    }

    return CMD_NONE;
}

static u8 disk_handle_open_prg(DISK_CHANNEL *channel, char *filename)
{
    PARSED_FILENAME parsed;
    disk_parse_filename(filename, &parsed);

    if (parsed.drive)
    {
        return CMD_NO_DRIVE;    // Try serial device (if any)
    }

    if (parsed.type == D64_FILE_REL)
    {
        return CMD_DISK_ERROR;  // Not supported yet
    }

    if (parsed.mode == 'W')
    {
        return disk_handle_open_write(channel, &parsed);
    }

    return disk_handle_open_read(channel, &parsed);
}

static u8 disk_hand_open_buffer(DISK_CHANNEL *channel)
{
    channel->buf_len = sizeof(channel->buf);
    channel->buf_ptr = 0;
    channel->buf_mode = DISK_BUF_USE;
    return CMD_NONE;
}

static u8 disk_handle_open(DISK_CHANNEL *channel)
{
    char *filename = channel->filename;

    if (channel->number == 15)      // Command channel
    {
        if (!disk_handle_command(channel, filename))
        {
            return CMD_NO_DRIVE;    // Try serial device (if any)
        }

        return CMD_NONE;
    }
    else if (filename[0] == '$')    // Directory
    {
        channel->filename_dir = filename + 1;
        return disk_handle_open_dir(channel);
    }
    else if (filename[0] == '#')    // Buffer
    {
        return disk_hand_open_buffer(channel);
    }

    return disk_handle_open_prg(channel, filename);
}

static u8 disk_handle_close(DISK_CHANNEL *channel, DISK_CHANNEL *channels)
{
    if (channel->number == 15)  // Command channel
    {
        disk_init_all_channels(channel->d64.image, channels);
    }
    else
    {
        disk_close_channel(channel);
    }

    return CMD_NONE;
}

static u8 disk_handle_send_byte(DISK_CHANNEL *channel)
{
    if (!channel)
    {
        return CMD_DISK_ERROR;
    }

    if (channel->number == 15 && !disk_bytes_left(channel))
    {
        disk_handle_command(channel, "-");  // Write disk status to buffer
    }

    u8 data;
    if (!disk_read_data(channel, &data, 1))
    {
        return CMD_DISK_ERROR;
    }

    *KFF_BUF = data;    // Send the byte

    if (!disk_bytes_left(channel))
    {
        if (channel->buf_mode == DISK_BUF_DIR)
        {
            u8 *ptr = channel->buf;
            if (!disk_put_dir_entry(channel, &ptr))
            {
                disk_put_dir_footer(channel, &ptr);
                channel->buf_mode = DISK_BUF_USE;
            }

            channel->buf_len = ptr - channel->buf;
            channel->buf_ptr = 0;
            return CMD_NONE;
        }

        return CMD_END_OF_FILE;
    }

    return CMD_NONE;
}

static u8 disk_handle_unlisten(DISK_CHANNEL *channel)
{
    if (!channel || channel->number != 15 || !channel->buf2_ptr)
    {
        return CMD_NONE;
    }

    // Null terminate "filename"
    if (channel->buf2[channel->buf2_ptr - 1] == (u8)'\r')
    {
        channel->buf2[--channel->buf2_ptr] = 0;
    }
    else
    {
        channel->buf2[channel->buf2_ptr] = 0;
    }

    disk_handle_command(channel, channel->filename);
    return CMD_NONE;
}

static u8 disk_handle_receive_byte(DISK_CHANNEL *channel)
{
    if (!channel)
    {
        return CMD_DISK_ERROR;
    }

    u8 data = c64_receive_byte();

    if (channel->number == 15)
    {
        channel->buf2[channel->buf2_ptr++] = data;
        return CMD_NONE;
    }

    if (channel->buf_mode != DISK_BUF_USE &&
        channel->buf_mode != DISK_BUF_SAVE)
    {
        return CMD_DISK_ERROR;
    }

    channel->buf[channel->buf_ptr++] = data;
    if (channel->buf_ptr >= sizeof(channel->buf))   // Check if buffer is full
    {
        channel->buf_ptr = 0;

        if (channel->buf_mode == DISK_BUF_SAVE)
        {
            c64_send_command(CMD_WAIT_SYNC);
            c64_interface(false);
            disk_receive_data(channel->buf2);   // Save temp data

            disk_write_data(channel, channel->buf, sizeof(channel->buf));

            disk_send_data(channel->buf2);  // Send temp data back
            c64_interface_sync();
        }
    }

    return CMD_NONE;
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

    while (size--)
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

static u8 disk_send_command(u8 cmd, DISK_CHANNEL*channels)
{
    c64_set_command(cmd);

    u32 led_status = STATUS_LED_OFF;
    for (u32 i=0; i<15; i++)
    {
        DISK_CHANNEL *channel = channels + i;
        if (channel->buf_mode != DISK_BUF_USE || channel->buf_len)
        {
            // LED on if a file is open
            led_status = STATUS_LED_ON;
            break;
        }
    }
    timer_start_ms(175);

    u8 reply;
    while (!c64_get_reply(cmd, &reply))
    {
        if (timer_elapsed())
        {
            if (disk_last_error > DISK_STATUS_SCRATCHED &&
                disk_last_error < DISK_STATUS_INIT)
            {
                // Blink if an error has occurred
                led_status = led_status == STATUS_LED_OFF ?
                    STATUS_LED_ON : STATUS_LED_OFF;
            }

            C64_CRT_CONTROL(led_status);
        }
    }

    return reply;
}

static void disk_loop(void)
{
    D64_IMAGE *image = &d64_state.image;    // Reuse memory from menu
    DISK_CHANNEL*channels = (DISK_CHANNEL *)(crt_ram_buf + 0x100);
    DISK_CHANNEL *channel, *talk = NULL, *listen = NULL;
    memset(channels, 0, sizeof(DISK_CHANNEL) * 16);
    disk_init_all_channels(image, channels);

    disk_last_error = DISK_STATUS_INIT;

    // BASIC commands to run are placed in dat_buffer
    u8 cmd = CMD_MOUNT_DISK;
    while (true)
    {
        u8 reply = disk_send_command(cmd, channels);
        cmd = CMD_NONE;

        switch (reply)
        {
            case REPLY_OK:
                break;

            case REPLY_LOAD:
                // Channel 0 will be used as load buffer - just as on 1541
                channel = channels + 0;
                disk_receive_filename(channel->filename);
                dbg("Got LOAD command for: %s", channel->filename);
                cmd = disk_handle_load(channel);
                break;

            case REPLY_SAVE:
                // Channel 1 will be used as save buffer - just as on 1541
                channel = channels + 1;
                disk_receive_filename(channel->filename);
                dbg("Got SAVE command for: %s", channel->filename);
                cmd = disk_handle_save(channel);
                break;

            case REPLY_OPEN:
                channel = disk_receive_channel(channels);
                disk_receive_filename(channel->filename);
                dbg("Got OPEN command for channel %u for: %s",
                    channel->number, channel->filename);
                cmd = disk_handle_open(channel);
                break;

            case REPLY_CLOSE:
                channel = disk_receive_channel(channels);
                dbg("Got CLOSE command for channel %u", channel->number);
                cmd = disk_handle_close(channel, channels);
                break;

            case REPLY_TALK:
                talk = disk_receive_channel(channels);
                break;

            case REPLY_UNTALK:
                talk = NULL;
                break;

            case REPLY_SEND_BYTE:
                cmd = disk_handle_send_byte(talk);
                break;

            case REPLY_LISTEN:
                listen = disk_receive_channel(channels);
                break;

            case REPLY_UNLISTEN:
                cmd = disk_handle_unlisten(listen);
                listen = NULL;
                break;

            case REPLY_RECEIVE_BYTE:
                cmd = disk_handle_receive_byte(listen);
                break;

            default:
                wrn("Got unknown disk reply: %x", reply);
                break;
        }
    }
}