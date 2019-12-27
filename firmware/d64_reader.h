/*
 * Copyright (c) 2019 Kim Jørgensen
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
#define DIRSECTS  19

static const uint16_t d64_track_offset[42] =
{
    0x0000, 0x0015, 0x002a, 0x003f, 0x0054, 0x0069, 0x007e, 0x0093,
    0x00a8, 0x00bd, 0x00d2, 0x00e7, 0x00fc, 0x0111, 0x0126, 0x013b,
    0x0150, 0x0165, 0x0178, 0x018b, 0x019e, 0x01b1, 0x01c4, 0x01d7,
    0x01ea, 0x01fc, 0x020e, 0x0220, 0x0232, 0x0244, 0x0256, 0x0267,
    0x0278, 0x0289, 0x029a, 0x02ab, 0x02bc, 0x02cd, 0x02de, 0x02ef,
    0x0300, 0x0311
};

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    uint8_t next_track;
    uint8_t next_sector;
} D64_SECTOR_HEADER;

typedef struct
{
    uint8_t next_track;
    uint8_t next_sector;
    uint8_t type;
    uint8_t track;
    uint8_t sector;
    char filename[16];
    uint8_t ignored[9];
    uint16_t blocks;
} D64_DIR_ENTRY;
#pragma pack(pop)

typedef struct
{
    bool visited[DIRSECTS];
    union
    {
        uint8_t sector[256];
        D64_DIR_ENTRY entries[8];
    };

    char *diskname;         // valid after d64_open
    D64_DIR_ENTRY *entry;   // valid after d64_read_dir
} D64_DIR;

static bool d64_validate_size(FSIZE_t imgsize)
{
    switch (imgsize)
    {
        case 174848:  // 35 tracks no errors
        case 175531:  // 35 w/ errors
        case 196608:  // 40 tracks no errors
        case 197376:  // 40 w/ errors
        case 205312:  // 42 tracks no errors
        case 206114:  // 42 w/ errors
            return true;
    }

    return false;
}
