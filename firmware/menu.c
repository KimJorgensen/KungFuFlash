/*
 * Copyright (c) 2019 Kim Jørgensen
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
#include "menu.h"
#include "menu_sd.h"
#include "menu_d64.h"
#include "loader.c"
#include "menu_sd.c"
#include "menu_d64.c"

static void menu_loop()
{
    if(!c64_send_reset_to_menu())
    {
        restart_to_menu();
    }

    menu_state = sd_menu_init();

    dbg("Waiting for commands...\n");
    bool should_save_dat = true;
    bool exit_loop = false;
    while (!exit_loop)
    {
        uint8_t element_no;
        uint8_t command = c64_got_command() ?
                          c64_receive_command() : CMD_NONE;
        switch (command)
        {
            case CMD_NONE:
                if (usb_gotc())
                {
                    c64_reset(true); // Force exit menu on C64
                    dat_file.boot_type = DAT_USB;
                    should_save_dat = false;
                    exit_loop = true;
                }
                break;

            case CMD_DIR:
                menu_state->dir(menu_state);
                break;

            case CMD_DIR_ROOT:
                menu_state->dir_up(menu_state, true);
                break;

            case CMD_DIR_UP:
                menu_state->dir_up(menu_state, false);
                break;

            case CMD_DIR_PREV_PAGE:
                menu_state->prev_page(menu_state);
                break;

            case CMD_DIR_NEXT_PAGE:
                menu_state->next_page(menu_state);
                break;

            case CMD_SELECT:
                c64_receive_data(&element_no, 1);
                exit_loop = menu_state->select(menu_state, element_no);
                break;

            case CMD_BASIC:
                c64_send_exit_menu();
                dat_file.boot_type = DAT_BASIC;
                exit_loop = true;
                break;

            case CMD_KILL:
                c64_send_exit_menu();
                dat_file.boot_type = DAT_KILL;
                exit_loop = true;
                break;

            case CMD_KILL_C128:
                c64_send_exit_menu();
                dat_file.boot_type = DAT_KILL_C128;
                exit_loop = true;
                break;

            case CMD_RESET:
                c64_send_exit_menu();
                system_restart();
                break;

            default:
                wrn("Got unknown command: %02x\n", command);
                c64_send_reply(REPLY_OK);
                break;
        }
    }

    c64_interface(false);
    if (should_save_dat)
    {
        save_dat();
    }
}

static void handle_failed_to_read_sd(void)
{
    c64_send_exit_menu();
    c64_send_warning("Failed to read SD card");
    restart_to_menu();
}

static void handle_unsupported(const char *file_name)
{
    c64_send_exit_menu();

    sprint(scratch_buf, "File is not supported or invalid\r\n\r\n%s", file_name);
    c64_send_warning(scratch_buf);
    c64_send_reset_to_menu();
}

static void to_petscii_pad(char *dest, const char *src, uint8_t size)
{
    for(uint8_t i=0; i<size; i++)
    {
        char c = *src;
        if (c)
        {
            src++;
            *dest++ = ascii_to_petscii(c);
        }
        else
        {
            *dest++ = ' ';  // Pad with space
        }
    }
}

static bool format_path(char *buf, bool include_file)
{
    bool in_root = false;

    uint16_t len;
    for (len = 0; len < sizeof(dat_file.path) && dat_file.path[len]; len++)
    {
        buf[len] = dat_file.path[len];
    }

    if (include_file)
    {
        if (len > 1)
        {
            buf[len++] = '/';
        }

        char *filename = dat_file.file;
        for (; *filename; len++)
        {
            buf[len] = *filename++;
        }
    }
    buf[len] = 0;

    uint16_t index = 0;
    if (len == 0)
    {
        buf[0] = '.';
        buf[1] = 0;
    }
    else if (len > DIR_NAME_LENGTH)
    {
        index = len - DIR_NAME_LENGTH;
        buf[index] = '.';
        buf[index+1] = '.';
    }
    else if (len == 1 && buf[0] == '/')
    {
        in_root = true;
    }

    to_petscii_pad(buf, buf + index, DIR_NAME_LENGTH);
    return in_root;
}

static void send_page_end(void)
{
    scratch_buf[0] = 0;
    c64_send_data(scratch_buf, ELEMENT_LENGTH);
}
