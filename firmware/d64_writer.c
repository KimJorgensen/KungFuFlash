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

#include "d64_writer.h"

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
