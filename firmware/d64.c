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

    d64->last_read_track = track;
    d64->last_read_sector = sector;

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
    if (!file_open(&d64->file, filename, FA_READ|FA_WRITE))
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

static bool d64_write_sector(D64 *d64, D64_SECTOR *sector_buffer, uint8_t track, uint8_t sector) 
{
    FSIZE_t offset;
    if (d64->image_type == D64_IMAGE_D81)
    {
        offset = d81_get_offset(d64, track, sector);    // not supported yet
    }
    else
    {
        offset = d64_get_offset(d64, track, sector);
    }

    if (!file_seek(&d64->file, offset) ||
        file_write(&d64->file, sector_buffer, sizeof(D64_SECTOR)) != sizeof(D64_SECTOR))
    {
        err("Failed to write track %d sector %d\n", track, sector);
        return false;
    }
    return true;
}

static bool d64_write_sync(D64 *d64) 
{
    return f_sync(&d64->file) == FR_OK;
}

static bool available_in_bam(D64_HEADER_SECTOR *header, uint8_t track, uint8_t sector) 
{
    return ((header->entries[track-1].data[sector >> 3] & (1<<(sector & 0x07))) != 0);
}

static bool allocate_in_bam(D64_HEADER_SECTOR *header, uint8_t track, uint8_t sector) 
{
    D64_BAM_ENTRY *bam_entry = &header->entries[track-1];
    uint8_t *d = &bam_entry->data[sector / 8];
    if ((*d & (1<<(sector % 8))) != 0)
    {
        *d &= ~(1<<(sector % 8));
        bam_entry->free_sectors--;
        return true;
    }
    else
    {
        return false;
    }
}

static bool deallocate_in_bam(D64_HEADER_SECTOR *header, uint8_t track, uint8_t sector) 
{
    D64_BAM_ENTRY *bam_entry = &header->entries[track-1];
    uint8_t *d = &bam_entry->data[sector / 8];
    if ((*d & (1<<(sector % 8))) == 0)
    {
        *d |= (1<<(sector % 8));
        bam_entry->free_sectors++;
        return true;
    }
    else
    {
        return false;
    }
}

static void deallocate_chain(D64 *d64, D64_HEADER_SECTOR *header)
{
    while(d64->sector.next_track!=0 && 
            deallocate_in_bam(header, 
                              d64->sector.next_track,
                              d64->sector.next_sector) &&
            d64_read_next_sector(d64, &d64->sector)) ;
}


static bool d64_update_file_for_save(D64 *d64) {
    D64_DIR_ENTRY *direntry_entry = d64->entry;         // let's save some info - d64->sector will be overwritten
    uint8_t direntry_track_pos = d64->last_read_track;
    uint8_t direntry_sector_pos = d64->last_read_sector;
    uint8_t file_start_track = d64->entry->track;
    uint8_t file_start_sector = d64->entry->sector;
    
    d64->entry->blocks = 0;
    d64->entry->type &= 0x7f;
    
    if(!d64_write_sector(d64, &d64->sector, direntry_track_pos, direntry_sector_pos) ||
        !d64_read_disk_header(d64))
    {
        return false;
    }
    D64_HEADER_SECTOR header;
    memcpy(&header, &d64->sector, sizeof(D64_HEADER_SECTOR));
    d64_set_next_sector(d64, file_start_track, file_start_sector);
    deallocate_chain(d64, &header);                                     // old file deallocated
    memcpy(&d64->sector, &header, sizeof(D64_HEADER_SECTOR));

    if(!d64_write_sector(d64, (D64_SECTOR *)&d64->d64_header, 18, 0))    // deallocation is written to disk
    {
        return false;
    }
    
    if(d64_read_sector(d64, direntry_track_pos, direntry_sector_pos)) 
    {
        d64->entry = direntry_entry;
    }
    else
    {
        return false;
    }

    return d64_write_sync(d64);
}

static bool d64_finalize_file_for_save(D64 *d64, uint8_t blocks)
{
    d64->entry->blocks = blocks;
    d64->entry->type |= 0x80;
    return d64_write_sector(d64, 
                            &d64->sector, 
                            d64->last_read_track, 
                            d64->last_read_sector) && d64_write_sync(d64);

}

static bool find_next_available_track(D64_HEADER_SECTOR *header,
                                     uint8_t from_track,
                                     uint8_t *new_track)
{
    *new_track = from_track;
    while(header->entries[*new_track-1].free_sectors==0) 
    {
        (*new_track)++;
        if(*new_track>35) 
        {
            *new_track = 1;
        } 
        else if (*new_track == 18) 
        {
            (*new_track)++;
        }
        if((*new_track == from_track) || from_track == 18) 
        {
            // disk or directory is full
            return false;
        }
    }
    return true;
}


