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
static void sd_format_size(char *buffer, uint32_t size)
{
    char units[] = "BKmg";
    char *unit = units;

    while (size >= 1000)
    {
        size += 24;
        size /= 1024;
        unit++;
    }

    if (size < 100)
    {
        *buffer++ = ' ';
    }
    if (size < 10)
    {
        *buffer++ = ' ';
    }

    sprint(buffer, "%u%c ", size, *unit);
}

static void sd_format_element(char *buffer, FILINFO *info)
{
    *buffer++ = ' ';

    // Element name
    const uint8_t filename_len = (ELEMENT_LENGTH-1)-6;
    to_petscii_pad(buffer, info->fname, filename_len);
    buffer += filename_len;
    *buffer++ = ' ';

    // Element type or size
    if (info->fattrib & AM_DIR)
    {
        sprint(buffer, " dir ");
    }
    else
    {
        sd_format_size(buffer, info->fsize);
    }
}

static uint8_t sd_send_page(SD_STATE *state, uint8_t selected_element)
{
    bool send_dot_dot = !state->in_root && state->page_no == 0;

    FILINFO file_info;
    uint8_t element;
    for (element=0; element<MAX_ELEMENTS_PAGE; element++)
    {
        if (send_dot_dot)
        {
            file_info.fname[0] = '.';
            file_info.fname[1] = '.';
            file_info.fname[2] = 0;
            file_info.fattrib = AM_DIR;
            send_dot_dot = false;
        }
        else
        {
            if (!dir_read(&state->end_page, &file_info))
            {
                file_info.fname[0] = 0;
            }

            if (!file_info.fname[0])
            {
                // End of dir
                dir_close(&state->end_page);
                state->dir_end = true;

                send_page_end();
                break;
            }

        }

        sd_format_element(scratch_buf, &file_info);
        if (element == selected_element)
        {
            scratch_buf[0] = SELECTED_ELEMENT;
        }

        c64_send_data(scratch_buf, ELEMENT_LENGTH);
    }

    return element;
}

static void handle_dir_open(SD_STATE *state)
{
    if (!dir_open(&state->start_page, ""))
    {
        handle_failed_to_read_sd();
    }

    state->end_page = state->start_page;
    state->page_no = 0;
    state->dir_end = false;
}

static void handle_dir_command(SD_STATE *state)
{
    handle_dir_open(state);

    dir_current(dat_file.path, sizeof(dat_file.path));
    state->in_root = format_path(scratch_buf, false);
    dbg("Reading path %s\n", dat_file.path);

    // Search for last selected element
    FILINFO file_info;
    bool found = false;
    uint8_t selected_element = MAX_ELEMENTS_PAGE;

    if (dat_file.file[0])
    {
        DIR first_page = state->start_page;
        while (true)
        {
            uint8_t element = 0;
            if (!state->in_root && state->page_no == 0)
            {
                element++;
            }

            for (; element<MAX_ELEMENTS_PAGE; element++)
            {
                if (!dir_read(&state->end_page, &file_info))
                {
                    file_info.fname[0] = 0;
                }

                if (!file_info.fname[0])
                {
                    break;
                }

                if (strncmp(dat_file.file, file_info.fname, sizeof(dat_file.file)) == 0)
                {
                    found = true;
                    break;
                }
            }

            if (found)
            {
                state->end_page = state->start_page;
                selected_element = element;
                break;
            }

            if (!file_info.fname[0])
            {
                state->start_page = first_page;
                state->end_page = first_page;
                state->page_no = 0;
                break;
            }

            state->start_page = state->end_page;
            state->page_no++;
        }
    }

    if (found && get_file_type(&file_info) == FILE_D64 &&
        dat_file.prg.element != ELEMENT_NOT_SELECTED)
    {
        menu_state = d64_menu_init(file_info.fname);
        menu_state->dir(menu_state);
        return;
    }

    c64_send_reply(REPLY_READ_DIR);
    c64_send_data(scratch_buf, DIR_NAME_LENGTH);
    sd_send_page(state, selected_element);
}

