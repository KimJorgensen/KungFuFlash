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
 *
 ******************************************************************************
 * Derived from Draco Browser V1.0C 8 Bit (27.12.2009)
 * Created 2009 by Sascha Bader
 *
 * The code can be used freely as long as you retain
 * a notice describing original source and author.
 *
 * THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL,
 * BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
 *
 * Newer versions might be available here: http://www.sascha-bader.de/html/code.html
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <conio.h>
#include <cbm.h>
#include <errno.h>
#include <joystick.h>
#include "screen.h"
#include "dir.h"
#include "base.h"
#include "kff_usb.h"
#include "disk.h"
#include "launcher_asm.h"

/* declarations */
static void mainLoop(void);
static void browserLoop(void);
static void updateScreen(void);

static void help(void);
static bool handleCommand(uint8_t cmd, bool options, uint8_t last_selected);
static void showDir(void);
static void updateDir(uint8_t last_selected);
static void printDirPage(void);
static void printElement(uint8_t pos);

static const char *usb_read_text(void);
static void showMessage(const char *text, uint8_t color);

/* definitions */
#define EF3_USB_CMD_LEN 12
#define LONG_PRESS 2000
#define CH_SHIFT_ENTER (0x80|CH_ENTER)

static bool isC128 = false;

static char linebuffer[SCREENW+1];
static uint8_t *bigBuffer = NULL;
static Directory *dir = NULL;

static const uint8_t textc = COLOR_LIGHTGREEN;
static const uint8_t backc = COLOR_VIOLET;
static const uint8_t warnc = COLOR_YELLOW;
static const uint8_t errorc = COLOR_LIGHTRED;

#define KUNG_FU_FLASH_VER "Kung Fu Flash v" ## VERSION

// Place program name in the start of the output file
// so it can be read by the firmware
#pragma rodata-name (push, "LOWCODE")
const char programNameVer[] = KUNG_FU_FLASH_VER;

// Area used by firmware for placing BASIC commands to run at start-up
const char padding[BASIC_CMD_BUF_SIZE] = {};
#pragma rodata-name (pop)

static const char programBar[] = {"          " KUNG_FU_FLASH_VER "           "};
static const char menuBar[] =    {"          " KUNG_FU_FLASH_VER "  <F1> Help"};

int main(void)
{
    initScreen(COLOR_BLACK, COLOR_BLACK, textc);

    dir = (Directory *)malloc(sizeof(Directory));
    bigBuffer = (uint8_t *)dir;

    if (dir == NULL)
    {
        showMessage("Failed to allocate memory.", errorc);
        while (true);
    }

    if (joy_install(&joy_static_stddrv) != JOY_ERR_OK)
    {
        showMessage("Failed to install joystick driver.", errorc);
        while (true);
    }

    isC128 = is_c128() != 0;

    mainLoop();
    free(dir);
    return 0;
}

static void mainLoop(void)
{
    const char *cmd, *text;
    bool send_fclose;
    uint8_t i;

    // Get command from Kung Fu Flash cartridge using the EF3 USB protocol
    for (i=0; i<EF3_USB_CMD_LEN; i++)
    {
        cmd = ef3usb_check_cmd();
    }

    while (true)
    {
        send_fclose = false;

        if (cmd == NULL)
        {
            showMessage("Communication with cartridge failed.", errorc);
        }
        else if (cmd[0] == 'm' && cmd[1] == 's')    // Show message
        {
            uint8_t color = textc;                  // msg: Normal message
            if (cmd[2] == 'w')                      // msw: Warning message
            {
                color = warnc;
            }
            else if (cmd[2] == 'e')                 // mse: Error message
            {
                color = errorc;
            }

            text = usb_read_text();
            showMessage(text, color);

            if (cmd[2] == 'w')
            {
                waitKey();
            }
            else if (cmd[2] == 'f')                 // msf: Flash programming message
            {
                textcolor(errorc);
                cprintf("PLEASE DO NOT POWER OFF OR RESET");
            }

            send_fclose = true;
        }
        else if (strcmp(cmd, "rst") == 0)           // Reset to menu
        {
            ef3usb_send_str("done");
            browserLoop();
            send_fclose = true;
        }
        else if (strcmp(cmd, "wfr") == 0)           // Disable screen and wait for reset
        {
            ef3usb_send_str("done");
            wait_for_reset();
        }
        else if (strcmp(cmd, "mnt") == 0)           // Mount disk
        {
            ef3usb_send_str("done");
            disk_mount_and_load();
        }
        else if (strcmp(cmd, "prg") == 0)           // Load PRG
        {
            ef3usb_send_str("load");
            usbtool_prg_load_and_run();
        }
        else
        {
            showMessage("Received unsupported command.", errorc);
            cprintf("Command: %s", cmd);
            ef3usb_send_str("etyp");
        }

        cmd = ef3usb_get_cmd(send_fclose);
    }
}

