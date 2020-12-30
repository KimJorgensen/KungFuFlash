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

#include "kff_usb.h"
#include "screen.h"

static uint8_t cmd_buf[] = "kff:\x00\x00";
static uint8_t xpos, color;
static uint16_t count;

static void startProgress(void)
{
    uint8_t i;

    xpos = 0;
    color = BACKC;
    count = 450;

    for (i=1; i<SCREENW-1; i++)
    {
        SCREEN_RAM[i] |= 0x80;
        COLOR_RAM[i] = SEARCHC;
    }
}

static void progress(void)
{
    while (!(USB_STATUS & USB_RX_READY))
    {
        if (--count == 0)
        {
            count = 150;
            if (xpos >= SCREENW-2)
            {
                xpos = 0;
                color = color == BACKC ? SEARCHC : BACKC;
            }

            COLOR_RAM[++xpos] = color;
        }
    }
}

static void kff_send_data(void *data, uint16_t size)
{
    startProgress();
    ef3usb_send_data(data, size);
}

static uint8_t kff_receive_byte(void)
{
    progress();
    return USB_DATA;
}

void kff_receive_data(void* data, uint16_t size)
{
    progress();
    ef3usb_receive_data(data, size);
}

uint8_t kff_send_command(uint8_t cmd)
{
    cmd_buf[4] = cmd;
    kff_send_data(cmd_buf, 5);

    return kff_receive_byte();
}

static void kff_send_ext_header(uint8_t cmd, uint8_t data)
{
    cmd_buf[4] = cmd;
    cmd_buf[5] = data;
    kff_send_data(cmd_buf, 6);
}

uint8_t kff_send_ext_command(uint8_t cmd, uint8_t data)
{
    kff_send_ext_header(cmd, data);
    return kff_receive_byte();
}

uint8_t kff_send_data_command(uint8_t cmd, uint8_t *data, uint8_t size)
{
    kff_send_ext_header(cmd, size);
    if (size)
    {
        ef3usb_send_data(data, size);
    }

    return kff_receive_byte();
}
