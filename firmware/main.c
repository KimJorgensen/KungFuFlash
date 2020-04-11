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
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "commands.h"
#include "print.h"
#include "hal.c"
#include "print.c"
#include "filesystem.c"
#include "file_types.c"
#include "cartridge.c"
#include "commands.c"
#include "menu.c"
#include "disk_drive.c"

int main(void)
{
    configure_system();
    log("System configured\n\n");
    c64_launcher_mode();

    bool sd_msg_shown = false;
    while (!mount_sd_card())
    {
        if (!sd_msg_shown)
        {
            c64_enable();
            c64_send_message("Please insert a FAT formatted SD card");
            sd_msg_shown = true;
        }

        delay_ms(1000);
    }

    if (!auto_boot())
    {
        c64_enable();
        menu_loop();
    }

    if (dat_file.boot_type != DAT_DISK)
    {
        filesystem_unmount();

        if (dat_file.boot_type == DAT_CRT)
        {
            // Disable all interrupts besides the C64 bus handler beyond this point
            // to ensure consistent response times
            usb_disable();
        }
    }
    else
    {
        usb_disable();
    }

    if (!c64_set_mode())
    {
        c64_disable();
        restart_to_menu();
    }

    if (dat_file.boot_type == DAT_DISK)
    {
        disk_loop();
    }

    dbg("In main loop...\n");
    while (true)
    {
        // Forward data from USB to C64
        if (usb_gotc() && ef3_can_putc())
        {
            ef3_putc(usb_getc());
        }

        // Forward data from C64 to USB
        if (ef3_gotc())
        {
            usb_putc(ef3_getc());
        }
    }
}
