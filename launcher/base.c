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

#include <stdint.h>
#include <stdio.h>
#include <conio.h>
#include <joystick.h>
#include "base.h"

#define LONG_PRESS  (180)
#define KEY_PRESS   (80)
#define KEY_REPEAT  (20)
#define MIN_PRESS   (4)
#define DEBOUNCE    (30)

static uint8_t debounceJoy(void)
{
    uint8_t i, j, pressed = 0;

    for (i=0; i<DEBOUNCE; i++)
    {
        j = joy_read(JOY_2) & JOY_MASK;
        pressed |= j;
    }

    return pressed;
}

uint8_t getJoy(void)
{
    static uint8_t last;
    uint8_t j, btn_last = 0;
    uint16_t cnt, cnt_max = KEY_PRESS;

    if (last)
    {
        if (JOY_BTN_1(last))
        {
            btn_last = last;
        }
        else
        {
            cnt_max = KEY_REPEAT;
        }
    }

    for (cnt=0; cnt<cnt_max; cnt++)
    {
        last = debounceJoy();
        if (!last)
        {
            break;
        }

        if (JOY_BTN_1(last))
        {
            if (last == JOY_BTN_1_MASK) // only button
            {
                if (btn_last)
                {
                    break;
                }

                cnt_max = LONG_PRESS;
            }
            else    // button and direction
            {
                if (btn_last == last)
                {
                    break;
                }

                btn_last = JOY_BTN_1_MASK;
                cnt_max = KEY_PRESS;
            }
        }

        j = last;
    }

    if (cnt < MIN_PRESS)
    {
        return CH_NONE;
    }

    if (JOY_BTN_1(j))
    {
        if (j != JOY_BTN_1_MASK)
        {
            return JOY_UP(j) ? CH_FIRE_UP :
                    JOY_DOWN(j) ? CH_FIRE_DOWN :
                     JOY_LEFT(j) ? CH_FIRE_LEFT : CH_FIRE_RIGHT;
        }

        if (cnt < LONG_PRESS)
        {
            return CH_ENTER;    // short press
        }

        return CH_SHIFT_ENTER;  // long press
    }

    return JOY_UP(j) ? CH_CURS_UP :
            JOY_DOWN(j) ? CH_CURS_DOWN :
             JOY_LEFT(j) ? CH_CURS_LEFT : CH_CURS_RIGHT;
}

void waitKey(void)
{
	revers(1);
	textcolor(COLOR_PURPLE);
	cputs("PRESS A KEY");
	revers(0);

    while (1)
    {
        if (kbhit())
        {
            cgetc();
            break;
        }

        if (getJoy())
        {
            break;
        }
    }
}

void waitRelease(void)
{
    while (1)
    {
        if (kbhit())
        {
            cgetc();
        }
        else if (!getJoy())
        {
            break;
        }
    }
}
