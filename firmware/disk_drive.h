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

static const u16 dir_start_addr = 0x0401;   // Start of BASIC (for the PET)
static const u16 dir_link_addr = 0x0101;

static u8 disk_last_error;

typedef enum
{
    DISK_STATUS_OK          = 00,
    DISK_STATUS_SCRATCHED   = 01,
    DISK_STATUS_NOT_FOUND   = 62,
    DISK_STATUS_EXISTS      = 63,
    DISK_STATUS_INIT        = 73,
    DISK_STATUS_UNSUPPORTED = 0xFF
} DISK_STATUS;

typedef struct
{
    const char *name;
    bool wildcard;
    bool overwrite;
    u8 drive;
    u8 type;
    char mode;
} PARSED_FILENAME;

typedef enum
{
    DISK_BUF_NONE   = 0x00,
    DISK_BUF_USE,
    DISK_BUF_DIR,
    DISK_BUF_SAVE
} DISK_BUF_MODE;

typedef struct
{
    u8 number;

    u8 buf_mode;    // DISK_BUF_MODE
    u16 buf_len;
    u16 buf_ptr;
    u8 buf[256];

    union
    {
        char filename[256];
        u8 buf2[256];
    };
    char *filename_dir;
    u8 buf2_ptr;

    D64 d64;
    DIR dir;
    FIL file;
} DISK_CHANNEL;
