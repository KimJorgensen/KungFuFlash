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

static void sprint_u16_left(char *buffer, uint16_t val)
{
    uint16_t div = 10000;
    bool found = false;

    uint8_t ii = 0;
    for(uint8_t i=0; i<5; i++)
    {
        uint8_t d = (val / div) % 10;
        if (d || found || i==4)
        {
            found = true;
            buffer[ii++] = '0' + d;
        }
        else
        {
            buffer[4-i] = ' ';
        }

        div /= 10;
    }
}

static void d64_sanitize_filename(char *dest, const char *src)
{
    char c;
    while ((c = *src++) && c != 0xa0)
    {
        *dest++ = sanitize_char(c);
    }

    *dest = 0;
}

static void d64_sanitize_name_pad(char *dest, const char *src, uint8_t size)
{
    for(uint8_t i=0; i<size; i++)
    {
        char c = *src;
        if (c)
        {
            src++;
            *dest++ = sanitize_char(c);
        }
        else
        {
            *dest++ = ' ';
        }
    }
}

static void d64_format_diskname(char *buffer, D64 *d64)
{
    char *diskname = d64_get_diskname(d64);

    // Star
    sprint(scratch_buf, " *     ");
    buffer += 7;

    // Diskname
    const uint8_t diskname_len = 23;
    d64_sanitize_name_pad(buffer, diskname, diskname_len);
    buffer += diskname_len;

    // Entry type
    sprint(buffer, "    --- ");
}

static void d64_format_entry_type(char *buffer, uint8_t type)
{
    if(!(type & 0x80))
    {
        *buffer++ = '*';   // to display really deleted files as "*DEL", $80 as " DEL"
    }
    else
    {
        *buffer++ = ' ';
    }

    strcpy(buffer, d64_types[type & 7]);
    buffer += 3;

    if(type & 0x40)
    {
        *buffer++ = '<';
    }
    else
    {
        *buffer++ = ' ';
    }
}

static void d64_format_entry(char *buffer, D64_DIR_ENTRY *entry)
{
    // Blocks
    *buffer++ = ' ';
    sprint_u16_left(buffer, entry->blocks);
    buffer += 5;
    *buffer++ = ' ';

    // Filename
    const uint8_t filename_len = (ELEMENT_LENGTH-7)-5;
    d64_sanitize_name_pad(buffer, entry->filename, filename_len);
    buffer += filename_len;

    // Entry type
    d64_format_entry_type(buffer, entry->type);
}

static uint8_t d64_send_page(D64_STATE *state, uint8_t selected_element)
{
    uint8_t element;
    for (element=0; element<MAX_ELEMENTS_PAGE; element++)
    {
        if (element == 0 && state->page == 0)
        {
            sprint(scratch_buf, " ..                               DIR ");
        }
        else if (element == 1 && state->page == 0)
        {
            d64_format_diskname(scratch_buf, &state->d64);
        }
        else
        {
            D64_DIR_ENTRY *entry;
            if (!(entry = d64_read_dir(&state->d64)))
            {
                state->dir_end = true;
                send_page_end();
                break;
            }

            d64_format_entry(scratch_buf, entry);
        }

        if (element == selected_element)
        {
            scratch_buf[0] = SELECTED_ELEMENT;
        }

        c64_send_data(scratch_buf, ELEMENT_LENGTH);
    }

    return element;
}

static void d64_handle_delete_file(const char *file_name, D64_DIR_ENTRY *entry)
{
    c64_send_exit_menu();
    c64_send_prg_message("Deleting file.");
    c64_interface(false);
    save_dat();

    if (!d64_delete_file(&d64_state.d64, entry))
    {
        sd_send_warning_restart("Failed to delete file", file_name);
    }

    c64_interface(true);
    c64_send_reset_to_menu();
}

static bool d64_skip_to_page(D64_STATE *state, uint8_t page)
{
    bool page_found = true;
    state->page = page;

    uint16_t skip = state->page * MAX_ELEMENTS_PAGE;
    for (uint16_t i=2; i<skip; i++)
    {
        if (!d64_read_dir(&state->d64))
        {
            page_found = false;
            break;
        }
    }

    if (!page_found)
    {
        d64_rewind_dir(&state->d64);
        state->page = 0;
    }

    return page_found;
}

static void d64_dir(D64_STATE *state)
{
    c64_send_reply(REPLY_READ_DIR);

    d64_rewind_dir(&state->d64);
    format_path(scratch_buf, true);
    c64_send_data(scratch_buf, DIR_NAME_LENGTH);
    state->dir_end = false;

    // Search for last selected element
    uint8_t selected_element;
    if (dat_file.prg.element == ELEMENT_NOT_SELECTED)
    {
        state->page = 0;
        dat_file.prg.element = 0;
        selected_element = MAX_ELEMENTS_PAGE;
    }
    else
    {
        selected_element = dat_file.prg.element % MAX_ELEMENTS_PAGE;
        uint8_t page = dat_file.prg.element / MAX_ELEMENTS_PAGE;

        if (!d64_skip_to_page(state, page))
        {
            dat_file.prg.element = 0;
            selected_element = MAX_ELEMENTS_PAGE;
        }
    }

    d64_send_page(state, selected_element);
}

