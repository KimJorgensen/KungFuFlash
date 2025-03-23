/*
 * Copyright (c) 2019-2025 Kim Jørgensen and Sven Oliver (SvOlli) Moll
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
 * The Ross cartridge supports 16 kb, 32 kb, and 64 kb ROM
 *
 * Any read or write to $DE00-$DEFF will switch banks
 * Any read or write to $DF00-$DFFF turns ROM off
 *
 * See https://github.com/msolajic/EX-YU_64K_C64_CART for more details
 */

static u32 ross_bank;

/*************************************************
* C64 bus read callback
*************************************************/
FORCE_INLINE bool ross_read_handler(u32 control, u32 addr)
{
    if ((control & (C64_ROML|C64_ROMH)) != (C64_ROML|C64_ROMH))
    {
        C64_DATA_WRITE(crt_ptr[addr & 0x3fff]);
        return true;
    }

    if (!(control & C64_IO1))
    {
        // Any read to IO1: Switch bank
        crt_ptr = crt_banks[ross_bank & 0x03];
        ross_bank >>= 2;
        return false;
    }

    if (!(control & C64_IO2))
    {
        // Any read to IO2: Disable ROM
        C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_NONE);
        return false;
    }

    return false;
}

/*************************************************
* C64 bus write callback
*************************************************/
FORCE_INLINE void ross_write_handler(u32 control, u32 addr, u32 data)
{
    if (!(control & C64_IO1))
    {
        // Any write to IO1: Switch bank
        crt_ptr = crt_banks[ross_bank & 0x03];
        ross_bank >>= 2;
        return;
    }

    if (!(control & C64_IO2))
    {
        // Any write to IO2: Disable ROM
        C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_NONE);
        return;
    }
}

static void ross_init(DAT_CRT_HEADER *crt_header)
{
    if (crt_header->banks <= 1)
    {
        // 16 kb cartridge - no bank switching
        ross_bank = 0;
    }
    else if (crt_header->banks == 2)
    {
        // 32 kb cartridge - 2 banks
        ross_bank = (1 << 4) | (0 << 2) | (1 << 0);
    }
    else
    {
        // 64 kb cartridge - 4 banks
        ross_bank = (3 << 4) | (2 << 2) | (1 << 0);
    }
}

C64_BUS_HANDLER(ross)
