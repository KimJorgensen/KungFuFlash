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
    *(*ptr)++ = (uint8_t)value;
    *(*ptr)++ = (uint8_t)(value >> 8);
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
    const uint16_t start_addr = 0x0401; // Start of BASIC (for the PET)
    const uint16_t link_addr = 0x0101;

    if (!d64_read_disk_header(d64))
    {
        return start_addr;
    }

    put_u16(ptr, link_addr);
    put_u16(ptr, 0);                    // drive number
    put_u8(ptr, 0x12);                  // reverse on
    put_diskname(ptr, d64->diskname);
    put_u8(ptr, 0);                     // end of line

    uint16_t blocks_free = 0;
    d64_read_bam(d64, &blocks_free);

    while (d64_read_dir(d64))
    {
        if (disk_filename_match(d64->entry, filename))
        {
            put_u16(ptr, link_addr);
            put_dir_entry(ptr, d64->entry);
            put_u8(ptr, 0);             // end of line
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

static void disk_parse_filename(char *filename, PARSED_FILENAME *parsed)
{
    char *f_ptr = filename;
    char c, last_c = 0;
    parsed->drive = 0;
    parsed->overwrite = false;
    
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
    parsed->type = 0;
    parsed->mode = 0;

    // Scan for type and mode
    while (*f_ptr)
    {
        if (*f_ptr == ',')
        {
            *f_ptr++ = 0;   // Null terminate filename

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
            f_ptr++;
        }
    }
}

static bool disk_find_file(D64 *d64, const char *filename, uint8_t file_type)
{
    d64_rewind_dir(d64);
    while (d64_read_dir(d64))
    {
        if (file_type && (d64->entry->type & 7) != file_type)
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
            (sector_len = d64_read_file_sector(d64, &d64->sector)))
    {
        memcpy(ptr, d64->sector.data, sector_len);
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
    PARSED_FILENAME parsed_filename;
    disk_parse_filename(filename, &parsed_filename);

    if (parsed_filename.drive)
    {
        // Try serial device (if any)
        c64_send_reply(REPLY_NOT_FOUND);
        return;
    }

    if (!parsed_filename.type)
    {
        parsed_filename.type = D64_FILE_PRG;
    }
    else if (parsed_filename.type == D64_FILE_REL)
    {
        // Not supported
        c64_send_reply(REPLY_NOT_FOUND);
    }

    if (disk_find_file(d64, parsed_filename.name, parsed_filename.type))
    {
        d64_read_file_start(d64, &d64->sector);

        uint16_t prg_size = 0;
        uint16_t start_addr = 0;
        uint8_t sector_len;

        while ((prg_size + D64_SECTOR_DATA_LEN) <= 16*1024 &&
                (sector_len = d64_read_file_sector(d64, &d64->sector)))
        {
            if (!prg_size)
            {
                if (sector_len > 2)
                {
                    sector_len -= 2;
                    start_addr = *((uint16_t *)d64->sector.data);
                    memcpy(ptr, d64->sector.data + 2, sector_len);
                }
                else
                {
                    break;
                }
            }
            else
            {
                memcpy(ptr, d64->sector.data, sector_len);
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
    if (filename[0] >= '0' && filename[0] <= '9')
    {
        if (filename[0] != '0')
        {
            // Try serial device (if any)
            c64_send_reply(REPLY_NOT_FOUND);
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

    uint8_t *start_ptr = ptr;
    uint16_t start_addr = disk_create_dir_prg(d64, &ptr, filename);
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
        disk_handle_load_dir(d64, filename + 1, ptr);
    }
    else
    {
        disk_handle_load_prg(d64, filename, ptr);
    }
}

static uint8_t disk_handle_create_file_entry(D64 *d64, char *filename) 
{
    PARSED_FILENAME parsed_filename;

    if(d64->image_type!=D64_IMAGE_D64) 
    {
        return REPLY_NOT_SUPPORTED;
    }
   
    disk_parse_filename(filename, &parsed_filename);
    if (parsed_filename.drive!=0)               // this is a 'single drive system'
    {
        return REPLY_NOT_SUPPORTED;
    }

    if (!parsed_filename.type)
    {
        parsed_filename.type = D64_FILE_PRG;
    }
    else if (parsed_filename.type == D64_FILE_REL)
    {
        // Not supported
        return REPLY_NOT_SUPPORTED;
    }

    bool file_found = disk_find_file(d64, parsed_filename.name, parsed_filename.type);
    
    uint8_t ret_val = 0;

    if(file_found)
    {
        if(!parsed_filename.overwrite) 
        {
            // File already exists, overwrite not selected
            ret_val = REPLY_SAVE_ERROR;
        } 
        else 
        {
            ret_val = d64_update_file_for_save(d64) ? REPLY_SAVE_OK : REPLY_SAVE_ERROR;
        }
    } 
    else 
    {
        ret_val = d64_create_file_for_save(d64, 
                                           parsed_filename.name, 
                                           parsed_filename.type) ? REPLY_SAVE_OK : REPLY_SAVE_ERROR;
    }
    // d64->entry contains the file's first track/sector position
    return ret_val;
}

static uint8_t disk_handle_save_data(D64 *d64, DISK_CHANNEL *channel, uint8_t *blocks)
{
    uint8_t *receive_buffer = (uint8_t *)&crt_ram_buf[SAVE_BUFFER_OFFSET];
    uint8_t recv_size = 0;

    if( !d64_read_disk_header(d64) )
    {
        c64_receive_byte(); // let's read and discard the first batch of bytes. Error is error..
        c64_interface(false);
        return 255;       
    } 
    
    while((recv_size = c64_receive_byte()) != 0)  
    {
        if((uint16_t)recv_size + channel->bytes_ptr > D64_SECTOR_DATA_LEN) 
        {
            c64_interface(false);
            uint8_t remaining = D64_SECTOR_DATA_LEN - channel->bytes_ptr;
            if(remaining>0)
            {
                memcpy(&channel->sector.data[channel->bytes_ptr],
                       receive_buffer,
                       remaining);
            }

            if(!d64_write_interim_block(d64, 
                                        &channel->sector, 
                                        channel->track_pos, 
                                        channel->sector_pos))
            {
                return 255;
            }

            c64_interface(true);
            
            memcpy(&channel->sector.data[0],
                    receive_buffer + remaining,
                    recv_size - remaining);
            channel->bytes_ptr = recv_size - remaining;
            channel->track_pos = channel->sector.next_track;
            channel->sector_pos = channel->sector.next_sector;
            (*blocks)++;
        }
        else
        {
            memcpy(&channel->sector.data[channel->bytes_ptr], 
                   receive_buffer,
                   recv_size);
            channel->bytes_ptr += recv_size;
        }
        c64_send_data("DONE", 4);
        c64_send_reply(recv_size);
    }
    
    memset(&channel->sector.data[channel->bytes_ptr], 
            0x00, 
            D64_SECTOR_DATA_LEN - channel->bytes_ptr);

    c64_interface(false);
    
    // write last sector's data, update BAM
    if(!d64_write_last_block(d64, &channel->sector, 
                            channel->track_pos, channel->sector_pos, 
                            channel->bytes_ptr))
    {
        return 255;
    }

    (*blocks)++;

    return recv_size;   // normally this is always 0
}

static bool disk_finalize_saved_filename(D64 *d64, char *filename, uint8_t blocks) 
{
    PARSED_FILENAME parsed_filename;
    
    disk_parse_filename(filename, &parsed_filename);
    
    bool file_found = disk_find_file(d64, parsed_filename.name, parsed_filename.type);
    if(!file_found)
        return false;
    return d64_finalize_file_for_save(d64, blocks);
}

static uint8_t disk_read_status(D64 *d64, DISK_CHANNEL *channel)
{
    uint8_t len = 13;

    // TODO: Return last error
    memcpy(channel->sector.data, "00, OK,00,00\r", len);
    return len;
}

static void disk_handle_open(D64 *d64, DISK_CHANNEL *channel, char *filename)
{
    channel->bytes_left = 0;
    channel->bytes_ptr = 0;

    PARSED_FILENAME parsed_filename;
    disk_parse_filename(filename, &parsed_filename);

    if (parsed_filename.drive || parsed_filename.type == D64_FILE_REL ||
        parsed_filename.mode == 'W')
    {
        // Not supported
        c64_send_reply(REPLY_NOT_FOUND);
        return;
    }

    if (channel->number == 15)
    {
        channel->bytes_left = disk_read_status(d64, channel);
    }
    else if (disk_find_file(d64, parsed_filename.name, parsed_filename.type))
    {
        d64_read_file_start(d64, &channel->sector);
        channel->bytes_left = d64_read_file_sector(d64, &channel->sector);
    }
    else
    {
        c64_send_reply(REPLY_NOT_FOUND);
        return;
    }

    if (channel->bytes_left)
    {
        c64_send_reply(REPLY_READ_DONE);
    }
    else
    {
        c64_send_reply(REPLY_END_OF_FILE);
    }
}

static void disk_close_channel(DISK_CHANNEL *channel)
{
    channel->bytes_left = 0;
    channel->bytes_ptr = 0;
    channel->sector.next_track = 0;
    channel->sector.next_sector = 0;
}

static void disk_close_all_channels(DISK_CHANNEL *channels)
{
    for (uint8_t i=0; i<16; i++)
    {
        DISK_CHANNEL *channel = channels + i;
        disk_close_channel(channel);
        channel->number = i;
    }
}

static void disk_handle_close(D64 *d64, DISK_CHANNEL *channels, DISK_CHANNEL *channel)
{
    if (channel->number == 15)
    {
        disk_close_all_channels(channels);
    }
    else
    {
        disk_close_channel(channel);
    }

    c64_send_reply(REPLY_OK);
}

static void disk_handle_get_byte(D64 *d64, DISK_CHANNEL *channel)
{
    if (!channel || !channel->bytes_left)
    {
        c64_send_reply(REPLY_READ_ERROR);
        return;
    }

    uint8_t data = channel->sector.data[channel->bytes_ptr++];
    channel->bytes_left--;
    bool end_of_file = false;

    if (!channel->bytes_left)
    {
        if (channel->number == 15)
        {
            channel->bytes_left = disk_read_status(d64, channel);
            channel->bytes_ptr = 0;
            end_of_file = true;
        }
        else
        {
            channel->bytes_left = d64_read_file_sector(d64, &channel->sector);
            channel->bytes_ptr = 0;
            end_of_file = channel->bytes_left == 0;
        }
    }

    if (end_of_file)
    {
        c64_send_reply(REPLY_END_OF_FILE);
    }
    else
    {
        c64_send_reply(REPLY_READ_DONE);
    }

    c64_send_data(&data, 1);
}

static DISK_CHANNEL *disk_get_channel(DISK_CHANNEL *channels)
{
    uint8_t channel_number = c64_receive_byte();
    channel_number &= 0x0f;

    DISK_CHANNEL *channel = channels + channel_number;
    return channel;
}

static void disk_loop(void)
{
    D64 *d64 = &d64_state.d64;  // Reuse memory from menu
    DISK_CHANNEL *channel = 0, *channels = (DISK_CHANNEL *)crt_ram_banks[1];
    disk_close_all_channels(channels);

    uint8_t *ptr = crt_banks[1];
    uint8_t file_len;
    char filename[42] = {};

    while (true)
    {
        uint8_t command = c64_receive_command();
        switch (command)
        {
            case CMD_NONE:
                break;

            case CMD_LOAD:
                file_len = c64_receive_byte();
                c64_receive_data(filename, file_len);
                filename[file_len] = 0;

                dbg("Got LOAD command for: %s\n", filename);
                disk_handle_load(d64, filename, ptr);
                break;

            case CMD_LOAD_NEXT_BANK:
                dbg("Got LOAD NEXT BANK command\n");
                disk_handle_load_next_bank(d64, ptr);
                break;

            case CMD_OPEN:
                channel = disk_get_channel(channels);
                file_len = c64_receive_byte();
                c64_receive_data(filename, file_len);
                filename[file_len] = 0;

                dbg("Got OPEN command for channel %d for: %s\n", channel->number, filename);
                disk_handle_open(d64, channel, filename);
                break;

            case CMD_CLOSE:
                channel = disk_get_channel(channels);
                dbg("Got CLOSE command for channel %d\n", channel->number);
                disk_handle_close(d64, channels, channel);
                break;

            case CMD_TALK:
                channel = disk_get_channel(channels);
                c64_send_reply(REPLY_OK);
                break;

            case CMD_UNTALK:
                channel = 0;
                c64_send_reply(REPLY_OK);
                break;

            case CMD_GET_BYTE:
                disk_handle_get_byte(d64, channel);
                break;

            case CMD_SAVE:
                file_len = c64_receive_byte();
                c64_receive_data(filename, file_len);
                filename[file_len] = 0;
                channel = channels + 1;                 // channel 1 will be used as save buffer - just as on 1541
                channel->bytes_ptr = 2;
                c64_receive_data(channel->sector.data, 2);
                dbg("Got SAVE command for: %s\n", filename);
                c64_interface(false);
                uint8_t ret_val = disk_handle_create_file_entry(d64, filename);
                channel->track_pos = d64->entry->track;
                channel->sector_pos = d64->entry->sector;
                c64_interface(true);
                c64_send_data("DONE", 4);
                c64_send_reply(ret_val);
                break;

            case CMD_SAVE_BUFFER: ;
                uint8_t blocks = 0;
                ret_val = disk_handle_save_data(d64, channel, &blocks); // returns always with c64 i/f off
                if(ret_val == 0)
                {
                    ret_val = (disk_finalize_saved_filename(d64, filename, blocks)) ? 0 : REPLY_SAVE_ERROR;
                }
                c64_interface(true);
                c64_send_data("DONE", 4);
                c64_send_reply(ret_val);
                disk_close_channel(channel);
                channel->number = 1;
                break;

            default:
                wrn("Got unknown disk command: %02x\n", command);
                c64_send_reply(REPLY_OK);
                break;
        }
    }
}