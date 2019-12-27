/*
 * (c) 2010 Thomas Giesel
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
 * Thomas Giesel skoe@directbox.com
 */

#include <stdint.h>
#include <string.h>
#include <c64.h>

#include "usb.h"

#define USB_ID      *((uint8_t*) 0xde08)
#define USB_STATUS  *((uint8_t*) 0xde09)
#define USB_DATA    *((uint8_t*) 0xde0a)

#define USB_RX_READY 0x80
#define USB_TX_READY 0x40

static char request[12];

/*
 * 6:39 Anfang
 * 5:39 Asm
 * 5:17 Asm256
 * 5:46 mit Flow Control (5:30 ohne Statuszeile)
 */


/******************************************************************************/
/**
 * Check if a command arrived from USB. The format is:
 *
 * Len:      01234567890
 * Command: "efstart:crt"
 *
 * Return NULL if there is no command complete. Return the 0-terminated file
 * type (e.g. "crt") if it is complete.
 */
char* usbCheckForCommand(void)
{
    request[11] = '\0';

    while (USB_STATUS & USB_RX_READY)
    {
        memmove(request, request + 1, sizeof(request) - 1);
        request[sizeof(request) - 2] = USB_DATA;

        if (memcmp(request, "efstart:", 8) == 0)
        {
            return request + 8;
        }
    }
    return NULL;
}


void usbSendResponseWAIT(void)
{
    usbSendData("wait", 4);
}


void usbSendResponseSTOP(void)
{
    usbSendData("stop", 4);
    usbDiscardBuffer();
}

void usbSendResponseLOAD(void)
{
    usbSendData("load", 4);
}


void usbSendResponseBTYP(void)
{
    usbSendData("btyp", 4);
}
