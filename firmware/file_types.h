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

// From http://vice-emu.sourceforge.net/vice_16.html
typedef enum {
    CRT_NORMAL_CARTRIDGE = 0x00,
    CRT_ACTION_REPLAY,
    CRT_KCS_POWER_CARTRIDGE,
    CRT_FINAL_CARTRIDGE_III,
    CRT_SIMONS_BASIC,
    CRT_OCEAN_TYPE_1,
    CRT_EXPERT_CARTRIDGE,
    CRT_FUN_PLAY_POWER_PLAY,
    CRT_SUPER_GAMES,
    CRT_ATOMIC_POWER,
    CRT_EPYX_FASTLOAD,
    CRT_WESTERMANN_LEARNING,
    CRT_REX_UTILITY,
    CRT_FINAL_CARTRIDGE_I,
    CRT_MAGIC_FORMEL,
    CRT_C64_GAME_SYSTEM_SYSTEM_3,
    CRT_WARP_SPEED,
    CRT_DINAMIC,
    CRT_ZAXXON_SUPER_ZAXXON,
    CRT_MAGIC_DESK_DOMARK_HES_AUSTRALIA,
    CRT_SUPER_SNAPSHOT_V5,
    CRT_COMAL_80,
    CRT_STRUCTURED_BASIC,
    CRT_ROSS,
    CRT_DELA_EP64,
    CRT_DELA_EP7X8,
    CRT_DELA_EP256,
    CRT_REX_EP256,
    CRT_MIKRO_ASSEMBLER,
    CRT_FINAL_CARTRIDGE_PLUS,
    CRT_ACTION_REPLAY_4,
    CRT_STARDOS,
    CRT_EASYFLASH,
    CRT_EASYFLASH_XBANK,
    CRT_CAPTURE,
    CRT_ACTION_REPLAY_3,
    CRT_RETRO_REPLAY,
    CRT_MMC64,
    CRT_MMC_REPLAY,
    CRT_IDE64,
    CRT_SUPER_SNAPSHOT_V4,
    CRT_IEEE_488,
    CRT_GAME_KILLER,
    CRT_PROPHET64,
    CRT_EXOS,
    CRT_FREEZE_FRAME,
    CRT_FREEZE_MACHINE,
    CRT_SNAPSHOT64,
    CRT_SUPER_EXPLODE_V5_0,
    CRT_MAGIC_VOICE,
    CRT_ACTION_REPLAY_2,
    CRT_MACH_5,
    CRT_DIASHOW_MAKER,
    CRT_PAGEFOX,
    CRT_KINGSOFT,
    CRT_SILVERROCK_128K_CARTRIDGE,
    CRT_FORMEL_64,
    CRT_RGCD,
    CRT_RR_NET_MK3,
    CRT_EASYCALC,
    CRT_GMOD2
} CRT_TYPE;

typedef enum {
    CRT_CHIP_ROM = 0x00,
    CRT_CHIP_RAM,
    CRT_CHIP_FLASH
} CRT_CHIP_TYPE;

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    uint8_t signature[16];
    uint32_t header_length;
    uint16_t version;
    uint16_t cartridge_type;
    uint8_t exrom;
    uint8_t game;
    uint8_t reserved[6];
    uint8_t cartridge_name[32];
} CRT_HEADER;

typedef struct
{
    uint8_t signature[4];
    uint32_t packet_length;
    uint16_t chip_type;
    uint16_t bank;
    uint16_t start_address;
    uint16_t image_size;
} CRT_CHIP_HEADER;

typedef struct
{
    char signature[8];
    char filename[17];
    uint8_t rel_record_size;
} P00_HEADER;
#pragma pack(pop)

typedef enum {
    FILE_NONE       = 0x00,
    FILE_CRT,
    FILE_PRG,
    FILE_P00,
    FILE_D64,
    FILE_D64_PRG,
    FILE_ROM,

    FILE_UPD        = 0xfd,
    FILE_DAT,
    FILE_UNKNOWN
} FILE_TYPE;

typedef enum {
    DAT_FLAG_PERSIST_BASIC = 0x01
} DAT_FLAGS;

typedef enum {
    DAT_NONE = 0x00,
    DAT_CRT,
    DAT_PRG,
    DAT_USB,
    DAT_DISK,
    DAT_KERNAL,
    DAT_BASIC,
    DAT_KILL,
    DAT_KILL_C128
} DAT_BOOT_TYPE;

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    uint16_t type;          // CRT_TYPE
    uint8_t exrom;          // EXROM line status
    uint8_t game;           // GAME line status
    uint8_t banks;          // Number of 16k CRT banks in use (0-64)
    uint32_t flash_hash;    // Used id crt_banks > 4
} DAT_CRT_HEADER;

typedef struct
{
    uint16_t size;
    uint16_t element;       // Used if file is a D64
    char name[17];          // Used if file is a P00 or D64
} DAT_PRG_HEADER;

typedef struct
{
    uint8_t signature[8];   // DAT_SIGNATURE

    uint8_t flags;          // DAT_FLAGS
    uint8_t boot_type;      // DAT_BOOT_TYPE
    uint8_t reserved;       // Should be 0

    union
    {
        DAT_CRT_HEADER crt; // boot_type == DAT_CRT
        DAT_PRG_HEADER prg; // boot_type == DAT_PRG
    };

    char path[736];
    char file[256];
} DAT_HEADER;
#pragma pack(pop)

#define ELEMENT_NOT_SELECTED 0xffff
#define DAT_SIGNATURE "KungFu:\1"

DAT_HEADER dat_file;
__attribute__((__section__(".sram"))) uint8_t dat_buffer[64*1024];
