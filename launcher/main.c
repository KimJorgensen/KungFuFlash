/*
 * Copyright (c) 2019-2025 Kim Jørgensen
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
#include "kff_data.h"
#include "ef3usb_loader.h"
#include "disk.h"
#include "launcher_asm.h"

/* declarations */
static void mainLoopEF3(void);
static void mainLoopKFF(void);
static void textReaderLoop(void);
static uint8_t menuLoop(void);

static void updateScreen(void);
static void help(void);
static bool isSearchChar(uint8_t c);
static uint8_t search(uint8_t c);
static void showDir(void);
static void updateDir(uint8_t last_selected);
static void printDirPage(void);
static void printElement(uint8_t pos);

static const char *kff_read_text(void);
static void showKFFMessage(uint8_t color);
static void showMessage(const char *text, uint8_t color);

/* definitions */
#define EF3_USB_CMD_LEN 12

static bool isC128 = false;

static char linebuffer[SCREENW+1];
static char inputBuffer[SEARCH_LENGTH+1];
static char searchBuffer[SEARCH_LENGTH+1];
static uint8_t searchLen;
static uint8_t *bigBuffer = NULL;
static uint16_t *pageBuffer = NULL;
static Directory *dir = NULL;

// Use different name to bypass FW update check for NTSC users (prior v1.32)
#define KUNG_FU_FLASH_VER_UPD "Kung Fu Flash " ## KFF_VER " "
#define KUNG_FU_FLASH_VER "Kung Fu Flash v" ## KFF_VER

// Place program name in the start of the output file
// so it can be read by the firmware
#pragma rodata-name (push, "LOWCODE")
const char programNameVer[FW_NAME_SIZE] = KUNG_FU_FLASH_VER_UPD;
#pragma rodata-name (pop)

static const char programBar[] = {"          " KUNG_FU_FLASH_VER "           "};
static const char menuBar[] =    {"          " KUNG_FU_FLASH_VER "  <F1> Help"};

int main(void)
{
    uint8_t i, tmp, tst;
    initScreen(COLOR_BLACK, COLOR_BLACK, TEXTC);

    dir = (Directory *)malloc(sizeof(Directory));
    bigBuffer = (uint8_t *)dir;
    pageBuffer = (uint16_t *)dir;

    if (dir == NULL)
    {
        showMessage("Failed to allocate memory.", ERRORC);
        while (true);
    }

    if (joy_install(&joy_static_stddrv) != JOY_ERR_OK)
    {
        showMessage("Failed to install joystick driver.", ERRORC);
        while (true);
    }

    isC128 = is_c128() != 0;

    if (USB_STATUS == KFF_ID_VALUE) // KFF mode
    {
        mainLoopKFF();
    }
    else    // EF3 mode
    {
        // We can also end up here if there is a hardware problem
        // Test EF3 RAM to make sure we are indeed in EF3 mode
        tst = tmp = EF_RAM_TEST;
        for (i=0; i<=8; i++)
        {
            if (EF_RAM_TEST != tst)
            {
                showMessage("Communication with cartridge failed.", ERRORC);
                cprintf("Check hardware");
                while (true);
            }
            EF_RAM_TEST = tst = 1<<i;
        }
        EF_RAM_TEST = tmp;

        mainLoopEF3();
    }

    free(dir);
    return 0;
}

static void mainLoopEF3(void)
{
    uint8_t i;
    const char *cmd;

    // Get command from Kung Fu Flash cartridge using the EF3 USB protocol
    for (i=0; i<EF3_USB_CMD_LEN; i++)
    {
        cmd = ef3usb_check_cmd();
    }

    while (true)
    {
        if (cmd == NULL)
        {
            showMessage("Communicating with USB...", WARNC);
        }
        else if (strcmp(cmd, "prg") == 0)   // Load PRG
        {
            ef3usb_send_str("load");
            ef3usb_load_and_run();
        }
        else
        {
            showMessage("Received unsupported command.", ERRORC);
            cprintf("Command: %s", cmd);
            ef3usb_send_str("etyp");
        }

        while (!(cmd = ef3usb_check_cmd()));
    }
}

