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

static void fs_format_diskname(char *buf, const char *filename)
{
    for (int i=0; i<16; i++)
    {
        char c = *filename;
        if (!c)
        {
            c = '\xa0';
        }
        else
        {
            c = ff_wtoupper(c);
            if (c == '_')
            {
                c = 0xa4;
            }

            filename++;
        }

        buf[i] = c;
    }
}

static char * fs_get_diskname(DISK_CHANNEL *channel)
{
    u8 disk_id = 0;
    dir_current(dat_file.path, sizeof(dat_file.path));
    char *path = dat_file.path;

    char *ptr = path;
    while (*ptr)
    {
        if (*ptr++ == '/')
        {
            path = ptr;
            disk_id++;
        }
    }

    if (!*path)
    {
        filesystem_getlabel(path);
        disk_id = 0;
    }

    // Reuse memory from D64
    char *diskname = channel->d64.image->d64_header.diskname;
    fs_format_diskname(diskname, path);
    sprint(diskname + 18, "%2X 2A", disk_id);

    return diskname;
}

static inline void fs_rewind_dir(DISK_CHANNEL *channel)
{
    dir_open(&channel->dir, NULL);
}

static u16 fs_truncate_blocks(u32 blocks)
{
    return blocks > 0xffff ? 0xffff : blocks;
}

static u16 fs_calc_blocks(FSIZE_t size)
{
    u32 blocks = (size / 254) + 1;
    return fs_truncate_blocks(blocks);
}

static D64_DIR_ENTRY * fs_read_dir(DISK_CHANNEL *channel)
{
    FILINFO file_info;
    if (!dir_read(&channel->dir, &file_info) || !file_info.fname[0])
    {
        return NULL;
    }

    // Reuse memory from D64
    D64_DIR_ENTRY *entry = &channel->d64.dir.entries[0];

    if (file_info.fattrib & AM_DIR)
    {
        entry->type = D64_FILE_DIR | D64_FILE_NO_SPLAT;
        entry->blocks = 0;
    }
    else
    {
        entry->type = D64_FILE_PRG | D64_FILE_NO_SPLAT;
        entry->blocks = fs_calc_blocks(file_info.fsize);
    }

    const char *filename = basic_get_filename(&file_info);
    d64_pad_filename(entry->filename, filename);
    entry->ignored[0] = 0;  // null terminate filename

    return entry;
}

static u16 fs_get_blocks_free(void)
{
    u32 blocks = filesystem_getfree() * 2;
    return fs_truncate_blocks(blocks);
}

static char * fs_get_filename(D64_DIR_ENTRY *entry)
{
    char *ptr = entry->filename;
    while (*ptr)
    {
        if (*ptr == '\xa0')
        {
            *ptr = 0;
            break;
        }
        ptr++;
    }

    return entry->filename;
}

static void fs_open_file_read(DISK_CHANNEL *channel, D64_DIR_ENTRY *entry)
{
    char *filename = fs_get_filename(entry);
    file_open(&channel->file, filename, FA_READ);
}

static inline size_t fs_read_data(DISK_CHANNEL *channel, u8 *buf, size_t buf_size)
{
    return file_read(&channel->file, buf, buf_size);
}

static inline bool fs_bytes_left(DISK_CHANNEL *channel)
{
    return !f_eof(&channel->file);
}

static bool fs_create_file(DISK_CHANNEL *channel, const char *filename,
                           u8 file_type, D64_DIR_ENTRY *existing_file)
{
    u8 mode = FA_WRITE|FA_CREATE_NEW;
    if (existing_file)
    {
        mode = FA_WRITE|FA_CREATE_ALWAYS;
        filename = fs_get_filename(existing_file);
    }

    return file_open(&channel->file, filename, mode);
}

static size_t fs_write_data(DISK_CHANNEL *channel, u8 *buf,
                            size_t buf_size)
{
    size_t result = file_write(&channel->file, buf, buf_size);
    if (!file_sync(&channel->file))
    {
        result = 0;
    }

    return result;
}

static inline bool fs_write_finalize(DISK_CHANNEL *channel)
{
    return file_close(&channel->file);
}

static bool fs_delete_file(DISK_CHANNEL *channel, D64_DIR_ENTRY *entry)
{
    char *filename = fs_get_filename(entry);
    return file_delete(filename);
}

static bool fs_dir_up(void)
{
    if (dat_file.disk.mode)
    {
        dat_file.disk.mode = DISK_MODE_FS;
        return true;
    }

    return dir_change("..");
}

static bool fs_dir_change(const char *path)
{
    if (dir_change(path))
    {
        dat_file.disk.mode = DISK_MODE_FS;
        return true;
    }

    return false;
}

static bool fs_change(DISK_CHANNEL *channel, const char *filename)
{
    if (*filename == '_')
    {
        return fs_dir_up();
    }

    if (*filename == '/')
    {
        return fs_dir_change(filename);
    }

    FILINFO file_info;
    if (!file_stat(filename, &file_info))
    {
        return false;
    }

    u8 file_type = get_file_type(&file_info);
    if (file_type == FILE_DIR)
    {
        return fs_dir_change(filename);
    }

    if (file_type != FILE_D64)
    {
        return false;
    }

    if (!d64_open(channel->d64.image, filename))
    {
        return false;
    }

    dat_file.disk.mode = DISK_MODE_D64;
    return true;
}

static bool fs_change_dir(DISK_CHANNEL *channel, char *path)
{
    while (*path)
    {
        char *filename = path;
        while (*path)
        {
            if (*path == '/')
            {
                if (*(path + 1) == '/')
                {
                    path++;
                }
                break;
            }
            else if (*path == ':')
            {
                break;
            }

            path++;
        }

        char end = *path;
        if (filename < path)
        {
            *path = 0;
            if (!fs_change(channel, filename))
            {
                return false;
            }
        }

        if (!end)
        {
            break;
        }
        path++;
    }

    return true;
}
