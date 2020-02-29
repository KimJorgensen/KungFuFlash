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

#define D64_SECTOR_DATA_LEN 254

static const uint16_t d64_track_offset[42] =
{
    0x0000, 0x0015, 0x002a, 0x003f, 0x0054, 0x0069, 0x007e, 0x0093,
    0x00a8, 0x00bd, 0x00d2, 0x00e7, 0x00fc, 0x0111, 0x0126, 0x013b,
    0x0150, 0x0165, 0x0178, 0x018b, 0x019e, 0x01b1, 0x01c4, 0x01d7,
    0x01ea, 0x01fc, 0x020e, 0x0220, 0x0232, 0x0244, 0x0256, 0x0267,
    0x0278, 0x0289, 0x029a, 0x02ab, 0x02bc, 0x02cd, 0x02de, 0x02ef,
    0x0300, 0x0311
};

static const char *d64_types[8] = {"DEL", "SEQ", "PRG", "USR", "REL", "CBM", "???", "???"};

typedef enum
{
  D64_FILE_DEL = 0,
  D64_FILE_SEQ,
  D64_FILE_PRG,
  D64_FILE_USR,
  D64_FILE_REL,
  D64_FILE_CBM,
  D64_FILE_DIR
} D64_FILE_TYPE;

typedef enum
{
  D64_IMAGE_UNKNOWN = 0,
  D64_IMAGE_D64,
  D64_IMAGE_D71,
  D64_IMAGE_D81
} D64_IMAGE_TYPE;

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    uint8_t next_track;
    uint8_t next_sector;
    uint8_t data[D64_SECTOR_DATA_LEN];
} D64_SECTOR;

typedef struct
{
    uint8_t free_sectors;
    uint8_t data[3];
} D64_BAM_ENTRY;

typedef struct
{
    uint8_t next_track;
    uint8_t next_sector;
    uint8_t dos_version;
    uint8_t double_sided;           // Only used by D71
    D64_BAM_ENTRY entries[35];
    char diskname[27];
    uint8_t unused;
    D64_BAM_ENTRY dolphin_dos[5];
    D64_BAM_ENTRY speed_dos[5];
    uint8_t unused2[9];
    uint8_t free_sectors_36_70[35]; // Only used by D71
} D64_HEADER_SECTOR;

typedef struct
{
    uint8_t free_sectors;
    uint8_t data[5];
} D81_BAM_ENTRY;

typedef struct
{
    uint8_t next_track;
    uint8_t next_sector;
    uint16_t version;
    uint16_t disk_id;
    uint8_t io_byte;
    uint8_t auto_boot;
    uint8_t reserved[8];
    D81_BAM_ENTRY entries[40];
} D81_BAM_SECTOR;

typedef struct
{
    uint8_t next_track;
    uint8_t next_sector;
    uint8_t dos_version;
    uint8_t unused;
    char diskname[27];
    uint8_t unused2[225];
} D81_HEADER_SECTOR;

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
    FIL file;
    uint8_t image_type;

    uint8_t visited_dir_sectors;
    union
    {
        D64_SECTOR sector;
        D64_HEADER_SECTOR d64_header;
        D81_HEADER_SECTOR d81_header;
        D81_BAM_SECTOR d81_bam;
        D64_DIR_ENTRY entries[8];
    };

    char *diskname;         // valid after d64_read_disk_header
    D64_DIR_ENTRY *entry;   // valid after d64_read_dir
} D64;

static uint8_t d64_image_type(FSIZE_t imgsize)
{
    switch (imgsize)
    {
        // D64
        case 174848:  // 35 tracks no errors
        case 175531:  // 35 w/ errors
        case 196608:  // 40 tracks no errors
        case 197376:  // 40 w/ errors
        case 205312:  // 42 tracks no errors
        case 206114:  // 42 w/ errors
            return D64_IMAGE_D64;

        // D71
        case 349696:  // 70 tracks no errors
        case 351062:  // 70 w/ errors
            return D64_IMAGE_D71;

        // D81
        case 819200:  // 80 tracks no errors
        case 822400:  // 80 w/ errors
            return D64_IMAGE_D81;
    }

    return D64_IMAGE_UNKNOWN;
}