static void handle_change_dir(SD_STATE *state, char *path, bool select_old)
{
    if (select_old && !state->in_root)
    {
        uint16_t dir_index = 0;
        uint16_t len;
        for (len = 0; len < sizeof(dat_file.path); len++)
        {
            char c = dat_file.path[len];
            if (!c)
            {
                break;
            }
            if (c == '/')
            {
                dir_index = len+1;
            }
        }

        memcpy(dat_file.file, dat_file.path + dir_index, len - dir_index);
        dat_file.file[len - dir_index] = 0;
    }
    else
    {
        dat_file.file[0] = 0;
    }

    if (!dir_change(path))
    {
        handle_failed_to_read_sd();
    }

    handle_dir_command(state);
}

static void handle_dir_next_page_command(SD_STATE *state)
{
    c64_send_reply(REPLY_READ_DIR_PAGE);

    if (!state->dir_end)
    {
        DIR start = state->end_page;
        state->page_no++;

        if (sd_send_page(state, MAX_ELEMENTS_PAGE) > 0)
        {
            state->start_page = start;
        }
        else
        {
            state->page_no--;
        }
    }
    else
    {
        send_page_end();
    }
}

static void handle_dir_prev_page_command(SD_STATE *state)
{
    if (state->page_no)
    {
        uint16_t target_page = state->page_no-1;
        bool not_found = false;

        handle_dir_open(state);
        c64_send_reply(REPLY_READ_DIR_PAGE);

        uint16_t elements_to_skip = MAX_ELEMENTS_PAGE * target_page;
        if (elements_to_skip && !state->in_root) elements_to_skip--;

        while (elements_to_skip--)
        {
            FILINFO file_info;
            if (!dir_read(&state->end_page, &file_info))
            {
                file_info.fname[0] = 0;
            }

            if (!file_info.fname[0])
            {
                not_found = true;
                break;
            }
        }

        if (not_found)
        {
            state->end_page = state->start_page;
        }
        else
        {
            state->page_no = target_page;
            state->start_page = state->end_page;
        }

        sd_send_page(state, MAX_ELEMENTS_PAGE);
    }
    else
    {
        reply_page_end();
    }
}

static void handle_file_open(FIL *file, const char *file_name)
{
    if (!file_open(file, file_name, FA_READ))
    {
        handle_failed_to_read_sd();
    }
}

static bool handle_load_file(const char *file_name, uint8_t file_type, uint8_t flags, uint8_t element)
{
    bool exit_menu = false;

    switch (file_type)
    {
        case FILE_PRG:
        case FILE_P00:
        {
            FIL file;
            handle_file_open(&file, file_name);
            dat_file.prg.name[0] = 0;

            bool prg_loaded;
            if (file_type == FILE_PRG)
            {
                prg_loaded = prg_load_file(&file);
            }
            else
            {
                prg_loaded = p00_load_file(&file);
            }

            if (prg_loaded)
            {
                c64_send_exit_menu();
                dat_file.prg.element = 0;
                dat_file.boot_type = DAT_PRG;
                exit_menu = true;
            }
            else
            {
                handle_unsupported(file_name);
            }

            file_close(&file);
        }
        break;

        case FILE_CRT:
        {
            FIL file;
            handle_file_open(&file, file_name);

            CRT_HEADER header;
            if (!crt_load_header(&file, &header))
            {
                handle_unsupported(file_name);
                break;
            }

            if (!crt_is_supported(header.cartridge_type))
            {
                sprint(scratch_buf, "Unsupported CRT type (%u)", header.cartridge_type);
                handle_unsupported_ex("Unsupported", scratch_buf, dat_file.file);
                break;
            }

            c64_send_exit_menu();
            c64_send_prg_message("Programming flash memory.");
            c64_interface(false);

            uint8_t banks = crt_program_file(&file);
            if (!banks)
            {
                c64_interface(true);
                sprint(scratch_buf, "Failed to read CRT file\r\n\r\n%s", file_name);
                c64_send_warning(scratch_buf);
                c64_send_reset_to_menu();
                break;
            }

            uint32_t flash_hash = crt_calc_flash_crc(banks);
            dat_file.crt.type = header.cartridge_type;
            dat_file.crt.exrom = header.exrom;
            dat_file.crt.game = header.game;
            dat_file.crt.banks = banks;
            dat_file.crt.flash_hash = flash_hash;
            dat_file.boot_type = DAT_CRT;
            exit_menu = true;
        }
        break;

        case FILE_D64:
        {
            if (flags & SELECT_FLAG_MOUNT)
            {
                basic_no_commands();
                dat_file.boot_type = DAT_DISK;
                exit_menu = true;
            }
            else
            {
                dat_file.prg.element = 0;
                menu_state = d64_menu_init(file_name);
                menu_state->dir(menu_state);
            }
        }
        break;

        case FILE_UPD:
        {
            if (!(flags & SELECT_FLAG_ACCEPTED))
            {
                FIL file;
                handle_file_open(&file, file_name);

                char firmware[FW_NAME_SIZE];
                if (upd_load(&file, firmware))
                {
                    handle_upgrade_menu(firmware, element);
                }
                else
                {
                    handle_unsupported(file_name);
                }

                file_close(&file);
            }
            else
            {
                c64_send_exit_menu();
                c64_send_prg_message("Upgrading firmware.");
                c64_interface(false);

                upd_program();
                save_dat();
                restart_to_menu();
            }
        }
        break;

        case FILE_DAT:
        {
            handle_unsupported_ex("System File", "This file is used by Kung Fu Flash", file_name);
        }
        break;

        default:
        {
            handle_unsupported(file_name);
        }
        break;
    }

    return exit_menu;
}

