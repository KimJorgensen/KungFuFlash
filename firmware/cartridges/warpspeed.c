/*
 * Copyright (c) 2019-2022 Kim Jørgensen and Sven Oliver (SvOlli) Moll
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

/*
 * Warp Speed has an easy hardware setup
 * - a single 16k bank that can be turned on or off
 * - read $DE00-$DFFF is a mirror of $9E00-$9FFF
 * - write $DE00-$DEFF turns ROM on
 * - write $DF00-$DFFF turns ROM off
 * still it's twice as fast as JiffyDOS and can load to RAM at $D000-$DFFF,
 * and also packs a lot of features.
 */

static u32 warpspeed_on;

/*************************************************
* C64 bus read callback
*************************************************/
FORCE_INLINE bool warpspeed_read_handler(u32 control, u32 addr)
{
    /* IO and ROM access */
    if ((control & (C64_IO1|C64_IO2|C64_ROML|C64_ROMH)) != (C64_IO1|C64_IO2|C64_ROML|C64_ROMH))
    {
        /* $DE00-$DFFF are a mirror of $9E00-$9FFF */
        C64_DATA_WRITE(crt_ptr[addr & 0x3fff]);
        return true;
    }

    return false;
}

/*************************************************
* C64 bus write callback
*************************************************/
FORCE_INLINE void warpspeed_write_handler(u32 control, u32 addr, u32 data)
{
    if (!(control & C64_IO1))
    {
        C64_CRT_CONTROL(warpspeed_on);
    }
    else if (!(control & C64_IO2))
    {
        C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_NONE);
    }
}

static void warpspeed_init(DAT_CRT_HEADER *crt_header)
{
    if (crt_header->type == CRT_WARP_SPEED)
    {
        warpspeed_on = STATUS_LED_ON|CRT_PORT_16K;
    }
    else // CRT_C128_WARP_SPEED
    {
        warpspeed_on = STATUS_LED_ON|CRT_PORT_NONE;
    }

    C64_CRT_CONTROL(warpspeed_on);
}

// Support C128 2MHz read access
C64_C128_BUS_HANDLER(warpspeed)
