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
 *
 ******************************************************************************
 * Based on d642prg V2.09
 * Original source (C) Covert Bitops
 * (C)2003/2009 by iAN CooG/HokutoForce^TWT^HVSC
 */

static inline bool d64_sync(D64_IMAGE *image)
{
    return file_sync(&image->file);
}

static inline bool d64_close(D64_IMAGE *image)
{
    return file_close(&image->file);
}

static FSIZE_t d64_get_offset(D64_IMAGE *image, D64_TS ts)
{
    FSIZE_t offset = 0;
    u8 track_on_side = ts.track;

    if (track_on_side > D64_TRACKS && image->type == D64_TYPE_D71)
    {
        offset = (FSIZE_t)d64_track_offset[D64_TRACKS] * D64_SECTOR_LEN;
        track_on_side -= D64_TRACKS;
    }

    if (!track_on_side || track_on_side > ARRAY_COUNT(d64_track_offset))
    {
        log("Invalid track/sector");
        return -1;
    }

    offset += (FSIZE_t)(d64_track_offset[track_on_side-1] + ts.sector) *
                       D64_SECTOR_LEN;
    return offset;
}

static FSIZE_t d81_get_offset(D64_TS ts)
{
    if (!ts.track || ts.track > D81_TRACKS)
    {
        log("Invalid track/sector");
        return -1;
    }

    return (FSIZE_t)((D81_SECTORS * (ts.track-1)) + ts.sector) *
                    D64_SECTOR_LEN;
}

static bool d64_seek(D64_IMAGE *image, D64_TS ts)
{
    FSIZE_t offset;
    if (image->type == D64_TYPE_D81)
    {
        offset = d81_get_offset(ts);
    }
    else
    {
        offset = d64_get_offset(image, ts);
    }

    if (offset >= f_size(&image->file) || !file_seek(&image->file, offset))
    {
        wrn("Failed to seek to track %u sector %u", ts.track, ts.sector);
        return false;
    }

    return true;
}

static bool d64_seek_read(D64_IMAGE *image, void *buffer, D64_TS ts)
{
    return d64_seek(image, ts) &&
           file_read(&image->file, buffer, D64_SECTOR_LEN) == D64_SECTOR_LEN;
}

static inline bool d64_read_sector(D64 *d64, void *buffer, D64_TS ts)
{
    return d64_seek_read(d64->image, buffer, ts);
}

static bool d64_read_from(D64_IMAGE *image, D64_SECTOR *sector, D64_TS ts)
{
    sector->current = ts;

    if (!d64_seek_read(image, &sector->next, ts))
    {
        sector->next.track = 0;
        return false;
    }

    return true;
}

static inline bool d64_read_next(D64 *d64)
{
    return d64->sector.next.track &&
           d64_read_from(d64->image, &d64->sector, d64->sector.next);
}

static bool d64_seek_write(D64_IMAGE *image, void *buffer, D64_TS ts)
{
    return d64_seek(image, ts) &&
           file_write(&image->file, buffer, D64_SECTOR_LEN) == D64_SECTOR_LEN;
}

static bool d64_write_sector(D64 *d64, void *buffer, D64_TS ts)
{
    return d64_seek_write(d64->image, buffer, ts) && d64_sync(d64->image);
}

static bool d64_write_to(D64 *d64, D64_SECTOR *sector, D64_TS ts)
{
    return d64_seek_write(d64->image, &sector->next, ts);
}

static bool d64_write_current(D64 *d64)
{
    return d64_write_to(d64, &d64->sector, d64->sector.current);
}

static void d64_rewind_dir(D64 *d64)
{
    if (d64->image->type == D64_TYPE_D81)
    {
        d64->sector.next.track = D81_TRACK_DIR;
        d64->sector.next.sector = D81_SECTOR_DIR;
    }
    else
    {
        d64->sector.next.track = D64_TRACK_DIR;
        d64->sector.next.sector = D64_SECTOR_DIR;
    }

    d64->sector_count = 0;
    d64->data_ptr = -1;
}

