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

static void t64_format_entry(char *buffer, T64_ENTRY *entry)
{
    u16 size = (entry->end_address - entry->start_address) + 2;
    u16 blocks = (size / 254) + 1;

    d64_format_entry(buffer, blocks, entry->filename, entry->file_type);
}

static u8 t64_send_page(T64_STATE *state, u8 selected_element)
{
    u8 element;
    for (element=0; element<MAX_ELEMENTS_PAGE; element++)
    {
        if (element == 0 && state->page == 0)
        {
            sprint(scratch_buf, " ..                               DIR ");
        }
        else if (element == 1 && state->page == 0)
        {
            char *image_name = state->image.header.user_description;
            d64_format_diskname(scratch_buf, image_name, 24);
        }
        else
        {
            if (!t64_read_dir(&state->image))
            {
                state->dir_end = true;
                send_page_end();
                break;
            }

            t64_format_entry(scratch_buf, &state->image.entry);
        }

        if (element == selected_element)
        {
            scratch_buf[0] = SELECTED_ELEMENT;
        }

        c64_send_data(scratch_buf, ELEMENT_LENGTH);
    }

    return element;
}

static bool t64_skip_to_page(T64_STATE *state, u8 page)
{
    bool page_found = true;
    state->page = page;

    u16 skip = state->page * MAX_ELEMENTS_PAGE;
    for (u16 i=2; i<skip; i++)
    {
        if (!t64_read_dir(&state->image))
        {
            page_found = false;
            break;
        }
    }

    if (!page_found)
    {
        t64_rewind_dir(&state->image);
        state->page = 0;
    }

    return page_found;
}

static u8 t64_dir(T64_STATE *state)
{
    t64_rewind_dir(&state->image);
    format_path(scratch_buf, true);
    c64_send_data(scratch_buf, DIR_NAME_LENGTH);
    state->dir_end = false;

    // Search for last selected element
    u8 selected_element;
    if (dat_file.prg.element == ELEMENT_NOT_SELECTED)
    {
        state->page = 0;
        dat_file.prg.element = 0;
        selected_element = MAX_ELEMENTS_PAGE;
    }
    else
    {
        selected_element = dat_file.prg.element % MAX_ELEMENTS_PAGE;
        u8 page = dat_file.prg.element / MAX_ELEMENTS_PAGE;

        if (!t64_skip_to_page(state, page))
        {
            dat_file.prg.element = 0;
            selected_element = MAX_ELEMENTS_PAGE;
        }
    }

    t64_send_page(state, selected_element);
    return CMD_READ_DIR;
}

static u8 t64_dir_up(T64_STATE *state, bool root)
{
    dat_file.prg.element = ELEMENT_NOT_SELECTED;    // Do not auto open T64 again
    t64_close(&state->image);

    menu = &sd_menu;
    if (root)
    {
        return menu->dir_up(menu->state, root);
    }

    return menu->dir(menu->state);
}

static u8 t64_next_page(T64_STATE *state)
{
    if (!state->dir_end)
    {
        state->page++;
        if (!t64_send_page(state, MAX_ELEMENTS_PAGE))
        {
            state->page--;
        }
    }
    else
    {
        send_page_end();
    }

    return CMD_READ_DIR_PAGE;
}

static u8 t64_prev_page(T64_STATE *state)
{
    if (state->page)
    {
        t64_rewind_dir(&state->image);
        state->dir_end = false;

        t64_skip_to_page(state, state->page-1);
        t64_send_page(state, MAX_ELEMENTS_PAGE);
    }
    else
    {
        send_page_end();
    }

    return CMD_READ_DIR_PAGE;
}

static u8 t64_select(T64_STATE *state, u8 flags, u8 element_no)
{
    u16 element = element_no + state->page * MAX_ELEMENTS_PAGE;

    dat_file.prg.element = element;
    dat_file.boot_type = DAT_NONE;
    dat_file.prg.name[0] = 0;

    if (element == 0)
    {
        if (flags & SELECT_FLAG_OPTIONS)
        {
            return handle_file_options("..", FILE_DIR_UP, element_no);
        }

        return t64_dir_up(state, false);
    }

    if (element == 1)
    {
        if (flags & SELECT_FLAG_OPTIONS)
        {
            return handle_file_options("*", FILE_T64_PRG, element_no);
        }

        element = ELEMENT_NOT_SELECTED; // Find first PRG
    }

    t64_rewind_dir(&state->image);
    bool found = false;
    for (u16 i=2; i<=element; i++)
    {
        if (!(found = t64_read_dir(&state->image)))
        {
            break;
        }

        if (element == ELEMENT_NOT_SELECTED)
        {
            element = 1;
            break;
        }
    }

    if (!found)
    {
        // File not not found
        if (element == 1 || element == ELEMENT_NOT_SELECTED)
        {
            dat_file.prg.element = 1;
            return handle_unsupported_ex("Not Found",
                "No PRG files were found in image", dat_file.file);
        }

        dat_file.prg.element = 0;
        return t64_dir(state);
    }

    d64_sanitize_name_pad(dat_file.prg.name, state->image.entry.filename, 16);
    dat_file.prg.name[16] = 0;

    if (flags & SELECT_FLAG_OPTIONS)
    {
        return handle_file_options(dat_file.prg.name, FILE_T64_PRG, element_no);
    }

    dat_file.prg.size = t64_read_prg(&state->image, dat_buffer,
                                     sizeof(dat_buffer));
    if (!prg_size_valid(dat_file.prg.size))
    {
        return handle_unsupported(dat_file.prg.name);
    }

    dat_file.boot_type = DAT_PRG;
    t64_close(&state->image);
    return CMD_WAIT_SYNC;
}

static void t64_open_image(const char *file_name)
{
    if (!t64_open(&t64_state.image, file_name))
    {
        fail_to_read_sd();
    }
}

static u8 t64_load_first(const char *file_name)
{
    t64_state.page = 0;
    t64_open_image(file_name);

    return t64_select(&t64_state, 0, 1);
}

static const MENU t64_menu = {
    .state = &t64_state,
    .dir = (u8 (*)(void *))t64_dir,
    .dir_up = (u8 (*)(void *, bool))t64_dir_up,
    .prev_page = (u8 (*)(void *))t64_prev_page,
    .next_page = (u8 (*)(void *))t64_next_page,
    .select = (u8 (*)(void *, u8, u8))t64_select
};

static const MENU * t64_menu_init(const char *file_name)
{
    t64_open_image(file_name);
    return &t64_menu;
}
