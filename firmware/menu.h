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
typedef struct MENU_STATE_s MENU_STATE;

struct MENU_STATE_s
{
    void (*dir)(MENU_STATE *state);
    void (*dir_up)(MENU_STATE *state, bool root);
    void (*prev_page)(MENU_STATE *state);
    void (*next_page)(MENU_STATE *state);
    bool (*select)(MENU_STATE *state, uint8_t flags, uint8_t element);
};

static MENU_STATE *menu_state;

static void handle_failed_to_read_sd(void);
static void handle_unsupported(const char *file_name);
static void handle_unsupported_ex(const char *title, const char *message, const char *file_name);
static void handle_file_options(const char *file_name, uint8_t file_type, uint8_t element_no);
static void handle_upgrade_menu(const char *firmware, uint8_t element_no);
static const char * to_petscii_pad(char *dest, const char *src, uint8_t size);
static bool format_path(char *buf, bool include_file);
static void send_page_end(void);
static void reply_page_end(void);