static bool d64_read_header(D64_IMAGE *image)
{
    D64_TS ts = {D64_TRACK_DIR, D64_SECTOR_HEADER};
    if (image->type == D64_TYPE_D81)
    {
        ts.track = D81_TRACK_DIR;
    }

    if (!d64_read_from(image, &image->header, ts))
    {
        return false;
    }

    if (image->type == D64_TYPE_D64)
    {
        return true;
    }

    if (image->type == D64_TYPE_D71)
    {
        ts.track += D64_TRACKS;
    }
    else    // D64_TYPE_D81
    {
        ts.sector++;
    }

    if (!d64_read_from(image, &image->bam, ts))
    {
        return false;
    }

    if (image->type == D64_TYPE_D81)
    {
        ts.sector++;
        return d64_read_from(image, &image->bam2, ts);
    }

    return true;
}

static bool d64_write_bam(D64 *d64)
{
    if (d64->image->type != D64_TYPE_D64)
    {
        D64_SECTOR *bam = &d64->image->bam;
        if (!d64_write_to(d64, bam, bam->current))
        {
            return false;
        }
    }

    if (d64->image->type == D64_TYPE_D81)
    {
        D64_SECTOR *bam = &d64->image->bam2;
        return d64_write_to(d64, bam, bam->current);
    }

    D64_SECTOR *header = &d64->image->header;
    return d64_write_to(d64, header, header->current);
}

static char * d64_get_diskname(D64 *d64)
{
    if (d64->image->type == D64_TYPE_D81)
    {
        return d64->image->d81_header.diskname;
    }

    return d64->image->d64_header.diskname;
}

static u16 d64_calc_blocks_free(D64_IMAGE *image)
{
    D64_HEADER_SECTOR *header = &image->d64_header;

    u16 blocks_free = 0;
    for (u8 i=0; i<ARRAY_COUNT(header->entries); i++)
    {
        if (i != (D64_TRACK_DIR-1))     // Skip track 18 (header/directory)
        {
            blocks_free += header->entries[i].free_sectors;
        }
    }

    if (image->type == D64_TYPE_D71 && header->double_sided)
    {
        for (u8 i=0; i<ARRAY_COUNT(header->free_sectors_36_70); i++)
        {
            if (i != (D64_TRACK_DIR-1)) // Skip track 53 (bam)
            {
                blocks_free += header->free_sectors_36_70[i];
            }
        }
    }

    return blocks_free;
}

static u16 d81_calc_blocks_free(D81_BAM_SECTOR *bam, bool side2)
{
    u16 blocks_free = 0;
    for (u8 i=0; i<ARRAY_COUNT(bam->entries); i++)
    {
        // Skip track 40 (header/bam/directory)
        if (i != (D81_TRACK_DIR-1) || side2)
        {
            blocks_free += bam->entries[i].free_sectors;
        }
    }

    return blocks_free;
}

static u16 d64_get_blocks_free(D64 *d64)
{
    D64_IMAGE *image = d64->image;
    if (d64->image->type == D64_TYPE_D81)
    {
        u16 blocks_free = d81_calc_blocks_free(&image->d81_bam1, false);
        blocks_free += d81_calc_blocks_free(&image->d81_bam2, true);
        return blocks_free;
    }

    return d64_calc_blocks_free(image);
}

static D64_DIR_ENTRY * d64_read_next_dir(D64 *d64)
{
    if (d64->data_ptr >= (ARRAY_COUNT(d64->dir.entries) - 1))
    {
        // Allow up to 320 dir entries. 144 is max for a standard D64/D71 disk
        if (d64->sector_count >= 40)
        {
            wrn("Directory too big or there is a recursive link");
            return NULL;
        }
        d64->sector_count++;

        if (!d64_read_next(d64))
        {
            return NULL;    // end of dir
        }
        d64->data_ptr = -1;
    }

    return &d64->dir.entries[++d64->data_ptr];
}

static bool d64_is_empty_or_deleted(D64_DIR_ENTRY *entry)
{
    return entry->filename[0] == '\x00' || entry->filename[0] == '\xa0' ||
           entry->type == D64_FILE_DEL;
}

static D64_DIR_ENTRY * d64_read_dir(D64 *d64)
{
    D64_DIR_ENTRY *entry;
    while ((entry = d64_read_next_dir(d64)))
    {
        if (!d64_is_empty_or_deleted(entry))
        {
            entry->ignored[0] = 0;  // null terminate filename
            return entry;
        }
    }

    return NULL;
}

