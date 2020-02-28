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

typedef struct
{
    MENU_STATE menu;
    MENU_STATE *prev_state;

    const char *title;
    uint8_t selected_element;
    uint8_t no_of_elements;
} OPTIONS_STATE;

static OPTIONS_STATE options_state;

typedef struct OPTIONS_ELEMENT_s OPTIONS_ELEMENT;
typedef bool (*options_func)(OPTIONS_STATE *, OPTIONS_ELEMENT *, uint8_t);

struct OPTIONS_ELEMENT_s
{
    char text[ELEMENT_LENGTH];
    options_func callback;
    uint8_t flags;
    uint8_t element_no;
};

typedef enum
{
    SELECT_FLAG_ACCEPTED    = 0x01,
    SELECT_FLAG_MOUNT       = 0x02
} SELECT_FLAGS_EXTRA;

static OPTIONS_STATE * options_init(const char *title);
static OPTIONS_ELEMENT * options_add_element(OPTIONS_STATE *state, options_func callback);
static inline void options_add_empty(OPTIONS_STATE *state);
static inline void options_add_text(OPTIONS_STATE *state, const char *text);
static void options_add_text_block(OPTIONS_STATE *state, const char *text);
static void options_add_select(OPTIONS_STATE *state, const char *text, uint8_t flags, uint8_t element_no);
static inline void options_add_dir(OPTIONS_STATE *state, const char *text);
static void handle_options(OPTIONS_STATE *state);

static const char * options_element_text(OPTIONS_ELEMENT *element, const char *text);