static void d64_dir_up(D64_STATE *state, bool root)
{
    dat_file.prg.element = ELEMENT_NOT_SELECTED;    // Do not auto open D64 again
    d64_close(&d64_state.image);

    menu_state = &sd_state.menu;
    if (root)
    {
        menu_state->dir_up(menu_state, root);
    }
    else
    {
        menu_state->dir(menu_state);
    }
}

static void d64_next_page(D64_STATE *state)
{
    c64_send_reply(REPLY_READ_DIR_PAGE);

    if (!state->dir_end)
    {
        state->page++;
        if (!d64_send_page(state, MAX_ELEMENTS_PAGE))
        {
            state->page--;
        }
    }
    else
    {
        send_page_end();
    }
}

static void d64_prev_page(D64_STATE *state)
{
    c64_send_reply(REPLY_READ_DIR_PAGE);

    if (state->page)
    {
        d64_rewind_dir(&state->d64);
        state->dir_end = false;

        d64_skip_to_page(state, state->page-1);
        d64_send_page(state, MAX_ELEMENTS_PAGE);
    }
    else
    {
        send_page_end();
    }
}

static bool d64_select(D64_STATE *state, uint8_t flags, uint8_t element_no)
{
    uint16_t element = element_no + state->page * MAX_ELEMENTS_PAGE;

    dat_file.prg.element = element;
    dat_file.boot_type = DAT_NONE;
    dat_file.prg.name[0] = 0;

    if (element == 0)
    {
        if (flags & SELECT_FLAG_OPTIONS)
        {
            handle_file_options("..", FILE_NONE, element_no);
        }
        else
        {
            d64_dir_up(state, false);
        }

        return false;
    }
    else if (element == 1)
    {
        if (flags & SELECT_FLAG_OPTIONS)
        {
            handle_file_options("*", FILE_D64_STAR, element_no);
            return false;
        }
        else if (!(flags & SELECT_FLAG_MOUNT))
        {
            basic_load("*");
            dat_file.boot_type = DAT_DISK;
            d64_close(&d64_state.image);
            return true;
        }

        element = ELEMENT_NOT_SELECTED; // Find first PRG
    }

    d64_rewind_dir(&state->d64);
    D64_DIR_ENTRY *entry = NULL;
    for (uint16_t i=2; i<=element; i++)
    {
        if (!(entry = d64_read_dir(&state->d64)))
        {
            break;
        }

        if (element == ELEMENT_NOT_SELECTED && d64_is_valid_prg(entry))
        {
            element = 1;
            break;
        }
    }

    if (!entry)
    {
        // File not not found
        if (element == 1 || element == ELEMENT_NOT_SELECTED)
        {
            dat_file.prg.element = 1;
            handle_unsupported_ex("Not Found", "No PRG files were found on disk",
                                  dat_file.file);
        }
        else
        {
            dat_file.prg.element = 0;
            d64_dir(state);
        }

        return false;
    }

    d64_sanitize_filename(dat_file.prg.name, entry->filename);
    dat_file.prg.name[16] = 0;

    if (flags & SELECT_FLAG_OPTIONS)
    {
        handle_file_options(dat_file.prg.name, FILE_D64_PRG, element_no);
        return false;
    }
    else if (flags & SELECT_FLAG_DELETE)
    {
        d64_handle_delete_file(dat_file.prg.name, entry);
        return false;
    }
    else if (!(flags & SELECT_FLAG_MOUNT))
    {
        if (!d64_is_valid_prg(entry))
        {
            handle_unsupported(dat_file.prg.name);
            return false;
        }

        basic_load(dat_file.prg.name);
        dat_file.boot_type = DAT_DISK;
        d64_close(&d64_state.image);
        return true;
    }

    dat_file.prg.size = d64_read_prg(&state->d64, entry, dat_buffer,
                                     sizeof(dat_buffer));
    if (!prg_size_valid(dat_file.prg.size))
    {
        handle_unsupported(dat_file.prg.name);
        return false;
    }

    c64_send_exit_menu();
    dat_file.boot_type = DAT_PRG;
    d64_close(&d64_state.image);
    return true;
}

static MENU_STATE * d64_menu_init(const char *file_name)
{
    if (!d64_state.menu.dir)
    {
        d64_state.menu.dir = (void (*)(MENU_STATE *))d64_dir;
        d64_state.menu.dir_up = (void (*)(MENU_STATE *, bool))d64_dir_up;
        d64_state.menu.prev_page = (void (*)(MENU_STATE *))d64_prev_page;
        d64_state.menu.next_page = (void (*)(MENU_STATE *))d64_next_page;
        d64_state.menu.select = (bool (*)(MENU_STATE *, uint8_t, uint8_t))d64_select;
    }

    if (!d64_open(&d64_state.image, file_name))
    {
        handle_failed_to_read_sd();
    }
    d64_state.d64.image = &d64_state.image;

    return &d64_state.menu;
}
