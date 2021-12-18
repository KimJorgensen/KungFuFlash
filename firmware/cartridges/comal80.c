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

/*************************************************
* C64 bus write callback
*************************************************/
FORCE_INLINE void comal80_write_handler(u32 control, u32 addr, u32 data)
{
    /* The register is reset to $00 on reset. Bits:
        7   Unused
        6   GAME and EXROM line, 0 = low (only GAME line on grey cartridge)
        5   EXROM line, 0 = low (only grey cartridge)
        4-3 Unused
        2   User eprom select (only black cartridge)
        0-1 Bank select
    */

    if (!(control & C64_IO1))
    {
        crt_ptr = crt_banks[data & 0x07];

        if (data & 0x40)
        {
            C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_NONE);
        }
        else
        {
            C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_16K);
        }
    }
}

static void comal80_init(void)
{
    C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_16K);
}

C64_BUS_HANDLER_READ(comal80, crt_read_handler)