static void mainLoopKFF(void)
{
    const char *text;
    uint8_t cmd, reply, cxy[3];
    uint16_t cmd_wait = 0;

    // Get command from Kung Fu Flash cartridge using the KFF protocol
    while (!(cmd = KFF_GET_COMMAND()) || cmd >= REPLY_OK)
    {
        if (++cmd_wait > 10000)
        {
            cmd_wait = 0;
            showMessage("Waiting for command from cartridge.", ERRORC);
            cprintf("Command: %x", cmd);
        }
    }

    while (true)
    {
        reply = REPLY_OK;
        switch (cmd)
        {
            case CMD_NONE:
                break;

            case CMD_MESSAGE:
                showKFFMessage(TEXTC);
                break;

            case CMD_WARNING:
                showKFFMessage(WARNC);
                waitKey();
                kff_wait_for_sync();
                break;

            case CMD_FLASH_MESSAGE:
                showKFFMessage(TEXTC);
                textcolor(ERRORC);
                cprintf("PLEASE DO NOT POWER OFF OR RESET");
                kff_wait_for_sync();
                break;

            case CMD_TEXT:
            case CMD_TEXT_WAIT:
                kff_receive_data(&cxy, sizeof(cxy));
                text = kff_read_text();
                textcolor(cxy[0]);
                cputsxy(cxy[1], cxy[2], text);
                if (cmd == CMD_TEXT_WAIT)
                {
                    kff_wait_for_sync();
                }
                break;

            case CMD_TEXT_READER:
                textReaderLoop();
                wait_for_reset();
                break;

            case CMD_MENU:
                cmd = menuLoop();
                continue;

            case CMD_WAIT_SYNC:
                clrscr();
                kff_wait_for_sync();
                break;

            case CMD_MOUNT_DISK:
                disk_mount_and_load();
                break;

            case CMD_WAIT_RESET:
                wait_for_reset();
                break;

            default:
                showMessage("Communication with cartridge failed.", ERRORC);
                cprintf("Unexpected command: %x", cmd);
                break;
        }

        cmd = kff_send_reply(reply);
    }
}

static const char *kff_read_text(void)
{
    uint16_t size;
    kff_receive_data(&size, 2);
    kff_receive_data(bigBuffer, size);
    bigBuffer[size] = 0;

    return bigBuffer;
}

static void showKFFMessage(uint8_t color)
{
    const char *text = kff_read_text();
    showMessage(text, color);
}

static void showTextPage(uint16_t page)
{
    uint8_t i, chr, last_c = 0;

    clrscr();
    revers(1);
    textcolor(BACKC);
    cputsxy(0, 0, " >");
    KFF_READ_PTR = 0;
    for (i=0; i<DIR_NAME_LENGTH; i++)
    {
        cputc(KFF_DATA);
    }
    cputs("< ");
    cputsxy(0, BOTTOM, programBar);
    revers(0);
    textcolor(TEXTC);
    gotoxy(0, 1);

    KFF_READ_PTR = pageBuffer[page];
    while (true)
    {
        chr = KFF_DATA;
        if (!chr || wherey() >= 24)
        {
            (KFF_READ_PTR)--;
            break;
        }

        if (chr == 0x0a && last_c != 0x0d)  // Handle line endings
        {
            cputs("\r\n");
        }
        else if (chr == 0x09)   // Handle tab
        {
            if ((wherex() % 2) == 0)
            {
                cputs("  ");
            }
            else
            {
                cputc(' ');
            }
        }
        else
        {
            cputc(chr);
        }

        last_c = chr;
    }

    pageBuffer[page + 1] = KFF_READ_PTR;
}

static void textReaderLoop(void)
{
    uint8_t c;
    uint16_t size, page = 0;

    KFF_READ_PTR = DIR_NAME_LENGTH;
    kff_receive_data(&size, 2);
    size++;
    pageBuffer[0] = KFF_READ_PTR;
    showTextPage(0);

    waitRelease();
    while (true)
    {
        c = kbhit() ? cgetc() : getJoy();
        switch (c)
        {
            case CH_ENTER:
            case CH_SHIFT_ENTER:
            case CH_DEL:
            case CH_STOP:
                return;

            case CH_HOME:
            case CH_FIRE_LEFT:
            case CH_FIRE_UP:
                page = 0;
                showTextPage(page);
                break;

            case CH_CURS_DOWN:
            case CH_CURS_RIGHT:
                if (KFF_READ_PTR < size && page < (sizeof(Directory)/2))
                {
                    showTextPage(++page);
                }
                break;

            case CH_CURS_LEFT:
            case CH_CURS_UP:
                if (page)
                {
                    showTextPage(--page);
                }
                break;

            default:
                break;
        }
    }
}

