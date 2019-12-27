/*
 * Copyright (c) 2019 Kim Jørgensen
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
 *
 ******************************************************************************
 * Based on d642prg V2.09
 * Original source (C) Covert Bitops
 * (C)2003/2009 by iAN CooG/HokutoForce^TWT^HVSC
 */
static bool d64_seek_and_read(FIL *file, uint8_t track, uint8_t sector, void *buf, size_t buf_size)
{
    if (!track || track > ARRAY_COUNT(d64_track_offset))
    {
        wrn("Invalid track %d sector %d\n", track, sector);
        return false;
    }

    FSIZE_t offset = (FSIZE_t)(d64_track_offset[track-1] + sector) * 256;
    if (!file_seek(file, offset) ||
        file_read(file, buf, buf_size) != buf_size)
    {
        err("Failed to read track %d sector %d\n", track, sector);
        return false;
    }

    return true;
}

static void d64_rewind(D64_DIR *dir)
{
    memset(dir->visited, 0, sizeof(dir->visited));
    dir->entries[0].next_track = 18;
    dir->entries[0].next_sector = 1;
    dir->entry = &dir->entries[8];
}

static bool d64_open(FIL *file, D64_DIR *dir)
{
    if (!d64_seek_and_read(file, 18, 0, dir->sector, sizeof(dir->sector)))
    {
        dir->diskname[0] = 0;
        return false;
    }

    dir->diskname = (char *)&dir->sector[0x90];
    for (uint8_t c=0; c<23; c++)    // fix 0s in diskname -> a0
    {
        if (dir->diskname[c] == '\x00')
        {
            dir->diskname[c] = '\xa0';
        }
    }
    dir->diskname[23] = 0;  // null terminate

    d64_rewind(dir);
    return true;
}

static bool d64_read_dir(FIL *file, D64_DIR *dir)
{
    while (true)
    {
        D64_DIR_ENTRY *entry = ++dir->entry;
        if (entry > &dir->entries[7])
        {
            entry = dir->entry = dir->entries;
            if (!entry->next_track)
            {
                return false;   // end of dir
            }

            // No support for non-standard long directories that extends from
            // track 18 to other tracks
            if(entry->next_track != 18 || entry->next_sector >= DIRSECTS)
            {
                wrn("Invalid link: %02d;%02d\n", entry->next_track, entry->next_sector);
                return false;
            }

            if (!dir->visited[entry->next_sector])
            {
                dir->visited[entry->next_sector] = true;
            }
            else
            {
                // this sector was already parsed, recursive directory!
                entry->next_track = 0;
                wrn("Recursive dir: %02d\n", entry->next_sector);
                return false;
            }

            if (!d64_seek_and_read(file, entry->next_track, entry->next_sector,
                                   dir->sector, sizeof(dir->sector)))
            {
                return false;
            }
        }

        bool has_filename = false;
        for (uint8_t c=0; c<16; c++)    // fix 0s in filename -> a0
        {
            if (entry->filename[c] == '\x00')
            {
                entry->filename[c] = '\xa0';
            }
            else if (entry->filename[c] != '\xa0')
            {
                has_filename = true;
            }
        }

        entry->ignored[0] = 0;  // null terminate filename

        if(has_filename)        // skip empty entries
        {
            return true;
        }
    }
}

static bool d64_is_valid_prg(D64_DIR *dir)
{
    D64_DIR_ENTRY *entry = dir->entry;
    if ((entry->type & 7) == 2 && entry->blocks && entry->track)
    {
        return true;
    }

    return false;
}

static size_t d64_read_prg(FIL *file, D64_DIR *dir, uint8_t *buf, size_t buf_size)
{
    if (!d64_is_valid_prg(dir)) // Only PRGs are supported
    {
        return 0;
    }

    D64_SECTOR_HEADER header = {dir->entry->track, dir->entry->sector};
    dbg("Reading PRG from track %d sector %d\n", header.next_track, header.next_sector);

    size_t prg_len = 0;
    while (header.next_track)
    {
        if (!d64_seek_and_read(file, header.next_track, header.next_sector,
                               &header, sizeof(header)))
        {
            return 0;
        }

        uint8_t bytes_to_read;
        if (header.next_track)
        {
            bytes_to_read = 254;
        }
        else
        {
            if(!header.next_sector)
            {
                return prg_len;
            }

            bytes_to_read = header.next_sector-1;
        }

        if((prg_len + bytes_to_read) > buf_size)
        {
            wrn("File too big to fit buffer or there is a recursive link\n");
            return 0;
        }

        if (file_read(file, buf, bytes_to_read) != bytes_to_read)
        {
            return 0;
        }

        prg_len += bytes_to_read;
        buf += bytes_to_read;
    }

    return prg_len;
}
