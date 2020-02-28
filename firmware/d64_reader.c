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
 *
 ******************************************************************************
 * Based on d642prg V2.09
 * Original source (C) Covert Bitops
 * (C)2003/2009 by iAN CooG/HokutoForce^TWT^HVSC
 */

static bool d64_read_next_sector(D64 *d64)
{
    uint8_t track = d64->sector.next_track;
    uint8_t sector = d64->sector.next_sector;

    if (!track || track > ARRAY_COUNT(d64_track_offset))
    {
        wrn("Invalid track %d sector %d\n", track, sector);
        return false;
    }

    FSIZE_t offset = (FSIZE_t)(d64_track_offset[track-1] + sector) * 256;
    if (!file_seek(&d64->file, offset) ||
        file_read(&d64->file, &d64->sector, sizeof(d64->sector)) != sizeof(d64->sector))
    {
        err("Failed to read track %d sector %d\n", track, sector);
        return false;
    }

    return true;
}

static void d64_rewind_dir(D64 *d64)
{
    d64->visited_dir_sectors = 0;
    d64->sector.next_track = 18;
    d64->sector.next_sector = 1;
    d64->entry = &d64->entries[8];
}

static bool d64_read_bam(D64 *d64)
{
    d64->sector.next_track = 18;
    d64->sector.next_sector = 0;

    if (!d64_read_next_sector(d64))
    {
        d64->sector.next_track = 0;
        return false;
    }

    d64->diskname = d64->bam.diskname;
    d64->diskname[23] = 0;  // null terminate
    d64_rewind_dir(d64);
    return true;
}

static bool d64_open(D64 *d64, const char *filename)
{
    if (!file_open(&d64->file, filename, FA_READ))
    {
        return false;
    }

    if (!d64_validate_size(f_size(&d64->file)))
    {
        file_close(&d64->file);
        return false;
    }

    d64_rewind_dir(d64);
    return true;
}

static bool d64_close(D64 *d64)
{
    return file_close(&d64->file);
}

static uint16_t d64_blocks_free(D64 *d64)
{
    uint16_t blocks_free = 0;
    for (uint8_t i=0; i<ARRAY_COUNT(d64->bam.entries); i++)
    {
        if (i != 17)    // Skip track 18 (directory)
        {
            blocks_free += d64->bam.entries[i].free_sectors;
        }
    }

    return blocks_free;
}

static bool d64_read_dir(D64 *d64)
{
    while (true)
    {
        D64_DIR_ENTRY *entry = ++d64->entry;
        if (entry > &d64->entries[7])
        {
            entry = d64->entry = d64->entries;
            if (!entry->next_track)
            {
                return false;   // end of dir
            }

            // Allow up to 248 dir entries. 144 is max for a standard disk
            if (d64->visited_dir_sectors > 30)
            {
                wrn("Directory too big or there is a recursive link\n");
                return false;
            }
            d64->visited_dir_sectors++;

            if (!d64_read_next_sector(d64))
            {
                return false;
            }
        }

        bool has_filename = false;
        if (entry->filename[0] != '\xa0' && entry->filename[0] != '\x00')
        {
            has_filename = true;
        }

        entry->ignored[0] = 0;              // null terminate filename

        if (has_filename && entry->type)    // skip empty and scratched entries
        {
            return true;
        }
    }
}

static bool d64_is_valid_prg(D64 *d64)
{
    D64_DIR_ENTRY *entry = d64->entry;
    if ((entry->type & 7) == D64_FILE_PRG && entry->track)
    {
        return true;
    }

    return false;
}

static bool d64_read_prg_start(D64 *d64)
{
    if (!d64_is_valid_prg(d64)) // Only PRGs are supported
    {
        return false;
    }

    D64_SECTOR *sector = &d64->sector;

    sector->next_track = d64->entry->track;
    sector->next_sector = d64->entry->sector;
    dbg("Reading PRG from track %d sector %d\n", sector->next_track, sector->next_sector);

    return true;
}

static uint8_t d64_read_prg_sector(D64 *d64, uint8_t *buf)
{
    uint8_t len = 0;
    D64_SECTOR *sector = &d64->sector;

    if (!sector->next_track || !d64_read_next_sector(d64))
    {
        return len;
    }

    if (sector->next_track)
    {
        len = D64_SECTOR_DATA_LEN;
    }
    else
    {
        if (!sector->next_sector)
        {
            return len;
        }

        len = sector->next_sector - 1;
    }

    memcpy(buf, sector->data, len);
    return len;
}

static size_t d64_read_prg(D64 *d64, uint8_t *buf, size_t buf_size)
{
    if (!d64_read_prg_start(d64))
    {
        return 0;
    }

    size_t prg_len = 0;
    uint8_t sector_len = 0;

    while ((prg_len + D64_SECTOR_DATA_LEN) <= buf_size &&
           (sector_len = d64_read_prg_sector(d64, buf)))
    {
        prg_len += sector_len;
        buf += sector_len;
    }

    if ((prg_len + D64_SECTOR_DATA_LEN) > buf_size)
    {
        wrn("File too big to fit buffer or there is a recursive link\n");
        return 0;
    }

    return prg_len;
}
