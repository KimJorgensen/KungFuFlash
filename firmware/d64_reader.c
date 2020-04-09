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

static FSIZE_t d64_get_offset(D64 *d64, uint8_t track, uint8_t sector)
{
    FSIZE_t offset = 0;
    uint8_t track_on_side = track;

    if (track > 35 && d64->image_type == D64_IMAGE_D71)
    {
        offset = (FSIZE_t)d64_track_offset[35] * 256;
        track_on_side -= 35;
    }

    if (!track_on_side || track_on_side > ARRAY_COUNT(d64_track_offset))
    {
        wrn("Invalid track %d sector %d\n", track, sector);
        return false;
    }

    offset += (FSIZE_t)(d64_track_offset[track_on_side-1] + sector) * 256;
    return offset;
}

static FSIZE_t d81_get_offset(D64 *d64, uint8_t track, uint8_t sector)
{
    if (!track || track > 80)
    {
        wrn("Invalid track %d sector %d\n", track, sector);
        return false;
    }

    FSIZE_t offset = (FSIZE_t)(((track - 1) * 40) + sector) * 256;
    return offset;
}

static bool d64_read_next_sector(D64 *d64, D64_SECTOR *d64_sector)
{
    uint8_t track = d64_sector->next_track;
    uint8_t sector = d64_sector->next_sector;

    FSIZE_t offset;
    if (d64->image_type == D64_IMAGE_D81)
    {
        offset = d81_get_offset(d64, track, sector);
    }
    else
    {
        offset = d64_get_offset(d64, track, sector);
    }

    if (!file_seek(&d64->file, offset) ||
        file_read(&d64->file, d64_sector, sizeof(D64_SECTOR)) != sizeof(D64_SECTOR))
    {
        err("Failed to read track %d sector %d\n", track, sector);
        d64_sector->next_track = 0;
        return false;
    }

    return true;
}

static void d64_set_next_sector(D64 *d64, uint8_t track, uint8_t sector)
{
    d64->sector.next_track = track;
    d64->sector.next_sector = sector;
}

static bool d64_read_sector(D64 *d64, uint8_t track, uint8_t sector)
{
    d64_set_next_sector(d64, track, sector);
    return d64_read_next_sector(d64, &d64->sector);
}

static void d64_rewind_dir(D64 *d64)
{
    if (d64->image_type == D64_IMAGE_D81)
    {
        d64_set_next_sector(d64, 40, 3);
    }
    else
    {
        d64_set_next_sector(d64, 18, 1);
    }

    d64->visited_dir_sectors = 0;
    d64->entry = &d64->entries[8];
}

static bool d64_read_disk_header(D64 *d64)
{
    if (d64->image_type == D64_IMAGE_D81)
    {
        d64_set_next_sector(d64, 40, 0);
    }
    else
    {
        d64_set_next_sector(d64, 18, 0);
    }

    if (!d64_read_next_sector(d64, &d64->sector))
    {
        return false;
    }

    if (d64->image_type == D64_IMAGE_D81)
    {
        d64->diskname = d64->d81_header.diskname;
    }
    else
    {
        d64->diskname = d64->d64_header.diskname;
    }

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

    d64->image_type = d64_image_type(f_size(&d64->file));
    if (d64->image_type == D64_IMAGE_UNKNOWN)
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

static uint16_t d64_calc_blocks_free(D64 *d64)
{
    uint16_t blocks_free = 0;
    for (uint8_t i=0; i<ARRAY_COUNT(d64->d64_header.entries); i++)
    {
        if (i != 17)    // Skip track 18 (header/directory)
        {
            blocks_free += d64->d64_header.entries[i].free_sectors;
        }
    }

    if (d64->image_type == D64_IMAGE_D71 && d64->d64_header.double_sided)
    {
        for (uint8_t i=0; i<ARRAY_COUNT(d64->d64_header.free_sectors_36_70); i++)
        {
            if (i != 17)    // Skip track 53 (bam)
            {
                blocks_free += d64->d64_header.free_sectors_36_70[i];
            }
        }
    }

    return blocks_free;
}

static uint16_t d81_calc_blocks_free(D64 *d64, bool side2)
{
    uint16_t blocks_free = 0;
    for (uint8_t i=0; i<ARRAY_COUNT(d64->d81_bam.entries); i++)
    {
        if (i != 39 || side2)   // Skip track 40 (header/bam/directory)
        {
            blocks_free += d64->d81_bam.entries[i].free_sectors;
        }
    }

    return blocks_free;
}

static bool d64_read_bam(D64 *d64, uint16_t *blocks_free)
{
    if (d64->image_type == D64_IMAGE_D81)
    {
        if (!d64_read_sector(d64, 40, 1))
        {
            return false;
        }

        *blocks_free = d81_calc_blocks_free(d64, false);

        if (!d64_read_sector(d64, 40, 2))
        {
            return false;
        }

        *blocks_free += d81_calc_blocks_free(d64, true);
        d64_rewind_dir(d64);
    }
    else
    {
        // d64_read_disk_header must be called before this method
        *blocks_free = d64_calc_blocks_free(d64);
    }

    return true;
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

            // Allow up to 320 dir entries. 144 is max for a standard D64/D71 disk
            if (d64->visited_dir_sectors >= 40)
            {
                wrn("Directory too big or there is a recursive link\n");
                return false;
            }
            d64->visited_dir_sectors++;

            if (!d64_read_next_sector(d64, &d64->sector))
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

static void d64_read_file_start(D64 *d64, D64_SECTOR *sector)
{
    sector->next_track = d64->entry->track;
    sector->next_sector = d64->entry->sector;
    dbg("Reading file from track %d sector %d\n", sector->next_track, sector->next_sector);
}

static uint8_t d64_read_file_sector(D64 *d64, D64_SECTOR *sector)
{
    uint8_t len = 0;

    if (!sector->next_track || !d64_read_next_sector(d64, sector))
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

    return len;
}

static size_t d64_read_prg(D64 *d64, uint8_t *buf, size_t buf_size)
{
    if (!d64_is_valid_prg(d64)) // Only PRGs are supported
    {
        return 0;
    }

    d64_read_file_start(d64, &d64->sector);

    size_t prg_len = 0;
    uint8_t sector_len = 0;

    while ((prg_len + D64_SECTOR_DATA_LEN) <= buf_size &&
           (sector_len = d64_read_file_sector(d64, &d64->sector)))
    {
        memcpy(buf, d64->sector.data, sector_len);
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
