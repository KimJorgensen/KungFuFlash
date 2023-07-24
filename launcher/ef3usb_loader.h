/*
 * Copyright (c) 2019-2022 Kim Jørgensen
 *
 * Derived from EasyFlash 3 Boot Image and EasyFash 3 Menu
 * Copyright (c) 2012-2013 Thomas Giesel
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

#include <stdint.h>
#include <stdbool.h>
#include "ef3usb.h"

#ifndef _EF3USB_LOADER_H_
#define _EF3USB_LOADER_H_

#define EF_RAM_TEST *((volatile uint8_t*) 0xdf10)

void ef3usb_load_and_run(void);

#endif /* _EF3USB_LOADER_H_ */
