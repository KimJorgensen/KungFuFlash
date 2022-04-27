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

static OPTIONS_ELEMENT *elements = (OPTIONS_ELEMENT *)(scratch_buf +
                                   (sizeof(scratch_buf)/2));

static u8 options_dir(OPTIONS_STATE *state)
{
    scratch_buf[0] = ' ';
    to_petscii_pad(scratch_buf + 1, state->title, DIR_NAME_LENGTH-1);
    c64_send_data(scratch_buf, DIR_NAME_LENGTH);

    for (u8 i=0; i<state->no_of_elements; i++)
    {
        OPTIONS_ELEMENT *element = elements + i;
        if (state->selected_element == i)
        {
            element->text[0] = SELECTED_ELEMENT;
        }
        else if (element->text[0] == SELECTED_ELEMENT)
        {
            element->text[0] = ' ';
        }

        c64_send_data(element->text, ELEMENT_LENGTH);
    }

    send_page_end();
    return CMD_READ_DIR;
}

static u8 options_dir_up(OPTIONS_STATE *state, bool root)
{
    menu = state->prev_menu;
    if (root)
    {
        return menu->dir_up(menu->state, root);
    }

    return menu->dir(menu->state);
}

static u8 options_prev_next_page(OPTIONS_STATE *state)
{
    return handle_page_end();
}

static u8 options_select(OPTIONS_STATE *state, u8 flags, u8 element_no)
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

    return options_dir(state);
}

static const MENU options_menu = {
    .state = &options_state,
    .dir = (u8 (*)(void *))options_dir,
    .dir_up = (u8 (*)(void *, bool))options_dir_up,
    .prev_page = (u8 (*)(void *))options_prev_next_page,
    .next_page = (u8 (*)(void *))options_prev_next_page,
    .select = (u8 (*)(void *, u8, u8))options_select
};

static OPTIONS_STATE * options_init(const char *title)
{
    // Can't nest options
    if (menu != &options_menu)
    {
        options_state.prev_menu = menu;
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
        wrn("Too many options on page. Overwriting last element");
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

static u8 options_callback(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, u8 flags)
{
    void (*callback)(u8) = element->user_state;
    callback(element->flags|flags);
    return CMD_NONE;
}

static void options_add_callback(OPTIONS_STATE *state, void (*callback)(u8), const char *text, u8 flags)
{
    OPTIONS_ELEMENT *element = options_add_text_element(state, options_callback, text);
    element->flags = flags;
    element->user_state = callback;
}

static u8 options_prev_select(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, u8 flags)
{
    menu = state->prev_menu;
    return menu->select(menu->state, element->flags|flags, element->element_no);
}

static void options_add_select(OPTIONS_STATE *state, const char *text, u8 flags, u8 element_no)
{
    OPTIONS_ELEMENT *element = options_add_text_element(state, options_prev_select, text);
    element->flags = flags;
    element->element_no = element_no;
}

static u8 options_prev_dir(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, u8 flags)
{
    menu = state->prev_menu;
    return menu->dir(menu->state);
}

static inline void options_add_dir(OPTIONS_STATE *state, const char *text)
{
    options_add_text_element(state, options_prev_dir, text);
}

static u8 handle_options(void)
{
    menu = &options_menu;
    return menu->dir(menu->state);
}
