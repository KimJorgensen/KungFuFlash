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
#ifndef COMMANDS_H
#define COMMANDS_H

#define DIR_NAME_LENGTH 34
#define ELEMENT_LENGTH 38
#define MAX_ELEMENTS_PAGE 22

// Use non-breaking spaces as first character for element type
#define SELECTED_ELEMENT 0xa0
#define TEXT_ELEMENT     0xe0

#define BASIC_CMD_BUF_SIZE 80

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
    CMD_LOAD_NEXT_BANK,
    CMD_OPEN,
    CMD_CLOSE,
    CMD_TALK,
    CMD_UNTALK,
    CMD_GET_BYTE
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
    REPLY_READ_DONE = 0x80,
    REPLY_READ_BANK,

    REPLY_NOT_FOUND,
    REPLY_READ_ERROR,
    REPLY_END_OF_FILE
} REPLY_TYPE;

#endif