static void updateScreen(void)
{
    clrscr();
    revers(0);
    textcolor(BACKC);
    drawFrame();
    revers(1);
    cputsxy(0, BOTTOM, menuBar);
    revers(0);

    showDir();
}

static uint8_t menuLoop(void)
{
    uint8_t c, last_selected = 0;
    uint8_t data, cmd, reply;

    memset(dir, 0, sizeof(Directory));
    updateScreen();

    cmd = CMD_NONE;
    kff_send_size_data(searchBuffer, searchLen);
    reply = REPLY_DIR;

    while (true)
    {
        if (cmd != CMD_NONE || reply != REPLY_OK)
        {
            cmd = kff_send_reply_progress(reply);
        }

        reply = REPLY_OK;
        switch (cmd)
        {
            case CMD_NONE:
                break;

            case CMD_READ_DIR:
                readDir(dir);
                showDir();
                waitRelease();
                continue;

            case CMD_READ_DIR_PAGE:
                if (!readDirPage(dir))
                {
                    dir->selected = last_selected;
                }

                if (dir->selected >= dir->no_of_elements)
                {
                    dir->selected =
                        dir->no_of_elements ? dir->no_of_elements-1 : 0;
                }

                if (dir->selected < dir->text_elements)
                {
                    dir->selected = dir->text_elements;
                }

                showDir();
                continue;

            default:
                return cmd;
        }

        c = kbhit() ? cgetc() : getJoy();
        switch (c)
        {
            case CH_NONE:
                break;

            // --- start / enter directory
            case CH_ENTER:
            case CH_SHIFT_ENTER:
                if (dir->no_of_elements)
                {
                    data = dir->selected;
                    if (isC128)
                    {
                        data |= SELECT_FLAG_C128;
                    }
                    if (c == CH_SHIFT_ENTER)
                    {
                        data |= SELECT_FLAG_OPTIONS;
                    }

                    KFF_SEND_BYTE(data);
                    reply = REPLY_SELECT;
                }
                break;

            // --- leave directory
            case CH_DEL:
            case CH_FIRE_LEFT:
                if (searchBuffer[0] && *dir->name != ' ')
                {
                    reply = search(c);
                }
                else
                {
                    reply = REPLY_DIR_UP;
                }
                break;

            // --- root directory
            case CH_HOME:
                if (searchBuffer[0] && *dir->name != ' ')
                {
                    reply = search(c);
                }
                else
                {
                    reply = REPLY_DIR_ROOT;
                }
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
                        reply = REPLY_DIR_NEXT_PAGE;
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
                        reply = REPLY_DIR_PREV_PAGE;
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
                        reply = REPLY_DIR_NEXT_PAGE;
                    }
                }
                break;

            case CH_CURS_LEFT:
                if (dir->no_of_elements)
                {
                    last_selected = 0;
                    reply = REPLY_DIR_PREV_PAGE;
                }
                break;

            // --- search
            case CH_INS:
            case CH_FIRE_UP:
                if (*dir->name != ' ')
                {
                    reply = search(c);
                }
                break;

            // --- clear search
            case CH_CLR:
                if (searchBuffer[0] && *dir->name != ' ')
                {
                    reply = search(c);
                }
                break;

            case CH_F1:
                help();
                break;

            case CH_FIRE_DOWN:
                if (searchBuffer[0] && *dir->name != ' ')
                {
                    reply = search(c);
                }
                else
                {
                    help();
                }
                break;

            case CH_F5:
                reply = REPLY_SETTINGS;
                break;

            case CH_FIRE_RIGHT:
                if (searchBuffer[0] && *dir->name != ' ')
                {
                    reply = search(c);
                }
                else
                {
                    reply = REPLY_SETTINGS;
                }
                break;

            case CH_F6:
                reply = REPLY_KILL_C128;
                break;

            case CH_F7:
                reply = REPLY_BASIC;
                break;

            case CH_F8:
                reply = REPLY_KILL;
                break;

            case CH_STOP:
                reply = REPLY_RESET;
                break;

            default:
                if (isSearchChar(c) && *dir->name != ' ')
                {
                    reply = search(c);
                }
                break;
        }
    }
}