static inline bool d64_is_file_type(D64_DIR_ENTRY *entry, u8 file_type)
{
    return (entry->type & 7) == file_type;
}

static bool d64_is_valid_prg(D64_DIR_ENTRY *entry)
{
    if (d64_is_file_type(entry, D64_FILE_PRG) && entry->start.track)
    {
        return true;
    }

    return false;
}

static void d64_open_read(D64 *d64, D64_TS ts)
{
    dbg("Reading file from track %u sector %u", ts.track, ts.sector);

    d64->sector.next = ts;
    d64->data_len = d64->data_ptr = 0;
}

static void d64_open_dir_read(D64 *d64)
{
    D64_TS ts = {D64_TRACK_DIR, D64_SECTOR_HEADER};
    if (d64->image->type == D64_TYPE_D81)
    {
        ts.track = D81_TRACK_DIR;
    }

    d64_open_read(d64, ts);
}

static void d64_open_file_read(D64 *d64, D64_DIR_ENTRY *entry)
{
    d64_open_read(d64, entry->start);
}

static void d64_open_file_write(D64 *d64, D64_DIR_ENTRY *entry)
{
    dbg("Writing file to track %u sector %u", entry->start.track,
        entry->start.sector);

    d64->sector.current = entry->start;
    d64->data_len = D64_SECTOR_DATA_LEN;
    d64->data_ptr = 0;
}

static u8 d64_bytes_left(D64 *d64)
{
    if (d64->data_ptr < d64->data_len)
    {
        return d64->data_len - d64->data_ptr;
    }

    return 0;
}

static size_t d64_read_data(D64 *d64, u8 *buf, size_t buf_size)
{
    size_t read_bytes = 0;

    u8 bytes_to_copy;
    while (!(bytes_to_copy = d64_bytes_left(d64)) || read_bytes < buf_size)
    {
        if (!bytes_to_copy)
        {
            if (d64_read_next(d64))
            {
                d64->data_ptr = 0;
                if (d64->sector.next.track)
                {
                    d64->data_len = D64_SECTOR_DATA_LEN;
                }
                else if (d64->sector.next.sector)
                {
                    d64->data_len = d64->sector.next.sector - 1;
                }
                else
                {
                    d64->data_len = 0;
                }

                bytes_to_copy = d64->data_len;
            }
            else
            {
                break;  // end of file
            }
        }

        size_t left_in_buffer = buf_size - read_bytes;
        if (bytes_to_copy > left_in_buffer)
        {
            bytes_to_copy = left_in_buffer;
        }

        memcpy(&buf[read_bytes], &d64->sector.data[d64->data_ptr],
               bytes_to_copy);
        d64->data_ptr += bytes_to_copy;
        read_bytes += bytes_to_copy;
    }

    return read_bytes;
}

static size_t d64_read_prg(D64 *d64, D64_DIR_ENTRY *entry, u8 *buf,
                           size_t buf_size)
{
    if (!d64_is_valid_prg(entry)) // Only PRGs are supported
    {
        return 0;
    }

    d64_open_file_read(d64, entry);

    size_t prg_len = d64_read_data(d64, buf, buf_size);
    if (d64_bytes_left(d64))
    {
        wrn("File too big to fit buffer or there is a recursive link");
        return 0;
    }

    return prg_len;
}

static bool d64_header_allocate(D64_BAM_ENTRY *entry, u8 sector)
{
    u8 *bitmap = &entry->data[sector >> 3];
    u8 mask = 1 << (sector & 0x07);

    if (*bitmap & mask)
    {
        *bitmap &= ~mask;
        entry->free_sectors--;
        return true;
    }

    return false;
}

static bool d64_header_deallocate(D64_BAM_ENTRY *entry, u8 sector)
{
    u8 *bitmap = &entry->data[sector >> 3];
    u8 mask = 1 << (sector & 0x07);

    if (!(*bitmap & mask))
    {
        *bitmap |= mask;
        entry->free_sectors++;
        return true;
    }

    return false;
}

static D81_BAM_ENTRY * d81_get_bam_entry(D64 *d64, u8 track)
{
    D81_BAM_SECTOR *bam = &d64->image->d81_bam1;
    if (track > ARRAY_COUNT(bam->entries))
    {
        track -= ARRAY_COUNT(bam->entries);
        bam = &d64->image->d81_bam2;
    }

    return &bam->entries[track-1];
}

