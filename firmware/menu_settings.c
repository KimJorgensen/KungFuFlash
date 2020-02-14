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

static uint8_t settings_flags;

static const char * settings_basic_text(void)
{
    sprint(scratch_buf, "Persist BASIC selection: %s",
           (settings_flags & DAT_FLAG_PERSIST_BASIC) ? "yes" : "no");

    return scratch_buf;
}

static bool settings_basic_change(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, uint8_t flags)
{
    if (settings_flags & DAT_FLAG_PERSIST_BASIC)
    {
        settings_flags &= ~DAT_FLAG_PERSIST_BASIC;
    }
    else
    {
        settings_flags |= DAT_FLAG_PERSIST_BASIC;
    }

    options_element_text(element, settings_basic_text());
    menu_state->dir(menu_state); // Refresh settings
    return false;
}

static bool settings_save(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, uint8_t flags)
{
    dat_file.flags = settings_flags;

    c64_send_exit_menu();
    c64_send_prg_message("Saving settings.");
    c64_interface(false);
    save_dat();
    restart_to_menu();

    return false;
}

static void handle_settings(void)
{
    settings_flags = dat_file.flags;

    OPTIONS_STATE *options = options_init("Settings");
    options_add_text_element(options, settings_basic_change, settings_basic_text());
    options_add_text_element(options, settings_save, "Save");
    options_add_dir(options, "Cancel");
    handle_options(options);
}
