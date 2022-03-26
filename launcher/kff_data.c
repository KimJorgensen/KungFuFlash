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

#include "kff_data.h"
#include "screen.h"

#define START_PROGRESS 0x80

static uint8_t xpos, color;
static uint16_t count;

static void progress(void)
{
    uint8_t i;

    if (--count == 0)
    {
        count = 150;

        if (xpos & START_PROGRESS)
        {
            for (i=1; i<SCREENW-1; i++)
            {
                SCREEN_RAM[i] |= 0x80;
                COLOR_RAM[i] = SEARCHC;
            }

            xpos = 0;
            color = BACKC;
            return;
        }

        if (xpos >= SCREENW-2)
        {
            xpos = 0;
            color = color == BACKC ? SEARCHC : BACKC;
        }

        COLOR_RAM[++xpos] = color;
    }
}

static void end_progress(void)
{
    uint8_t i;

    if (!(xpos & START_PROGRESS))
    {
        for (i=1; i<SCREENW-1; i++)
        {
            COLOR_RAM[i] = BACKC;
        }
    }
}

uint8_t kff_send_reply_progress(uint8_t reply)
{
    uint8_t cmd;
    KFF_COMMAND = reply;

    xpos = START_PROGRESS;
    count = 10;

    do
    {
        progress();
        cmd = KFF_COMMAND;
    }
    while (cmd == reply);

    end_progress();
    return cmd;
}
