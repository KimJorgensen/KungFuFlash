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
 */

static const uint8_t d64_sector_num[40] =
{
    21, 21, 21, 21, 21, 21, 21,     
    21, 21, 21, 21, 21, 21, 21, 21,  // 1-17: 21s, 18-24: 19s, 25-30: 18s, 31-40: 17s
    21, 21, 19, 19, 19, 19, 19, 19,
    19, 18, 18, 18, 18, 18, 18, 17,
    17, 17, 17, 17, 17, 17, 17, 17,
    17
};

typedef struct
{
    uint8_t track;
    uint8_t sector;
    D64_SECTOR directory_sector;
} D64_DIRECTORY_SECTOR;

typedef struct
{
    uint8_t track;
    uint8_t sector;
    uint8_t pos;
    D64_SECTOR d64sector;
} D64_DATA_SECTOR;

typedef struct
{
    D64_HEADER_SECTOR header_sector;
    D64_DIRECTORY_SECTOR modified_directory_sector_1;
    D64_DIRECTORY_SECTOR modified_directory_sector_2; // not necessarily needed - initialize track with 0!
    D64_DATA_SECTOR data_sector;
} D64_SAVE_BUFFER;
