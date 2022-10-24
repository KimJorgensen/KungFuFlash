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

#define FW_NAME_SIZE 20
#define KFF_ID_VALUE 0x2a
#define LOADING_OFFSET 0x80

// Commands from Kung Fu Flash to C64 and replies from C64 to Kung Fu Flash
typedef enum
{
    CMD_NONE = 0x00,    // Get reply from C64

    // Menu commands
    CMD_MESSAGE,
    CMD_WARNING,
    CMD_FLASH_MESSAGE,
    CMD_TEXT,
    CMD_TEXT_WAIT,

    CMD_MENU,
    CMD_READ_DIR,
    CMD_READ_DIR_PAGE,

    CMD_MOUNT_DISK,
    CMD_WAIT_RESET,     // Disable screen and wait for reset

    // Disk commands
    CMD_NO_DRIVE = 0x10,
    CMD_DISK_ERROR,
    CMD_NOT_FOUND,
    CMD_END_OF_FILE,

    // SYNC commands
    CMD_WAIT_SYNC = 0x50,
    CMD_SYNC = 0x55,

    REPLY_OK = 0x80,

    // Menu replies
    REPLY_DIR,
    REPLY_DIR_ROOT,
    REPLY_DIR_UP,
    REPLY_DIR_PREV_PAGE,
    REPLY_DIR_NEXT_PAGE,

    REPLY_SELECT,
    REPLY_SETTINGS,
    REPLY_BASIC,
    REPLY_KILL,
    REPLY_KILL_C128,

    REPLY_RESET,

    // Disk replies
    REPLY_LOAD = 0x90,
    REPLY_SAVE,

    REPLY_OPEN,
    REPLY_CLOSE,

    REPLY_TALK,
    REPLY_UNTALK,
    REPLY_SEND_BYTE,

    REPLY_LISTEN,
    REPLY_UNLISTEN,
    REPLY_RECEIVE_BYTE
} COMMAND_TYPE;

typedef enum
{
    // EAPI commands
    CMD_EAPI_NONE = 0x00,
    CMD_EAPI_INIT,
    CMD_WRITE_FLASH,
    CMD_ERASE_SECTOR,
} EAPI_COMMAND_TYPE;

typedef enum
{
    // EAPI replies
    REPLY_EAPI_OK = 0x00,
    REPLY_WRITE_WAIT,
    REPLY_WRITE_ERROR,
} EAPI_REPLY_TYPE;

typedef enum
{
    SELECT_FLAG_OPTIONS = 0x40,
    SELECT_FLAG_C128    = 0x80
} SELECT_FLAGS;
