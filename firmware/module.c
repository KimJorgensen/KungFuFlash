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

#include "cartridge.c"

__attribute__((__section__(".module")))
struct
{
    union
    {
        uint8_t buf[16*1024];   // 16kB module buffer
        MODULE_HEADER;
    };
} module;

static bool crt_module_initialized;

static bool crt_init_module(void)
{
    if (!crt_module_initialized)
    {
        if (memcmp(FW_NAME, module.fw_name, FW_NAME_SIZE) == 0)
        {
            module.init();
            crt_module_initialized = true;
        }
    }

    return crt_module_initialized;
}

static bool crt_is_supported(uint32_t cartridge_type)
{
    return crt_is_supported_(cartridge_type) ||
           (crt_init_module() && module.crt_is_supported(cartridge_type));
}

static void crt_install_handler(DAT_CRT_HEADER *crt_header)
{
    if (crt_is_supported_(crt_header->type))
    {
        crt_install_handler_(crt_header);
    }
    else
    {
        module.crt_install_handler(crt_header);
    }
}
