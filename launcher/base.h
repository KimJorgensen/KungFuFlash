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

#ifndef BASE_H
#define BASE_H

#define JOY_MASK (JOY_UP_MASK|JOY_DOWN_MASK|JOY_LEFT_MASK|JOY_RIGHT_MASK|JOY_BTN_1_MASK)

#define CH_NONE 0
#define CH_SHIFT_ENTER  (0x80|CH_ENTER)
#define CH_CLR          (0x80|CH_HOME)

#define CH_FIRE_UP      (0xa6)  // CBM and +
#define CH_FIRE_DOWN    (0xdc)  // CBM and -
#define CH_FIRE_LEFT    (0xdb)  // SHIFT and +
#define CH_FIRE_RIGHT   (0xdd)  // SHIFT and -

uint8_t getJoy(void);
void waitKey(void);
void waitRelease(void);

#endif