static const char *usb_read_text(void)
{
    uint16_t size;

    ef3usb_send_str("read");
    ef3usb_receive_data(&size, 2);
    ef3usb_receive_data(bigBuffer, size);
    bigBuffer[size] = 0;

    return bigBuffer;
}

static void updateScreen(void)
{
    clrscr();
    revers(0);
    textcolor(backc);
    drawFrame();
    gotoxy(0, BOTTOM);
    revers(1);
    cputs(menuBar);
    revers(0);

    showDir();
}

static bool handleCommand(uint8_t cmd, bool options, uint8_t last_selected)
{
    bool quit_browser = false;
    uint8_t data, reply;

    if (cmd != CMD_SELECT)
    {
        reply = kff_send_command(cmd);
    }
    else
    {
        data = dir->selected;
        if (isC128)
        {
            data |= SELECT_FLAG_C128;
        }
        if (options)
        {
            data |= SELECT_FLAG_OPTIONS;
        }

        reply = kff_send_ext_command(cmd, data);
    }

    switch (reply)
    {
        case REPLY_OK:
            break;

        case REPLY_READ_DIR:
            readDir(dir);
            showDir();
            break;

        case REPLY_READ_DIR_PAGE:
            if (!readDirPage(dir))
            {
                dir->selected = last_selected;
            }

            if (dir->selected >= dir->no_of_elements)
            {
                dir->selected = dir->no_of_elements ? dir->no_of_elements-1 : 0;
            }

            if (dir->selected < dir->text_elements)
            {
                dir->selected = dir->text_elements;
            }

            printDirPage();
            break;

        case REPLY_EXIT_MENU:
            quit_browser = true;
            break;

        default:
            break;
    }

    return quit_browser;
}

static void browserLoop(void)
{
    char *element = NULL;
    uint8_t c, j, last_selected = 0;
    uint16_t joy_cnt = 0, fire_cnt = 0;
    bool options = false, joy_down = false;
    uint8_t kff_cmd = CMD_DIR;

    memset(dir, 0, sizeof(Directory));
    updateScreen();

    while (true)
    {
        c = kbhit() ? cgetc() : 0;
        if (!c)
        {
            j = joy_read(1);
            c = JOY_UP(j) ? CH_CURS_UP :
                JOY_DOWN(j) ? CH_CURS_DOWN :
                JOY_LEFT(j) ? CH_CURS_LEFT :
                JOY_RIGHT(j) ? CH_CURS_RIGHT :
                JOY_BTN_1(j) ? CH_ENTER : 0;

            if (c)
            {
                if (joy_cnt)
                {
                    c = 0;
                }
                else if (c == CH_ENTER)
                {
                    if (fire_cnt < LONG_PRESS)
                    {
                        // wait for release or long press
                        fire_cnt++;
                        c = 0;
                    }
                    else if (fire_cnt == LONG_PRESS)
                    {
                        // long press
                        fire_cnt++;
                        joy_cnt = 30;
                        c = CH_SHIFT_ENTER;
                    }
                    else
                    {
                        // wait for release
                        c = 0;
                    }
                }
                else
                {
                    fire_cnt = 0;
                    // TODO: Use timer for this?
                    joy_cnt = joy_down ? 120 : 1200;
                    joy_down = true;
                }
            }
            else
            {
                if (fire_cnt && fire_cnt < LONG_PRESS)
                {
                    // short press
                    c = CH_ENTER;
                }

                fire_cnt = 0;
                joy_down = false;
                joy_cnt = 30;
            }
        }

        if (joy_cnt)
        {
            joy_cnt--;
        }

        switch (c)
        {
            // --- start / enter directory
            case CH_ENTER:
            case CH_SHIFT_ENTER:
                if (dir->no_of_elements)
                {
                    options = c == CH_SHIFT_ENTER;
                    kff_cmd = CMD_SELECT;
                }
                break;

            // --- leave directory
            case CH_DEL:
                kff_cmd = CMD_DIR_UP;
                break;

            // --- root directory
            case CH_HOME:
                kff_cmd = CMD_DIR_ROOT;
                break;

            case CH_CURS_DOWN:
                if (dir->no_of_elements)
                {
                    last_selected = dir->selected;
                    if ((dir->selected+1) < dir->no_of_elements)
                    {
                        dir->selected++;
                        updateDir(last_selected);
                    }
                    else if (dir->no_of_elements == MAX_ELEMENTS_PAGE)
                    {
                        dir->selected = 0;
                        kff_cmd = CMD_DIR_NEXT_PAGE;
                    }
                }
                break;

            case CH_CURS_UP:
                if (dir->no_of_elements)
                {
                    last_selected = dir->selected;
                    if (dir->selected > dir->text_elements)
                    {
                        dir->selected--;
                        updateDir(last_selected);
                    }
                    else
                    {
                        dir->selected = MAX_ELEMENTS_PAGE-1;
                        kff_cmd = CMD_DIR_PREV_PAGE;
                    }
                }
                break;

            case CH_CURS_RIGHT:
                if (dir->no_of_elements)
                {
                    if (dir->no_of_elements < MAX_ELEMENTS_PAGE)
                    {
                        last_selected = dir->selected;
                        dir->selected = dir->no_of_elements-1;
                        updateDir(last_selected);
                    }
                    else
                    {
                        last_selected = dir->no_of_elements-1;
                        kff_cmd = CMD_DIR_NEXT_PAGE;
                    }
                }
                break;

            case CH_CURS_LEFT:
                if (dir->no_of_elements)
                {
                    last_selected = 0;
                    kff_cmd = CMD_DIR_PREV_PAGE;
                }
                break;

            case CH_F1:
                help();
                break;

            case CH_F5:
                kff_cmd = CMD_SETTINGS;
                break;

            case CH_F6:
                kff_cmd = CMD_KILL_C128;
                break;

            case CH_F7:
                kff_cmd = CMD_BASIC;
                break;

            case CH_F8:
                kff_cmd = CMD_KILL;
                break;

            case CH_STOP:
                kff_cmd = CMD_RESET;
                break;
        }

        if (kff_cmd != CMD_NONE)
        {
            if(handleCommand(kff_cmd, options, last_selected))
            {
                break;
            }

            kff_cmd = CMD_NONE;
        }
    }
}