static bool isSearchChar(uint8_t c)
{
    return (c >= 0x20 && c <= 0x5f) || (c >= 0xc1 && c <= 0xda);
}

static uint8_t search(uint8_t c)
{
    uint8_t cursor_pos, cursor_delay;
    bool changed = false;
    uint8_t i, last_selected = dir->selected;

    dir->selected++;
    printElement(last_selected);
    dir->selected = last_selected;

    textcolor(TEXTC);
    revers(1);
    memcpy(inputBuffer, searchBuffer, searchLen);

    for (i=searchLen; i<SEARCH_LENGTH; i++)
    {
        inputBuffer[i] = ' ';
    }
    inputBuffer[SEARCH_LENGTH] = 0;

    cursor_pos = searchLen < SEARCH_LENGTH ? searchLen : SEARCH_LENGTH;

    while (c != CH_ENTER)
    {
        if (c == CH_HOME)
        {
            cursor_pos = 0;
        }
        else if (c == CH_DEL || c == CH_FIRE_LEFT)
        {
            if (cursor_pos)
            {
                cursor_pos--;
            }
            else
            {
                break;
            }

            for (i=cursor_pos; i<SEARCH_LENGTH-1; i++)
            {
                inputBuffer[i] = inputBuffer[i+1];
            }
            inputBuffer[SEARCH_LENGTH-1] = ' ';
        }
        else if (c == CH_CURS_LEFT)
        {
            if (cursor_pos)
            {
                cursor_pos--;
            }
        }
        else if (c == CH_CLR || c == CH_FIRE_DOWN)
        {
            memset(inputBuffer, ' ', SEARCH_LENGTH);
            break;
        }
        else if (cursor_pos < SEARCH_LENGTH)
        {
            if (c == CH_INS || c == CH_FIRE_UP)
            {
                for (i=SEARCH_LENGTH-1; i>cursor_pos; i--)
                {
                    inputBuffer[i] = inputBuffer[i-1];
                }
                inputBuffer[cursor_pos] = ' ';
            }
            else if (c == CH_CURS_RIGHT)
            {
                cursor_pos++;
            }
            else if (c == CH_CURS_UP)
            {
                c = inputBuffer[cursor_pos] + 1;
                if (c < 0x2a)   // '*'
                {
                    c = 'a';
                }
                else if (c < 0x30)  // '0'
                {
                    c = '0';
                }
                else if (c > 0x39 && c < 0x41)  // '9' - 'a'
                {
                    c = ' ';
                }
                else if (c > 0x5a)  // 'z'
                {
                    c = '*';
                }

                inputBuffer[cursor_pos] = c;
            }
            else if (c == CH_CURS_DOWN)
            {
                c = inputBuffer[cursor_pos] - 1;
                if (c < 0x20)   // ' '
                {
                    c = '9';
                }
                else if (c < 0x2a)  // '*'
                {
                    c = 'z';
                }
                else if (c < 0x30)   // '0'
                {
                    c = '*';
                }
                else if (c > 0x39 && c < 0x41)  // '9' - 'a'
                {
                    c = ' ';
                }
                else if (c > 0x5a)  // 'z'
                {
                    c = ' ';
                }

                inputBuffer[cursor_pos] = c;
            }
            else if (isSearchChar(c))
            {
                inputBuffer[cursor_pos++] = c;
            }
        }

        sprintf(linebuffer, "> Search: %s <", inputBuffer);
        cputsxy(1, 0, linebuffer);

        cursor_delay = 0;
        while (true)
        {
            if (cursor_delay == 0)
            {
                SCREEN_RAM[11 + cursor_pos] ^= 0x80;
                cursor_delay = 120;
            }
            cursor_delay--;

            if (kbhit())
            {
                c = cgetc();
                if (c != CH_CURS_UP && c != CH_CURS_DOWN)
                {
                    break;
                }
            }

            c = getJoy();
            if (c)
            {
                if (c == CH_FIRE_RIGHT)
                {
                    c = ' ';
                }
                break;
            }
        }
        revers(1);
    }

    for (i=SEARCH_LENGTH; i>0; i--) // remove trailing space
    {
        if (inputBuffer[i-1] == ' ')
        {
            inputBuffer[i-1] = 0;
        }
        else
        {
            break;
        }
    }
    searchLen = i;

    for (i=0; i<=searchLen; i++)
    {
        if (searchBuffer[i] != inputBuffer[i])
        {
            changed = true;
            searchBuffer[i] = inputBuffer[i];
        }
    }

    if (!changed)
    {
        showDir();
        waitRelease();
        return REPLY_OK;
    }

    kff_send_size_data(searchBuffer, searchLen);
    return REPLY_DIR;
}

