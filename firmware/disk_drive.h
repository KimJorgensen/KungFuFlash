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

#ifndef DISK_DRIVE_H
#define DISK_DRIVE_H

typedef struct
{
    const char *name;
    uint8_t drive;
    uint8_t type;
    char mode;
    bool overwrite;
} PARSED_FILENAME;

typedef struct
{
    uint8_t number;
    uint8_t bytes_left;
    uint8_t bytes_ptr;
    D64_SECTOR sector;
} DISK_CHANNEL;

// RECV_BUFFER_OFFSET = $0100 - SAVE_BUF_SIZE. Check disk.s 
#define RECV_BUFFER_OFFSET 0x70

#endif // DISK_DRIVE_H