/*
 * Copyright (c) 2019-2024 Kim Jørgensen and Sven Oliver (SvOlli) Moll
 * Copyright (c) 2024 Vladan Nikolic
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
 * Ross has an easy hardware setup
 * - a single 16k bank that can be turned on or off
 * - read $DE00-$DFFF will switch banks if 32k image 
 * - read $DF00-$DFFF turns ROM off
 */

static u32 ross_on;

/*************************************************
* C64 bus read callback
*************************************************/
FORCE_INLINE bool ross_read_handler(u32 control, u32 addr)
{
/*    if (!(control & C64_IO1))
    {
        crt_ptr = crt_banks[addr & 0x3f];
        return false;
    }
  */  
    /* IO and ROM access */
    if (!(control & C64_IO2))
    {
        // Any read to IO2: Disable ROM
        C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_NONE);
        return true;
        
    }

    return false;
}

/*************************************************
* C64 bus write callback
*************************************************/
FORCE_INLINE void ross_write_handler(u32 control, u32 addr, u32 data)
{

    // No support for write
    
}

static void ross_init(DAT_CRT_HEADER *crt_header)
{
    ross_on = STATUS_LED_ON|CRT_PORT_16K;
//    crt_rom_ptr = crt_banks[0];
    
    C64_CRT_CONTROL(ross_on);
}

C64_BUS_HANDLER(ross)
