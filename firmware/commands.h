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

#define DIR_NAME_LENGTH 36
#define ELEMENT_LENGTH 38
#define MAX_ELEMENTS_PAGE 22
#define SEARCH_LENGTH 26

// Use non-breaking spaces as first character in dir name
#define SEARCH_SUPPORTED 0xa0
#define CLEAR_SEARCH     0xe0

// Use non-breaking spaces as first character for element type
#define SELECTED_ELEMENT 0xa0
#define TEXT_ELEMENT     0xe0

#define BASIC_CMD_BUF_SIZE  80
#define LOADING_OFFSET      0x80
#define SAVE_BUFFER_OFFSET  0x70

typedef enum {
    CMD_NONE = 0x00,

    // Menu commands
    CMD_DIR,
    CMD_DIR_ROOT,
    CMD_DIR_UP,
    CMD_DIR_PREV_PAGE,
    CMD_DIR_NEXT_PAGE,

    CMD_SELECT,
    CMD_SETTINGS,
    CMD_BASIC,
    CMD_KILL,
    CMD_KILL_C128,

    CMD_RESET,

    // Disk commands
    CMD_LOAD = 0x80,
    CMD_SAVE,
    CMD_OPEN,
    CMD_CLOSE,
    CMD_TALK,
    CMD_UNTALK,
    CMD_GET_BYTE,

    // EAPI commands
    CMD_EAPI_INIT = 0xf0,
    CMD_WRITE_FLASH,
    CMD_ERASE_SECTOR
} COMMAND_TYPE;

typedef enum {
    SELECT_FLAG_OPTIONS = 0x40,
    SELECT_FLAG_C128    = 0x80
} SELECT_FLAGS;

typedef enum {
    REPLY_OK = 0x00,        // No action on C64

    // Menu replies
    REPLY_READ_DIR,
    REPLY_READ_DIR_PAGE,

    REPLY_EXIT_MENU,        // Exit menu and wait for EFSTART:xxx command

    // Disk replies
    REPLY_NO_DRIVE = 0x80,

    REPLY_DISK_ERROR,
    REPLY_NOT_FOUND,
    REPLY_END_OF_FILE,

    // EAPI replies
    REPLY_WRITE_WAIT = 0xf0,
    REPLY_WRITE_ERROR
} REPLY_TYPE;
