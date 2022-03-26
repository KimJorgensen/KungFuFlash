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

static void sprint_u16_left(char *buffer, u16 val)
{
    u16 div = 10000;
    bool found = false;

    u8 ii = 0;
    for (u8 i=0; i<5; i++)
    {
        u8 d = (val / div) % 10;
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

static void d64_sanitize_name_pad(char *dest, const char *src, u8 size)
{
    for (u8 i=0; i<size; i++)
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

static void d64_format_diskname(char *buffer, const char *name, u8 name_len)
{
    // Star
    sprint(buffer, " *     ");
    buffer += 7;

    // Diskname
    d64_sanitize_name_pad(buffer, name, name_len);
    buffer += name_len;

    // Entry type
    const u8 entry_len = 27 - name_len;
    d64_sanitize_name_pad(buffer, "", entry_len);
    buffer += entry_len;
    sprint(buffer, "--- ");
}

static void d64_format_entry_type(char *buffer, u8 type)
{
    if (!(type & D64_FILE_NO_SPLAT))
    {
        *buffer++ = '*';   // to display really deleted files as "*DEL", $80 as " DEL"
    }
    else
    {
        *buffer++ = ' ';
    }

    strcpy(buffer, d64_types[type & 7]);
    buffer += 3;

    if (type & D64_FILE_LOCKED)
    {
        *buffer++ = '<';
    }
    else
    {
        *buffer++ = ' ';
    }
}

static void d64_format_entry(char *buffer, u16 blocks,
                             const char *filename, u8 type)
{
    // Blocks
    *buffer++ = ' ';
    sprint_u16_left(buffer, blocks);
    buffer += 5;
    *buffer++ = ' ';

    // Filename
    d64_sanitize_name_pad(buffer, filename, 16);
    buffer += 16;
    d64_sanitize_name_pad(buffer, "", 10);
    buffer += 10;

    // Entry type
    d64_format_entry_type(buffer, type);
}

static u8 d64_send_page(D64_STATE *state, u8 selected_element)
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
            char *diskname = d64_get_diskname(&state->d64);
            d64_format_diskname(scratch_buf, diskname, 23);
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

            d64_format_entry(scratch_buf, entry->blocks, entry->filename,
                             entry->type);
        }

        if (element == selected_element)
        {
            scratch_buf[0] = SELECTED_ELEMENT;
        }

        c64_send_data(scratch_buf, ELEMENT_LENGTH);
    }

    return element;
}

static u8 d64_handle_delete_file(D64_STATE *state, const char *file_name,
                                 D64_DIR_ENTRY *entry)
{
    sd_send_prg_message("Deleting file.");
    save_dat();

    if (!d64_delete_file(&state->d64, entry))
    {
        sd_send_warning_restart("Failed to delete file", file_name);
    }

    c64_interface_sync();
    return CMD_MENU;
}

static bool d64_skip_to_page(D64_STATE *state, u8 page)
{
    bool page_found = true;
    state->page = page;

    u16 skip = state->page * MAX_ELEMENTS_PAGE;
    for (u16 i=2; i<skip; i++)
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

static u8 d64_dir(D64_STATE *state)
{
    d64_rewind_dir(&state->d64);
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

        if (!d64_skip_to_page(state, page))
        {
            dat_file.prg.element = 0;
            selected_element = MAX_ELEMENTS_PAGE;
        }
    }

    d64_send_page(state, selected_element);
    return CMD_READ_DIR;
}

static u8 d64_dir_up(D64_STATE *state, bool root)
{
    dat_file.prg.element = ELEMENT_NOT_SELECTED;    // Do not auto open D64 again
    d64_close(&state->image);

    menu = &sd_menu;
    if (root)
    {
        return menu->dir_up(menu->state, root);
    }

    return menu->dir(menu->state);
}

static u8 d64_next_page(D64_STATE *state)
{
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

    return CMD_READ_DIR_PAGE;
}

static u8 d64_prev_page(D64_STATE *state)
{
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

    return CMD_READ_DIR_PAGE;
}

static u8 d64_select(D64_STATE *state, u8 flags, u8 element_no)
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

        return d64_dir_up(state, false);
    }

    if (element == 1)
    {
        if (flags & SELECT_FLAG_OPTIONS)
        {
            return handle_file_options("*", FILE_D64_STAR, element_no);
        }

        if (!(flags & SELECT_FLAG_MOUNT))
        {
            basic_load("*");
            dat_file.disk.mode = DISK_MODE_D64;
            dat_file.boot_type = DAT_DISK;
            d64_close(&state->image);
            return CMD_WAIT_SYNC;
        }

        element = ELEMENT_NOT_SELECTED; // Find first PRG
    }

    d64_rewind_dir(&state->d64);
    D64_DIR_ENTRY *entry = NULL;
    for (u16 i=2; i<=element; i++)
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
            return handle_unsupported_ex("Not Found",
                "No PRG files were found in image", dat_file.file);
        }

        dat_file.prg.element = 0;
        return d64_dir(state);
    }

    d64_sanitize_filename(dat_file.prg.name, entry->filename);
    dat_file.prg.name[16] = 0;

    if (flags & SELECT_FLAG_OPTIONS)
    {
        return handle_file_options(dat_file.prg.name, FILE_D64_PRG, element_no);
    }

    if (flags & SELECT_FLAG_DELETE)
    {
        return d64_handle_delete_file(state, dat_file.prg.name, entry);
    }

    if (!(flags & SELECT_FLAG_MOUNT))
    {
        if (!d64_is_valid_prg(entry))
        {
            return handle_unsupported(dat_file.prg.name);
        }

        basic_load(dat_file.prg.name);
        dat_file.disk.mode = DISK_MODE_D64;
        dat_file.boot_type = DAT_DISK;
        d64_close(&state->image);
        return CMD_WAIT_SYNC;
    }

    dat_file.prg.size = d64_read_prg(&state->d64, entry, dat_buffer,
                                     sizeof(dat_buffer));
    if (!prg_size_valid(dat_file.prg.size))
    {
        return handle_unsupported(dat_file.prg.name);
    }

    dat_file.boot_type = DAT_PRG;
    d64_close(&state->image);
    return CMD_WAIT_SYNC;
}

static const MENU d64_menu = {
    .state = &d64_state,
    .dir = (u8 (*)(void *))d64_dir,
    .dir_up = (u8 (*)(void *, bool))d64_dir_up,
    .prev_page = (u8 (*)(void *))d64_prev_page,
    .next_page = (u8 (*)(void *))d64_next_page,
    .select = (u8 (*)(void *, u8, u8))d64_select
};

static const MENU * d64_menu_init(const char *file_name)
{
    if (!d64_open(&d64_state.image, file_name))
    {
        fail_to_read_sd();
    }
    d64_state.d64.image = &d64_state.image;

    return &d64_menu;
}
