/*
 * Copyright (c) 2019-2021 Kim Jørgensen
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

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "system_stm32f4xx.c"
#include "file_types.h"
#include "module.h"
#include "c64_interface.h"
#include "cartridge.c"

extern void module_init(void);

__attribute__((__section__(".module_header")))
MODULE_HEADER header = {
    .fw_name = FW_NAME,
    .init = module_init,
    .crt_is_supported = crt_is_supported,
    .crt_install_handler = crt_install_handler
};
