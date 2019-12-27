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
#include "kff_usb.h"

static uint8_t cmd_buf[] = "kff:\x00\x00";

uint8_t ef3usb_receive_byte(void)
{
    while (!(USB_STATUS & USB_RX_READY));
    return USB_DATA;
}

void ef3usb_send_byte(uint8_t data)
{
    while (!(USB_STATUS & USB_TX_READY));
    USB_DATA = data;
}

uint8_t kff_send_command(uint8_t cmd)
{
    cmd_buf[4] = cmd;
    ef3usb_send_data(cmd_buf, 5);

    return ef3usb_receive_byte();
}

uint8_t kff_send_ext_command(uint8_t cmd, uint8_t data)
{
    cmd_buf[4] = cmd;
    cmd_buf[5] = data;
    ef3usb_send_data(cmd_buf, 6);

    return ef3usb_receive_byte();
}
