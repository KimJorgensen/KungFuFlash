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

static u8 settings_flags;

static u8 settings_refresh(OPTIONS_ELEMENT *element, const char *text)
{
    options_element_text(element, text);
    return menu->dir(menu->state);  // Refresh settings
}

static const char * settings_basic_text(void)
{
    sprint(scratch_buf, "Persist BASIC selection: %s",
           (settings_flags & DAT_FLAG_PERSIST_BASIC) ? "yes" : "no");

    return scratch_buf;
}

static u8 settings_basic_change(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, u8 flags)
{
    if (settings_flags & DAT_FLAG_PERSIST_BASIC)
    {
        settings_flags &= ~DAT_FLAG_PERSIST_BASIC;
    }
    else
    {
        settings_flags |= DAT_FLAG_PERSIST_BASIC;
    }

    return settings_refresh(element, settings_basic_text());
}

static const char * settings_autostart_text(void)
{
    sprint(scratch_buf, "Autostart disk image: %s",
           (settings_flags & DAT_FLAG_AUTOSTART_D64) ? "yes" : "no");

    return scratch_buf;
}

static u8 settings_autostart_change(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, u8 flags)
{
    if (settings_flags & DAT_FLAG_AUTOSTART_D64)
    {
        settings_flags &= ~DAT_FLAG_AUTOSTART_D64;
    }
    else
    {
        settings_flags |= DAT_FLAG_AUTOSTART_D64;
    }

    return settings_refresh(element, settings_autostart_text());
}

static const char * settings_device_text(void)
{
    sprint(scratch_buf, "Disk device number: %u", get_device_number(settings_flags));
    return scratch_buf;
}

static u8 settings_device_change(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, u8 flags)
{
    u8 device = get_device_number(settings_flags) + 1;
    set_device_number(&settings_flags, device);

    return settings_refresh(element, settings_device_text());
}

static u8 settings_save(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, u8 flags)
{
    dat_file.flags = settings_flags;

    sd_send_prg_message("Saving settings.");
    save_dat();
    restart_to_menu();

    return CMD_NONE;
}

static u8 handle_settings(void)
{
    settings_flags = dat_file.flags;

    OPTIONS_STATE *options = options_init("Settings");
    options_add_text_element(options, settings_basic_change, settings_basic_text());
    options_add_text_element(options, settings_autostart_change, settings_autostart_text());
    options_add_text_element(options, settings_device_change, settings_device_text());
    options_add_text_element(options, settings_save, "Save");
    options_add_dir(options, "Cancel");
    return handle_options();
}