static void showDir(void)
{
    if (*dir->name == CLEAR_SEARCH)
    {
        searchLen = 0;
        searchBuffer[0] = 0;
    }

    if (searchBuffer[0] && *dir->name != ' ')
    {
        sprintf(linebuffer, "> Search: %-26s <", searchBuffer);
    }
    else
    {
        sprintf(linebuffer, ">%-36s<", dir->name);
    }

    textcolor(BACKC);
    revers(1);
    cputsxy(1, 0, linebuffer);

    revers(0);
    printDirPage();
}

static void help(void)
{
    clrscr();
    revers(0);
    textcolor(TEXTC);
    cputsxy(0, 0, programBar);
    textcolor(BACKC);
    cputsxy(10, 1, "\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3\xa3");

    cputsxy(2, 2, "github.com/KimJorgensen/KungFuFlash");

    textcolor(TEXTC);
    cputsxy(0, 5, "<CRSR> or Joy       Change selection");
    cputsxy(0, 6, "<RETURN> or Fire    Run/Change Dir");
    cputsxy(0, 7, " + <SHIFT> or Hold  Options");
    cputsxy(0, 8, "<HOME>              Root Dir");
    cputsxy(0, 9, "<DEL> or Fire left  Dir Up");
    cputsxy(0, 10, "<A-Z> or Fire up    Search");
    cputsxy(0, 11, "<F1> or Fire down   Help");
    cputsxy(0, 12, "<F5> or Fire right  Settings");
    cputsxy(0, 13, "<F6>                C128 Mode");
    cputsxy(0, 14, "<F7>                BASIC (Cart Active)");
    cputsxy(0, 15, "<F8>                Kill");
    cputsxy(0, 16, "<RUN/STOP>          Reset");

    cputsxy(0, 18, "Use joystick in port 2");

    textcolor(COLOR_RED);
    cputsxy(0, 20, "KUNG FU FLASH IS PROVIDED WITH NO");
    cputsxy(0, 21, "WARRANTY OF ANY KIND.");
    textcolor(COLOR_LIGHTRED);
    cputsxy(0, 22, "USE IT AT YOUR OWN RISK!");

    gotoxy(0, 24);
    waitKey();
    updateScreen();
    waitRelease();
}

static void updateDir(uint8_t last_selected)
{
    printElement(last_selected);
    printElement(dir->selected);
}

static void printDirPage(void)
{
    uint8_t element_no = 0;

    for (;element_no < dir->no_of_elements; element_no++)
    {
        printElement(element_no);
    }

    // clear empty lines
    for (;element_no < DIRH; element_no++)
    {
        cputsxy(1, 1+element_no, "                                      ");
    }
}

static void printElement(uint8_t element_no)
{
    char *element;

    element = dir->elements[element_no];
    if (element[0] == TEXT_ELEMENT)
    {
        textcolor(WARNC);
    }
    else
    {
        textcolor(TEXTC);
    }

    if (element_no == dir->selected)
    {
        revers(1);
    }
    else
    {
        revers(0);
    }

    cputsxy(1, 1+element_no, element);
    revers(0);
}

static void showMessage(const char *text, uint8_t color)
{
    clrscr();

    revers(1);
    textcolor(BACKC);
    cputsxy(0, 0, "                                        ");
    cputsxy(0, BOTTOM, programBar);
    revers(0);

    textcolor(color);
    cputsxy(0, 3, text);

    cputs("\r\n\r\n\r\n");
}