static bool d81_allocate(D64 *d64, D64_TS ts)
{
    void *entry = d81_get_bam_entry(d64, ts.track);
    return d64_header_allocate(entry, ts.sector);
}

static bool d81_deallocate(D64 *d64, D64_TS ts)
{
    void *entry = d81_get_bam_entry(d64, ts.track);
    return d64_header_deallocate(entry, ts.sector);
}

static bool d71_allocate_36_70(D64 *d64, D64_TS ts)
{
    u8 track = ts.track - D64_TRACKS;

    D71_BAM_ENTRY *entry = &d64->image->d71_bam.entries[track-1];
    u8 *bitmap = &entry->data[ts.sector >> 3];
    u8 mask = 1 << (ts.sector & 0x07);

    if (*bitmap & mask)
    {
        *bitmap &= ~mask;
        d64->image->d64_header.free_sectors_36_70[track-1]--;
        return true;
    }

    return false;
}

static bool d71_deallocate_36_70(D64 *d64, D64_TS ts)
{
    u8 track = ts.track - D64_TRACKS;

    D71_BAM_ENTRY *entry = &d64->image->d71_bam.entries[track-1];
    u8 *bitmap = &entry->data[ts.sector >> 3];
    u8 mask = 1 << (ts.sector & 0x07);

    if (!(*bitmap & mask))
    {
        *bitmap |= mask;
        d64->image->d64_header.free_sectors_36_70[track-1]++;
        return true;
    }

    return false;
}

static D64_BAM_ENTRY * d64_get_bam_entry(D64 *d64, u8 track)
{
    D64_HEADER_SECTOR *header = &d64->image->d64_header;
    return &header->entries[track-1];
}

static bool d64_allocate(D64 *d64, D64_TS ts)
{
    D64_BAM_ENTRY *entry = d64_get_bam_entry(d64, ts.track);
    return d64_header_allocate(entry, ts.sector);
}

static bool d64_deallocate(D64 *d64, D64_TS ts)
{
    D64_BAM_ENTRY *entry = d64_get_bam_entry(d64, ts.track);
    return d64_header_deallocate(entry, ts.sector);
}

static bool d64_deallocate_sector(D64 *d64, D64_TS ts)
{
    if (d64->image->type == D64_TYPE_D81)
    {
        return d81_deallocate(d64, ts);
    }

    if (d64->image->type == D64_TYPE_D71 && ts.track > D64_TRACKS)
    {
        return d71_deallocate_36_70(d64, ts);
    }

    return d64_deallocate(d64, ts);
}

static void d64_deallocate_file(D64 *d64, D64_DIR_ENTRY *entry)
{
    d64->sector.next = entry->start;
    while (d64->sector.next.track &&
           d64_deallocate_sector(d64, d64->sector.next) &&
           d64_read_next(d64));
}

static bool d64_delete_file(D64 *d64, D64_DIR_ENTRY *entry)
{
    if ((entry->type & 7) >= D64_FILE_REL)  // REL files are not supported yet
    {
        return false;
    }

    // update the dir entry
    entry->type = D64_FILE_DEL;
    if (!d64_write_current(d64))
    {
        return false;
    }

    d64_deallocate_file(d64, entry);

    // write updated BAM back to disk
    return d64_write_bam(d64) && d64_sync(d64->image);
}

static u8 d64_get_tracks(D64 *d64)
{
    switch (d64->image->type)
    {
        case D64_TYPE_D81:  return D81_TRACKS;
        case D64_TYPE_D71:  return D71_TRACKS;
        default:            return D64_TRACKS;
    }
}

static bool d64_has_free_sector(D64 *d64, u8 track)
{
    if (d64->image->type == D64_TYPE_D81)
    {
        D81_BAM_ENTRY *entry = d81_get_bam_entry(d64, track);
        return track != D81_TRACK_DIR && entry->free_sectors != 0;
    }

    if (track <= D64_TRACKS)
    {
        D64_BAM_ENTRY *entry = d64_get_bam_entry(d64, track);
        return track != D64_TRACK_DIR && entry->free_sectors != 0;
    }

    track -= D64_TRACKS;
    D64_HEADER_SECTOR *header = &d64->image->d64_header;
    return track != D64_TRACK_DIR && header->free_sectors_36_70[track-1] != 0;
}

