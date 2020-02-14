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

static OPTIONS_ELEMENT *elements = (OPTIONS_ELEMENT *)(scratch_buf +
                                   (sizeof(scratch_buf)/2));

static void options_dir(OPTIONS_STATE *state)
{
    c64_send_reply(REPLY_READ_DIR);

    to_petscii_pad(scratch_buf, state->title, DIR_NAME_LENGTH);
    c64_send_data(scratch_buf, DIR_NAME_LENGTH);

    for (uint8_t i=0; i<state->no_of_elements; i++)
    {
        OPTIONS_ELEMENT *element = elements + i;
        if (state->selected_element == i)
        {
            element->text[0] = SELECTED_ELEMENT;
        }

        c64_send_data(element->text, ELEMENT_LENGTH);
    }

    send_page_end();
}

static void options_dir_up(OPTIONS_STATE *state, bool root)
{
    menu_state = state->prev_state;
    if (root)
    {
        menu_state->dir_up(menu_state, root);
    }
    else
    {
        menu_state->dir(menu_state);
    }
}

static void options_prev_next_page(OPTIONS_STATE *state)
{
    reply_page_end();
}

static bool options_select(OPTIONS_STATE *state, uint8_t flags, uint8_t element_no)
{
    flags &= ~(SELECT_FLAG_OPTIONS); // No options in options menu

    if (element_no < state->no_of_elements)
    {
        OPTIONS_ELEMENT *element = elements + element_no;
        options_func callback = element->callback;
        if (callback)
        {
            state->selected_element = element_no;
            return callback(state, element, flags);
        }
    }

    options_dir(state);
    return false;
}

static OPTIONS_STATE * options_init(const char *title)
{
    if (!options_state.menu.dir)
    {
        options_state.menu.dir = (void (*)(MENU_STATE *))options_dir;
        options_state.menu.dir_up = (void (*)(MENU_STATE *, bool))options_dir_up;
        options_state.menu.prev_page = (void (*)(MENU_STATE *))options_prev_next_page;
        options_state.menu.next_page = (void (*)(MENU_STATE *))options_prev_next_page;
        options_state.menu.select = (bool (*)(MENU_STATE *, uint8_t, uint8_t))options_select;
    }

    // Can't nest options
    if (menu_state != &options_state.menu)
    {
        options_state.prev_state = menu_state;
    }
    options_state.title = title;
    options_state.selected_element = MAX_ELEMENTS_PAGE;
    options_state.no_of_elements = 0;

    return &options_state;
}

static const char * options_element_text(OPTIONS_ELEMENT *element, const char *text)
{
    text = to_petscii_pad(element->text + 1, text, ELEMENT_LENGTH-2);
    element->text[ELEMENT_LENGTH-1] = ' ';

    return text;
}

static OPTIONS_ELEMENT * options_add_element(OPTIONS_STATE *state, options_func callback)
{
    if (state->no_of_elements == MAX_ELEMENTS_PAGE)
    {
        wrn("Too many options on page. Overriding last element\n");
        state->no_of_elements = MAX_ELEMENTS_PAGE-1;
    }

    OPTIONS_ELEMENT *element = elements + state->no_of_elements;
    if (callback)
    {
        element->text[0] = ' ';

        // Select first non-text element
        if (state->selected_element == MAX_ELEMENTS_PAGE)
        {
            state->selected_element = state->no_of_elements;
        }
    }
    else
    {
        element->text[0] = TEXT_ELEMENT;
    }

    element->callback = callback;
    state->no_of_elements++;

    return element;
}

static OPTIONS_ELEMENT * options_add_text_element(OPTIONS_STATE *state, options_func callback, const char *text)
{
    OPTIONS_ELEMENT *element = options_add_element(state, callback);
    options_element_text(element, text);

    return element;
}

static inline void options_add_text(OPTIONS_STATE *state, const char *text)
{
    options_add_text_element(state, NULL, text);
}

static inline void options_add_empty(OPTIONS_STATE *state)
{
    options_add_text(state, "");
}

static void options_add_text_block(OPTIONS_STATE *state, const char *text)
{
    do
    {
        OPTIONS_ELEMENT *element = options_add_element(state, NULL);
        text = options_element_text(element, text);
    }
    while (*text);

    options_add_empty(state);
}

static bool options_prev_select(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, uint8_t flags)
{
    menu_state = state->prev_state;
    return menu_state->select(menu_state, element->flags|flags, element->element_no);
}

static void options_add_select(OPTIONS_STATE *state, const char *text, uint8_t flags, uint8_t element_no)
{
    OPTIONS_ELEMENT *element = options_add_text_element(state, options_prev_select, text);
    element->flags = flags;
    element->element_no = element_no;
}

static bool options_prev_dir(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, uint8_t flags)
{
    menu_state = state->prev_state;
    menu_state->dir(menu_state);
    return false;
}

static inline void options_add_dir(OPTIONS_STATE *state, const char *text)
{
    options_add_text_element(state, options_prev_dir, text);
}

static void handle_options(OPTIONS_STATE *state)
{
    menu_state = &state->menu;
    menu_state->dir(menu_state);
}
