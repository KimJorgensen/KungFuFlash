/*
 * Copyright (c) 2022 Kim Jørgensen
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

static u8 * const pagefox_mode[8] =
{
    CRT_DAT_BANK(0),
    CRT_DAT_BANK(1),
    CRT_DAT_BANK(2),
    CRT_DAT_BANK(3),
    CRT_RAM_BANK(0),
    CRT_RAM_BANK(2),
    NULL,
    NULL
};

/*************************************************
* C64 bus read callback
*************************************************/
FORCE_INLINE bool pagefox_read_handler(u32 control, u32 addr)
{
    if ((control & (C64_ROML|C64_ROMH)) != (C64_ROML|C64_ROMH) && crt_ptr)
    {
        C64_DATA_WRITE(crt_ptr[addr & 0x3fff]);
        return true;
    }

    return false;
}

/*************************************************
* C64 bus write callback
*************************************************/
FORCE_INLINE void pagefox_write_handler(u32 control, u32 addr, u32 data)
{
    if (!(control & C64_IO1))
    {
        // Register at $de80-$deff
        if (addr & 0x80)
        {
            u32 mode = (data >> 1) & 0x07;
            crt_ptr = pagefox_mode[mode];

            if (!(data & 0x10))
            {
                C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_16K);
            }
            else
            {
                C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_NONE);
            }
        }

        return;
    }

    // Ram at $8000-$bfff
    if ((addr >> 14) == 0x02 &&
        (((u32)crt_ptr >> 15) == ((u32)CRT_RAM_BANK(0) >> 15)))
    {
        crt_ptr[addr & 0x3fff] = (u8)data;
        return;
    }
}

static void pagefox_init(void)
{
    C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_16K);
}

C64_BUS_HANDLER(pagefox)
