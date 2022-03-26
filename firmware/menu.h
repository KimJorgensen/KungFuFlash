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

typedef struct
{
    void *state;
    u8 (*dir)(void *state);
    u8 (*dir_up)(void *state, bool root);
    u8 (*prev_page)(void *state);
    u8 (*next_page)(void *state);
    u8 (*select)(void *state, u8 flags, u8 element);
} MENU;

static const MENU *menu;

static void fail_to_read_sd(void);
static u8 handle_unsupported(const char *file_name);
static u8 handle_unsupported_ex(const char *title, const char *message, const char *file_name);
static u8 handle_unsupported_warning(const char *message, const char *file_name, u8 element_no);
static u8 handle_unsaved_crt(const char *file_name, void (*handle_save)(u8));
static u8 handle_file_options(const char *file_name, u8 file_type, u8 element_no);
static u8 handle_upgrade_menu(const char *firmware, u8 element_no);
static const char * to_petscii_pad(char *dest, const char *src, u8 size);
static bool format_path(char *buf, bool include_file);
static void send_page_end(void);
static u8 handle_page_end(void);