static bool d64_find_available_track(D64 *d64, u8 *track)
{
    u8 tracks = d64_get_tracks(d64);

    for (s8 offset=0; offset<tracks; offset++)
    {
        s8 new_track = *track - offset;
        if (new_track > 0 && d64_has_free_sector(d64, new_track))
        {
            *track = new_track;
            return true;
        }

        new_track = *track + offset;
        if (new_track <= tracks && d64_has_free_sector(d64, new_track))
        {
            *track = new_track;
            return true;
        }
    }

    return false;   // disk is full
}

static u8 d64_get_sectors(D64 *d64, u8 track)
{
    switch (d64->image->type)
    {
        case D64_TYPE_D81:
            return D81_SECTORS;

        case D64_TYPE_D71:
            if (track > D64_TRACKS)
            {
                track -= D64_TRACKS;
            }
        default:
            return d64_track_offset[track] - d64_track_offset[track-1];
    }
}

static bool d64_find_free_sector(D64 *d64, D64_TS *ts, u8 interleave)
{
    u8 sectors = d64_get_sectors(d64, ts->track);

    for (int count=0; count<sectors; count++)
    {
        ts->sector += interleave;
        if (ts->sector > sectors)
        {
            ts->sector -= (sectors + 1);
        }
        else if (ts->sector == sectors)
        {
            ts->sector = 0;
        }

        if (d64->image->type == D64_TYPE_D81)
        {
            if (d81_allocate(d64, *ts))
            {
                return true;
            }
        }
        else if (d64->image->type == D64_TYPE_D71 && ts->track > D64_TRACKS)
        {
            if (d71_allocate_36_70(d64, *ts))
            {
                return true;
            }
        }
        else
        {
            if (d64_allocate(d64, *ts))
            {
                return true;
            }
        }

        interleave = 1;
    }

    return false;
}

static bool d64_find_free_track_sector(D64 *d64, D64_TS *ts)
{
    if (!d64_find_available_track(d64, &ts->track))
    {
        return false;
    }

    u8 interleave = 1;
    if (d64->image->type == D64_TYPE_D64)
    {
        interleave = 10;
    }
    else if (d64->image->type == D64_TYPE_D71)
    {
        interleave = 6;
    }

    if (ts->sector == 255)
    {
        // start at sector 0
        ts->sector = -interleave;
    }

    return d64_find_free_sector(d64, ts, interleave);
}

static void d64_set_sector_length(D64 *d64, u8 size)
{
    d64->sector.next.track = 0;
    d64->sector.next.sector = size + 1;
}

static D64_DIR_ENTRY * d64_get_empty_dir(D64 *d64)
{
    d64_rewind_dir(d64);

    D64_DIR_ENTRY *entry;
    while ((entry = d64_read_next_dir(d64)))
    {
        if (d64_is_empty_or_deleted(entry))
        {
            // can be deleted entry (let's reuse it), or a new, empty entry
            return entry;
        }
    }

    // no empty entry found - allocate new dir sector
    u8 interleave = 3;
    if (d64->image->type == D64_TYPE_D81)
    {
        interleave = 1;
    }

    d64->sector.next = d64->sector.current;
    if (!d64_find_free_sector(d64, &d64->sector.next, interleave))
    {
        return NULL;
    }

    if (!d64_write_current(d64))
    {
        return NULL;
    }

    d64->sector.current = d64->sector.next;
    d64_set_sector_length(d64, D64_SECTOR_DATA_LEN);
    memset(&d64->sector.data, 0, D64_SECTOR_DATA_LEN);
    d64->data_ptr = 0;

    return d64->dir.entries;
}

static void d64_pad_filename(char *dest, const char *src)
{
    for (u8 i=0; i<16; i++)
    {
        if (*src)
        {
            dest[i] = *src++;
        }
        else
        {
            dest[i] = '\xa0';
        }
    }
}

static bool d64_is_valid_dos_version(D64 *d64)
{
    u8 version = d64->image->d64_header.dos_version;

    if (d64->image->type == D64_TYPE_D81)
    {
        return !version || version == D81_DOS_VERSION;
    }

    return !version || version == D64_DOS_VERSION;
}

