/*
 * Copyright (c) 2019-2023 Kim Jørgensen
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
#include "launcher_asm.h"

#ifndef KFF_DATA_H
#define KFF_DATA_H

#define KFF_DATA        *((volatile uint8_t*) 0xde00)
#define KFF_COMMAND     *((volatile uint8_t*) 0xde01)
#define KFF_READ_PTR    *((volatile uint16_t*) 0xde04)

#define KFF_SEND_BYTE(data) KFF_DATA = (data)
#define KFF_GET_COMMAND()   (KFF_COMMAND)

uint8_t kff_send_reply_progress(uint8_t reply);

#endif