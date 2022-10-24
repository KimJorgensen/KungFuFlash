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

#define COLOR_PURPLE        (4)
#define COLOR_YELLOW        (7)
#define COLOR_LIGHTRED      (10)
#define COLOR_LIGHTGREEN    (13)

static const char diag_header[] = "Diagnostic\r\n";
static const char diag_underline[] = "\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3";
static const char diag_saving[] = "Saving settings.";

static u32 diag_button;
static u32 diag_wait_timeout;

static u32 diag_button_pressed(void)
{
    return (diag_button = button_pressed());
}

static void diag_button_wait_release(void)
{
    while (diag_button_pressed());
}

static void usb_send_text(const char *text)
{
    for (const char *p = text; *p && usb_can_putc(); )
    {
        usb_putc(*p++);
    }
}

static bool diag_timeout_handler(void)
{
    if (!diag_wait_timeout)
    {
        return false;
    }
    diag_wait_timeout--;

    u32 button_prev = diag_button;
    if (diag_button_pressed() && !button_prev)
    {
        diag_wait_timeout = 0;
        c64_reset(true);
        return false;
    }

    return true;
}

static bool diag_send_result(bool saving)
{
    static u16 cnt = 0;

    // Restore handler to display result on C64
    C64_INSTALL_HANDLER(kff_handler);

    if (c64_is_reset())
    {
        diag_wait_timeout = 150;

        kff_init();
        c64_interface_enable_no_check();
        c64_reset(false);

        sprint(scratch_buf, "%s\r\n\r\n\r\nPhi2 offset   :\r\n\r\nPhi2 frequency:",
            diag_header);
        c64_send_message(scratch_buf);
        c64_send_text(COLOR_PURPLE, 0, 4, diag_underline);
    }
    else
    {
        diag_wait_timeout = 20;

        // c64_interface_sync without phi2 check
        c64_set_command(CMD_SYNC);
        c64_interface_enable_no_check();
        c64_wait_for_command(CMD_SYNC);
    }

    // Send result to USB and C64
    sprint(scratch_buf, "%d   ", dat_file.phi2_offset);
    usb_send_text(scratch_buf);
    c64_send_text(dat_file.phi2_offset ? COLOR_YELLOW : COLOR_LIGHTGREEN,
                  16, 7, scratch_buf);

    if (!saving)
    {
        if (diag_state == DIAG_STOP)
        {
            sprint(scratch_buf, "%u Hz (%u)  ", diag_phi2_freq, cnt++);
        }
        else
        {
            cnt = 0;
            sprint(scratch_buf, "");
        }
        sprint(scratch_buf, "%24s\r\n", scratch_buf);
        usb_send_text(scratch_buf);
        c64_send_text_wait(COLOR_LIGHTGREEN, 16, 9, scratch_buf);
    }
    else
    {
        sprint(scratch_buf, "%40s\r\n", diag_saving);
        usb_send_text(scratch_buf);
        c64_send_text_wait(COLOR_LIGHTRED, 0, 9, scratch_buf);
    }

    c64_interface(false);
    return !c64_is_reset(); // True if no new button press is detected
}

NO_RETURN diag_save_and_restart(s8 offset)
{
    load_dat();
    dat_file.phi2_offset = offset;
    save_dat();

    diag_send_result(true);

    led_off();
    diag_button_wait_release();
    system_restart();
}

static void diag_handle_button(void)
{
    u32 i = 0;
    bool long_press = false;

    // Detect long press
    u32 button = diag_button;
    while (diag_button_pressed())
    {
        button |= diag_button;
        if (++i > 100)
        {
            long_press = true;
            break;
        }
    }

    if (!long_press)
    {
        if (button & MENU_BTN)
        {
            dat_file.phi2_offset++;
        }

        if (button & SPECIAL_BTN)
        {
            dat_file.phi2_offset--;
        }

        c64_reset(true);    // Reset C64 after phi2 adjustment
    }
    else
    {
        if (button & MENU_BTN)
        {
            dat_file.phi2_offset = 0;
            c64_reset(true);    // Reset C64 after phi2 adjustment
        }

        if (button & SPECIAL_BTN)
        {
            diag_save_and_restart(dat_file.phi2_offset);
        }
    }

    if (diag_send_result(false))
    {
        led_off();
        diag_button_wait_release();
    }
}

static void diag_loop(void)
{
    usb_send_text(diag_header);
    c64_wait_handler = diag_timeout_handler;

    if (diag_button_pressed() & SPECIAL_BTN)
    {
        diag_save_and_restart(0);   // Reset and save offset
    }

    diag_send_result(false);
    led_off();
    diag_button_wait_release();

    while (true)
    {
        // Install IRQ handler to measure the Phi2 frequency
        diag_state = DIAG_INIT;
        C64_INSTALL_HANDLER(c64_diag_handler);
        c64_interface_enable_no_check();

        // Wait for the measurement or button press
        while (diag_state != DIAG_STOP && !diag_button_pressed());

        c64_interface(false);
        led_on();

        // Handle button press
        if (diag_button_pressed())
        {
            diag_handle_button();
        }
        else
        {
            diag_send_result(false);
        }
        led_off();
    }
}