static void showDir(void)
{
    char *title;
    if (dir->name[0] != NULL)
    {
        sprintf(linebuffer, "> %-34s <", dir->name);
        title = linebuffer;
    }
    else
    {
        title = "                                      ";
    }

    gotoxy(1, 0);
    textcolor(backc);
    revers(1);
    cputs(title);
    revers(0);

    printDirPage();
}

static void help(void)
{
    clrscr();
    revers(0);
    textcolor(textc);
    gotoxy(0, 0);
    cputs(programBar);
    textcolor(backc);
    gotoxy(10, 1);
    cputs("\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3");

    gotoxy(2, 2);
    cputs("github.com/KimJorgensen/KungFuFlash");

    textcolor(textc);
    gotoxy(0, 5);
    cputs("<CRSR> or Joy       Change selection");
    gotoxy(0, 6);
    cputs("<RETURN> or Fire    Run/Change Dir");
    gotoxy(0, 7);
    cputs(" + <SHIFT> or Hold  Options");
    gotoxy(0, 8);
    cputs("<HOME>              Root Dir");
    gotoxy(0, 9);
    cputs("<DEL>               Dir Up");
    gotoxy(0, 10);
    cputs("<F1>                Help");
    gotoxy(0, 11);
    cputs("<F5>                Settings");
    gotoxy(0, 12);
    cputs("<F6>                C128 Mode");
    gotoxy(0, 13);
    cputs("<F7>                BASIC (Cart Active)");
    gotoxy(0, 14);
    cputs("<F8>                Kill");
    gotoxy(0, 15);
    cputs("<RUN/STOP>          Reset");

    gotoxy(0, 17);
    cputs("Use joystick in port 2");

    textcolor(COLOR_RED);
    gotoxy(0, 19);
    cputs("KUNG FU FLASH IS PROVIDED WITH NO");
    gotoxy(0, 20);
    cputs("WARRANTY OF ANY KIND.");
    gotoxy(0, 21);
    textcolor(COLOR_LIGHTRED);
    cputs("USE IT AT YOUR OWN RISK!");
    gotoxy(0, 24);
    waitKey();
    updateScreen();
}

static void updateDir(uint8_t last_selected)
{
    printElement(last_selected);
    printElement(dir->selected);
}

static void printDirPage()
{
    uint8_t element_no = 0;

    for (;element_no < dir->no_of_elements; element_no++)
    {
        printElement(element_no);
    }

    // clear empty lines
    for (;element_no < DIRH; element_no++)
    {
        gotoxy(1, 1+element_no);
        cputs("                                      ");
    }
}

static void printElement(uint8_t element_no)
{
    char *element;

    element = dir->elements[element_no];
    if (element[0] == TEXT_ELEMENT)
    {
        textcolor(warnc);
    }
    else
    {
        textcolor(textc);
    }

    if (element_no == dir->selected)
    {
        revers(1);
    }
    else
    {
        revers(0);
    }

    gotoxy(1, 1+element_no);
    cputs(element);
    revers(0);
}

static void showMessage(const char *text, uint8_t color)
{
    clrscr();

    revers(1);
    textcolor(backc);
    gotoxy(0, 0);
    cputs("                                        ");
    gotoxy(0, BOTTOM);
    cputs(programBar);
    revers(0);

    textcolor(color);
    gotoxy(0, 3);
    cputs(text);

    cputs("\r\n\r\n\r\n");
}
