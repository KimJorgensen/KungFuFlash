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
    const MENU *prev_menu;

    const char *title;
    u8 selected_element;
    u8 no_of_elements;
} OPTIONS_STATE;

static OPTIONS_STATE options_state;

typedef struct OPTIONS_ELEMENT_s OPTIONS_ELEMENT;
typedef u8 (*options_func)(OPTIONS_STATE *, OPTIONS_ELEMENT *, u8);

struct OPTIONS_ELEMENT_s
{
    char text[ELEMENT_LENGTH];
    options_func callback;
    u8 flags;
    u8 element_no;
    void *user_state;
};

typedef enum
{
    SELECT_FLAG_ACCEPT      = 0x01,
    SELECT_FLAG_MOUNT       = 0x02,
    SELECT_FLAG_VIC         = 0x04,
    SELECT_FLAG_OVERWRITE   = 0x08,
    SELECT_FLAG_DELETE      = 0x10,
} SELECT_FLAGS_EXTRA;

static OPTIONS_STATE * options_init(const char *title);
static OPTIONS_ELEMENT * options_add_element(OPTIONS_STATE *state, options_func callback);
static inline void options_add_empty(OPTIONS_STATE *state);
static inline void options_add_text(OPTIONS_STATE *state, const char *text);
static void options_add_text_block(OPTIONS_STATE *state, const char *text);
static void options_add_select(OPTIONS_STATE *state, const char *text, u8 flags, u8 element_no);
static inline void options_add_dir(OPTIONS_STATE *state, const char *text);
static u8 handle_options(void);

static const char * options_element_text(OPTIONS_ELEMENT *element, const char *text);
