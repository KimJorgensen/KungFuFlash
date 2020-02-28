/*
 * Copyright (c) 2019-2020 Kim Jørgensen
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
#ifndef _LAUNCHER_ASM_H_
#define _LAUNCHER_ASM_H_

void usbtool_prg_load_and_run(void);
char* __fastcall__ ef3usb_get_cmd(bool send_fclose);
void wait_for_reset(void);
uint8_t is_c128(void);

#endif /* _LAUNCHER_ASM_H_ */
