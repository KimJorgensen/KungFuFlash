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

#define DISK_STATUS_OK          "00, OK,00,00"
#define DISK_STATUS_NOT_FOUND   "62,FILE NOT FOUND,00,00"
#define DISK_STATUS_INIT        "73,KUNG FU FLASH V" VERSION ",00,00"

const u16 dir_start_addr = 0x0401;  // Start of BASIC (for the PET)
const u16 dir_link_addr = 0x0101;

typedef struct
{
    const char *name;
    bool wildcard;
    bool overwrite;
    u8 drive;
    u8 type;
    char mode;
} PARSED_FILENAME;

typedef struct
{
    u8 number;

    u8 use_buf;
    u16 data_len;
    u16 data_ptr;
    u8 *buf;

    D64 d64;
    DIR dir;
    FIL file;
} DISK_CHANNEL;