static bool d64_create_file(D64 *d64, const char *file_name, u8 file_type,
                            D64_DIR_ENTRY *existing_file)
{
    if (!d64_is_valid_dos_version(d64))
    {
        return false;
    }

    D64_DIR_ENTRY *entry = existing_file;
    if (!entry)
    {
        entry = d64_get_empty_dir(d64);
        if (!entry)
        {
            return false;           // directory is full
        }
    }

    entry->start.track = d64->sector.current.track;
    entry->start.sector = 255;
    if (!d64_find_free_track_sector(d64, &entry->start))
    {
        d64_sync(d64->image);
        return false;               // disk is full
    }

    if (!existing_file)
    {
        d64_pad_filename(entry->filename, file_name);
        entry->type = file_type;    // without setting bit 7 (splat file)
        entry->blocks = 0;

        // write new dir entry
        if (!d64_write_current(d64) || !d64_sync(d64->image))
        {
            return false;
        }
    }

    d64->file.start = entry->start;
    d64->file.dir = d64->sector.current;
    d64->file.dir_ptr = d64->data_ptr;
    d64->sector_count = 1;

    d64_open_file_write(d64, entry);
    return true;
}

static size_t d64_write_data(D64 *d64, u8 *buf, size_t buf_size)
{
    size_t written_bytes = 0;
    while (written_bytes < buf_size)
    {
        u8 bytes_to_copy = d64_bytes_left(d64);
        if (!bytes_to_copy)
        {
            d64->sector.next = d64->sector.current;
            if (!d64_find_free_track_sector(d64, &d64->sector.next))
            {
                // disk full
                d64_set_sector_length(d64, D64_SECTOR_DATA_LEN);
            }

            if (!d64_write_current(d64) || !d64->sector.next.track)
            {
                break;
            }

            d64->sector.current = d64->sector.next;
            d64->data_ptr = 0;
            d64->sector_count++;
            bytes_to_copy = D64_SECTOR_DATA_LEN;
        }

        size_t left_in_buffer = buf_size - written_bytes;
        if (bytes_to_copy > left_in_buffer)
        {
            bytes_to_copy = left_in_buffer;
        }

        memcpy(&d64->sector.data[d64->data_ptr], &buf[written_bytes],
               bytes_to_copy);
        d64->data_ptr += bytes_to_copy;
        written_bytes += bytes_to_copy;
    }

    d64_sync(d64->image);
    return written_bytes;
}

static bool d64_write_finalize(D64 *d64)
{
    // write last sector's data
    memset(&d64->sector.data[d64->data_ptr], 0x00,
           D64_SECTOR_DATA_LEN - d64->data_ptr);
    d64_set_sector_length(d64, d64->data_ptr);

    if (!d64_write_current(d64))
    {
        return false;
    }

    // Read file dir entry
    if (!d64_read_from(d64->image, &d64->sector, d64->file.dir))
    {
        return false;
    }
    D64_DIR_ENTRY *entry = &d64->dir.entries[d64->file.dir_ptr];

    // delete existing file (if any)
    if (d64->file.start.track != entry->start.track ||
        d64->file.start.sector != entry->start.sector)
    {
        d64_deallocate_file(d64, entry);
        if (!d64_read_from(d64->image, &d64->sector, d64->file.dir))
        {
            return false;
        }
        entry->start = d64->file.start;
    }

    // update the entry for the new file
    entry->blocks = d64->sector_count;
    entry->type |= D64_FILE_NO_SPLAT;
    if (!d64_write_current(d64))
    {
        return false;
    }

    // write updated BAM back to disk
    return d64_write_bam(d64) && d64_sync(d64->image);
}

static void d64_init(D64_IMAGE *image, D64 *d64)
{
    d64->image = image;
    d64->data_len = d64->data_ptr = 0;
    d64->sector.next.track = 0;
}

static bool d64_open(D64_IMAGE *image, const char *filename)
{
    if (!file_open(&image->file, filename, FA_READ|FA_WRITE) &&
        !file_open(&image->file, filename, FA_READ))
    {
        return false;
    }

    image->type = d64_get_type(f_size(&image->file));
    if (image->type == D64_TYPE_UNKNOWN)
    {
        file_close(&image->file);
        return false;
    }

    return d64_read_header(image);
}
