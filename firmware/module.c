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

__attribute__((__section__(".module")))
struct
{
    union
    {
        u8 buf[16*1024];    // 16kB module buffer
        MODULE_HEADER;
    };
} module;

static void module_load(void)
{
    FIL file;
    if (file_open(&file, UPD_FILENAME, FA_READ))
    {
        if (!file_seek(&file, sizeof(dat_buffer)) ||
            file_read(&file, module.buf, sizeof(module.buf)) != sizeof(module.buf))
        {
            memset(module.buf, 0xff, sizeof(module.buf));
        }

        file_close(&file);
    }
}

static bool module_initialized;

static bool module_init(void)
{
    if (!module_initialized)
    {
        if (memcmp(FW_NAME, module.fw_name, FW_NAME_SIZE) == 0)
        {
            module.init();
            module_initialized = true;
        }
    }

    return module_initialized;
}