static bool get_next_free_sector(D64_HEADER_SECTOR *header,
                                     uint8_t from_track, uint8_t from_sector,
                                     uint8_t *new_track, uint8_t *new_sector, 
                                     bool allocate) 
{
    uint8_t interleave = 10;
    *new_sector = from_sector;
    if(!find_next_available_track(header, from_track, new_track)) 
    {
        return false;
    }
    if(from_track != *new_track) 
    {
        interleave = 1;
        *new_sector = d64_sector_num[*new_track-1]-1;
    }

    bool found = false;
    uint8_t sector_num_in_track = d64_sector_num[*new_track-1];
    for(int count = 0; count<sector_num_in_track; count++) 
    {
        *new_sector += interleave;
        if(*new_sector >= sector_num_in_track)
            *new_sector -= sector_num_in_track;
        
        if(allocate ? allocate_in_bam(header, *new_track, *new_sector) : available_in_bam(header, *new_track, *new_sector))
        {
            found = true;
            break;
        }
        interleave = 1;
    }
    return found;
}


static bool d64_get_next_empty_direntry(D64 *d64, D64_HEADER_SECTOR *header) {
    d64_rewind_dir(d64);
    bool entry_found = false;
    while(!entry_found) 
    {
        while(d64->entry < &d64->entries[8]) 
        {
            if((d64->entry->type & 7) == D64_FILE_DEL || 
                d64->entry->filename[0]=='\xa0' || 
                d64->entry->filename[0]=='\x00') 
            { // can be deleted entry (let's reuse it), or a new, empty entry
                entry_found = true;
                break;
            }
            d64->entry++;
        }
        if(entry_found)
            break;
            
        if(d64->sector.next_track)      // there is more linked directory sector
        {
            d64_read_next_sector(d64, &d64->sector);
            d64->entry = d64->entries;
            d64->visited_dir_sectors++;
            if(d64->visited_dir_sectors>40) 
            {
                wrn("Directory too big or there is a recursive link\n");
                break;
            }
        }
        else                        // no linked sector, this is the last one
        {
            uint8_t next_free_track;
            uint8_t next_free_sector;
            if(!get_next_free_sector(header, 
                                     d64->last_read_track, 
                                     d64->last_read_sector, 
                                     &next_free_track, 
                                     &next_free_sector,
                                     true))
            {
                break;                   // directory table is full
            }

            if(! d64_write_sector(d64, (D64_SECTOR *)header, 18, 0) ) 
            {
                break;
            }

            d64->sector.next_track = next_free_track;
            d64->sector.next_sector = next_free_sector;
            if(!d64_write_sector(d64, &d64->sector, d64->last_read_track, d64->last_read_sector)) 
            {
                break;
            }

            memset(&d64->sector, 0, sizeof(D64_SECTOR));
            d64->sector.next_track=0;
            d64->sector.next_sector=255;
            d64->entry = d64->entries;
            d64->last_read_track = next_free_track;     // let's lie that we have read this already
            d64->last_read_sector = next_free_sector;
            
            entry_found = true;
            break;
        }
    }
    return entry_found;
}

static bool d64_create_file_for_save(D64 *d64, 
                                     const char *file_name, 
                                     uint8_t file_type) {
    D64_HEADER_SECTOR header;

    if(!d64_read_disk_header(d64))
    {
        return false;
    }

    memcpy(&header, &d64->sector, sizeof(D64_HEADER_SECTOR));
    
    if(!d64_get_next_empty_direntry(d64, &header) )
    {
        return false;
    }

    uint8_t next_track_pos, next_sector_pos;

    get_next_free_sector(&header, 
                         1, 
                         11, 
                         &next_track_pos, 
                         &next_sector_pos,
                         false);

    memset(d64->entry->filename, '\xa0', 16);
    memcpy(d64->entry->filename, file_name, strlen(file_name)); // excluding trailing 0x00
    d64->entry->type = file_type;           // without setting bit 7
    d64->entry->blocks = 0;
    d64->entry->track = next_track_pos;
    d64->entry->sector = next_sector_pos;

    if(!d64_write_sector(d64, &d64->sector, d64->last_read_track, d64->last_read_sector)) 
    {
        return false;
    }

    return d64_write_sync(d64);
}

static bool d64_write_last_block(D64 *d64, 
                                 D64_SECTOR *sector, 
                                 uint8_t track_pos, 
                                 uint8_t sector_pos,
                                 uint8_t data_length)
{
    sector->next_track = 0;
    sector->next_sector = data_length + 1;
    allocate_in_bam(&d64->d64_header, track_pos, sector_pos);
    return d64_write_sector(d64, sector, track_pos, sector_pos) && // write data
           d64_write_sector(d64, &d64->sector, 18, 0);             // and write BAM also 
}


static bool d64_write_interim_block(D64 *d64, 
                                    D64_SECTOR *sector, 
                                    uint8_t track_pos, 
                                    uint8_t sector_pos)
{
    allocate_in_bam(&d64->d64_header, track_pos, sector_pos);

    uint8_t next_track, next_sector;
    if(!get_next_free_sector(&d64->d64_header, track_pos, sector_pos, 
                                               &next_track, &next_sector, 
                                               false)) 
    {
        return false;
    }
    sector->next_track = next_track;
    sector->next_sector = next_sector;

    return d64_write_sector(d64, sector, track_pos, sector_pos);
   
}