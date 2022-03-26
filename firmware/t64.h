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

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    char description[32];
    u16 version;
    u16 dir_entries;
    u16 used_entries;
    u16 reserved;
    char user_description[24];
} T64_HEADER;

typedef struct
{
    u8 type;            // T64_ENTRY_TYPE
    u8 file_type;
    u16 start_address;
    u16 end_address;
    u16 reserved;
    u32 file_offset;
    u32 reserved2;
    char filename[16];
} T64_ENTRY;
#pragma pack(pop)

typedef enum
{
    T64_FREE_ENTRY          = 0x00,
    T64_NORMAL_TAPE_FILE,
    T64_TAPE_WITH_HEADER,
    T64_MEMORY_SNAPSHOT,
    T64_TAPE_BLOCK,
    T64_DIGITIZED_STREAM
} T64_ENTRY_TYPE;

typedef struct
{
    FIL file;
    T64_HEADER header;
    T64_ENTRY entry;
    u16 next_entry;
} T64_IMAGE;
