/*
 * Copyright (c) 2019-2020 Kim Jørgensen, Sandor Vass
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
 ******************************************************************************
 * Based on d64_reader
 * Original source (C) Kim Jørgensen
 */

#include "disk_drive.h"
#include "d64_reader.h"
#include "d64_writer.h"

bool allocate_if_free(D64_SAVE_BUFFER *save_buffer, uint8_t track, uint8_t sector) {
    D64_BAM_ENTRY *bam_entry = &save_buffer->header_sector.entries[track-1];
    uint8_t *d = &bam_entry->data[sector / 8];
    if ((*d & (1<<(sector % 8))) != 0)
    {
        *d &= ~(1<<(sector % 8));
        return true;
    }
    else
    {
        return false;
    }
}

bool deallocate_in_bam(D64_SAVE_BUFFER *save_buffer, uint8_t track, uint8_t sector) {
    D64_BAM_ENTRY *bam_entry = &save_buffer->header_sector.entries[track-1];
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

static void d64_deallocate_from_sector(D64 *d64, D64_SAVE_BUFFER *save_buffer)
{
    uint8_t track = d64->sector.next_track;
    uint8_t sector = d64->sector.next_sector;

    while(track!=0) {
        if(!deallocate_in_bam(save_buffer, track, sector) ||
           !d64_read_sector(d64, track, sector)) 
        {
            break;
        } 
        track = d64->sector.next_track;
        sector = d64->sector.next_sector;
    }
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

bool d64_write_sync(D64 *d64) {
    return f_sync(&d64->file) == FR_OK;
}

static bool d64_find_and_allocate_free_dir_sector(D64 *d64, D64_SAVE_BUFFER *save_buffer, 
                                                  uint8_t *new_track, uint8_t *new_sector)
{
    // Directory entries shall not be allocated outside of track 18 (in case of 1541...)
    *new_track = 18;

    D64_BAM_ENTRY *dir_bam_entry = &save_buffer->header_sector.entries[*new_track - 1];
    if(dir_bam_entry->free_sectors==0)
    {
        return false;
    }

    *new_sector = d64->last_read_sector+9;

    for(uint8_t s = 0; s < d64_sector_num[*new_track-1]; s++) 
    {
        (*new_sector)++;
        if(*new_sector>=d64_sector_num[*new_track-1])
            *new_sector -= d64_sector_num[*new_track-1];

        uint8_t *d = &dir_bam_entry->data[*new_sector / 8];
        if((*d & (1<< (*new_sector % 8))) != 0) 
        {
            *d &= ~(1<<(*new_sector % 8 ));
            dir_bam_entry->free_sectors--;
            return true;
        }
    }
    return false;
}

static bool d64_get_first_free_sector(D64 *d64, D64_SAVE_BUFFER *save_buffer, uint8_t *free_track, uint8_t *free_sector) 
{
    D64_HEADER_SECTOR *header_sector = &save_buffer->header_sector;
    uint8_t track = 1;
    while(track<36) 
    {
        D64_BAM_ENTRY *bam_entry = &header_sector->entries[track-1];
        if(bam_entry->free_sectors > 0)
        {
            for(int sector = 0; sector<d64_sector_num[track-1]; sector++) 
            {
                uint8_t *d = &bam_entry->data[sector / 8];
                if((*d & (1<<(sector % 8))) != 0) 
                {
                    *d &= ~(1<<(sector % 8));
                    *free_track = track;
                    *free_sector = sector;
                    bam_entry->free_sectors--;
                    return true;
                }
            }
        }

        track++;
        if(track==18) 
        {
            track++;
        }
    }
    return false;
}

static bool d64_get_next_free_sector(D64_SAVE_BUFFER *save_buffer,
                                     uint8_t from_track, uint8_t from_sector,
                                     uint8_t *new_track, uint8_t *new_sector) 
    {
        *new_track = from_track;
        *new_sector = from_sector;
        uint8_t interleave = 10;
        while(save_buffer->header_sector.entries[*new_track-1].free_sectors==0) {
            (*new_track)++;
            *new_sector = 0;
            interleave = 0;
            if(*new_track>35) 
            {
                *new_track = 1;
            } 
            else if (*new_track == 18) 
            {
                (*new_track)++;
            }
            if(*new_track == from_track) {
                // ??? disk is full
                return false;
            }
        }
        
        bool found = false;
        uint8_t sector_num_in_track = d64_sector_num[*new_track-1];
        for(int count = 0; count<sector_num_in_track; count++) 
        {
            *new_sector += interleave;
            if(*new_sector >= sector_num_in_track)
                *new_sector -= sector_num_in_track;

            if(allocate_if_free(save_buffer, *new_track, *new_sector))
            {
                found = true;
                save_buffer->header_sector.entries[*new_track-1].free_sectors--;
                break;
            }
            interleave = 1;
        }
        return found;
    }

static bool d64_writer_create_file_entry(D64 *d64, D64_SAVE_BUFFER *save_buffer, 
                                         const char *file_name, uint8_t file_type,
                                         uint8_t filesize_in_blocks) 
{
    d64_rewind_dir(d64);
    d64_read_next_sector(d64, &d64->sector);
    d64->entry = d64->entries;
    uint8_t track=18, sector=1;
    bool entry_found = false;
    while(!entry_found) 
    {
        while(d64->entry < &d64->entries[8]) 
        {
            D64_DIR_ENTRY *entry = d64->entry;
            if((entry->type & 7) == D64_FILE_DEL || entry->filename[0]=='\xa0' || entry->filename[0]=='\x00') 
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
            track = d64->sector.next_track;
            sector = d64->sector.next_sector;
            d64_read_next_sector(d64, &d64->sector);
            d64->entry = d64->entries;
            d64->visited_dir_sectors++;
            if(d64->visited_dir_sectors>40) 
            {
                wrn("Directory too big or there is a recursive link\n");
                return false;
            }
        }
        else                        // no linked sector, this is the last one
        {
            uint8_t next_free_track;
            uint8_t next_free_sector;
            if(!d64_find_and_allocate_free_dir_sector(d64, save_buffer, &next_free_track, &next_free_sector))
            {
                return false;                   // directory table is full
            }

//            d64_read_sector(d64, track, sector);
            d64->sector.next_track = next_free_track;
            d64->sector.next_sector = next_free_sector;
            memcpy(&save_buffer->modified_directory_sector_2.directory_sector, &d64->sector, sizeof(D64_SECTOR));
            save_buffer->modified_directory_sector_2.track = track;
            save_buffer->modified_directory_sector_2.sector = sector;

            d64_read_sector(d64, next_free_track, next_free_sector);
            memset(&d64->sector, 0, sizeof(D64_SECTOR));
            d64->sector.next_track=0;
            d64->sector.next_sector=255;
            d64->entry = d64->entries;
            track = next_free_track;
            sector = next_free_sector;
            
            entry_found = true;
            break;
        }
        
    }

    if(!entry_found) 
    {
        wrn("shall not exit from loop without found any entry!!");
        return false;
    }

    uint8_t first_track, first_sector;

    if(!d64_get_first_free_sector(d64, save_buffer, &first_track, &first_sector)) {
        wrn("Shall not happen, free space check passed...");
        return false;
    }

    d64->entry->track = first_track;        // here will start the program to be saved
    d64->entry->sector = first_sector;
    memset(d64->entry->filename, '\xa0', 16);
    memcpy(d64->entry->filename, file_name, strlen(file_name)); // excluding trailing 0x00
    
    d64->entry->type = file_type | 0x80;
    d64->entry->blocks = filesize_in_blocks;
    
    memcpy(&save_buffer->modified_directory_sector_1.directory_sector, &d64->sector, sizeof(D64_SECTOR));
    save_buffer->modified_directory_sector_1.track = track;     // The dir sector's position where the entry is
    save_buffer->modified_directory_sector_1.sector = sector;

    save_buffer->data_sector.track = first_track;       // This is the data sector's position when saving
    save_buffer->data_sector.sector = first_sector;
    save_buffer->data_sector.pos = 0;

    return true;
}