static bool handle_select_command(SD_STATE *state, uint8_t flags, uint8_t element)
{
    bool exit_menu = false;
    uint8_t element_no = element;

    dat_file.boot_type = DAT_NONE;
    dat_file.prg.element = ELEMENT_NOT_SELECTED;    // don't auto open D64
    dat_file.file[0] = 0;

    if (!state->in_root && state->page_no == 0)
    {
        if (element_no == 0)
        {
            if (flags & SELECT_FLAG_OPTIONS)
            {
                handle_file_options("..", FILE_NONE, element);
            }
            else
            {
                handle_change_dir(state, "..", true);
            }

            return exit_menu;
        }

        element_no--;
    }

    FILINFO file_info;
    DIR dir = state->start_page;
    for (uint8_t i=0; i<=element_no; i++)
    {
        if (!dir_read(&dir, &file_info))
        {
            handle_failed_to_read_sd();
        }

        if (!file_info.fname[0]) // End of dir
        {
            break;
        }
    }

    dir_close(&dir);

    if (file_info.fname[0] == 0)
    {
        // File not not found
        handle_dir_command(state);
        return exit_menu;
    }

    uint8_t file_type = get_file_type(&file_info);
    strcpy(dat_file.file, file_info.fname);

    if (flags & SELECT_FLAG_OPTIONS)
    {
        handle_file_options(file_info.fname, file_type, element);
    }
    else if (file_type == FILE_NONE)
    {
        handle_change_dir(state, file_info.fname, false);
    }
    else
    {
        exit_menu = handle_load_file(file_info.fname, file_type, flags, element);
    }

    return exit_menu;
}

static void handle_dir_up_command(SD_STATE *state, bool root)
{
    if (root)
    {
        handle_change_dir(state, "/", false);
    }
    else
    {
        handle_change_dir(state, "..", true);
    }
}

static MENU_STATE *sd_menu_init(void)
{
    if (!sd_state.menu.dir)
    {
        sd_state.menu.dir = (void (*)(MENU_STATE *))handle_dir_command;
        sd_state.menu.dir_up = (void (*)(MENU_STATE *, bool))handle_dir_up_command;
        sd_state.menu.prev_page = (void (*)(MENU_STATE *))handle_dir_prev_page_command;
        sd_state.menu.next_page = (void (*)(MENU_STATE *))handle_dir_next_page_command;
        sd_state.menu.select = (bool (*)(MENU_STATE *, uint8_t, uint8_t))handle_select_command;
    }

    chdir_last();
    sd_state.page_no = 0;
    sd_state.dir_end = true;

    return &sd_state.menu;
}
