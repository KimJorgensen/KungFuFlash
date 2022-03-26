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

#define COLOR_VIOLET        (4)
#define COLOR_LIGHTGREEN    (13)

static const char diag_header[] = "Diagnostic\r\n";
static const char diag_underline[] = "\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3";

static void usb_send_text(const char *text)
{
    for (const char *p = text; *p && usb_can_putc(); )
    {
        usb_putc(*p++);
    }
}

static void diag_loop(void)
{
    bool usb_only = special_button_pressed();

    usb_send_text(diag_header);
    if (!usb_only)
    {
        c64_enable();
        c64_send_message(diag_header);
        c64_send_text_wait(COLOR_VIOLET, 0, 4, diag_underline);
    }
    else
    {
        C64_CRT_CONTROL(CRT_PORT_NONE);
        c64_reset(false);
    }

    u16 cnt = 0;
    while (true)
    {
        c64_interface(false);
        led_off();

        // Install IRQ handler to measure the Phi2 frequency
        diag_state = DIAG_INIT;
        C64_INSTALL_HANDLER(c64_diag_handler);
        c64_interface_enable_no_check();

        // Wait for the measurement
        while (diag_state != DIAG_STOP);
        c64_interface(false);
        led_on();
        delay_ms(10);

        // Send result to USB
        sprint(scratch_buf, "Phi2 frequency: %u Hz (%u)             \r\n",
               diag_phi2_freq, cnt++);
        usb_send_text(scratch_buf);

        if (!usb_only)
        {
            // Restore handler to display result on C64
            C64_INSTALL_HANDLER(kff_handler);
            c64_interface_sync();

            c64_send_text_wait(COLOR_LIGHTGREEN, 0, 7, scratch_buf);
        }
    }
}
