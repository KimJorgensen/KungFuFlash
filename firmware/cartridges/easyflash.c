/*
 * Copyright (c) 2019-2022 Kim Jørgensen
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

static u32 const ef_mode[8] =
{
    STATUS_LED_OFF|CRT_PORT_ULTIMAX,
    STATUS_LED_OFF|CRT_PORT_NONE,
    STATUS_LED_OFF|CRT_PORT_16K,
    STATUS_LED_OFF|CRT_PORT_8K,

    STATUS_LED_ON|CRT_PORT_ULTIMAX,
    STATUS_LED_ON|CRT_PORT_NONE,
    STATUS_LED_ON|CRT_PORT_16K,
    STATUS_LED_ON|CRT_PORT_8K
};

/*************************************************
* C64 bus read callback (early VIC-II cycle)
*************************************************/
FORCE_INLINE u32 ef_early_vic_handler(u32 addr)
{
    // Speculative read
    return crt_ptr[addr & 0x3fff];
}

/*************************************************
* C64 bus read callback (VIC-II cycle)
*************************************************/
FORCE_INLINE bool ef_vic_read_handler(u32 control, u32 data)
{
    if ((control & (C64_ROML|C64_ROMH)) != (C64_ROML|C64_ROMH))
    {
        C64_DATA_WRITE(data);
        return true;
    }

    return false;
}

/*************************************************
* C64 bus read callback
*************************************************/
FORCE_INLINE bool ef_read_handler(u32 control, u32 addr)
{
    if ((control & (C64_ROML|C64_ROMH)) != (C64_ROML|C64_ROMH))
    {
        C64_DATA_WRITE(crt_ptr[addr & 0x3fff]);
        return true;
    }

    if (!(control & C64_IO2))
    {
        // EasyFlash RAM at $df00-$dfff
        C64_DATA_WRITE(CRT_RAM_BUF[addr & 0xff]);
        return true;
    }

    return false;
}

/*************************************************
* C64 bus write callback
*************************************************/
FORCE_INLINE void ef_write_handler(u32 control, u32 addr, u32 data)
{
    if (!(control & C64_IO1))
    {
        switch (addr & 0xff)
        {
            // $de00 EF active flash memory bank register
            case 0x00:
            {
                crt_ptr = crt_banks[data & 0x3f];
            }
            return;

            /* $de02 EF Control register. Set to $00 on reset.
                Bit 7: Status LED, 1 = on
                Bit 6: Reserved, must be 0
                Bit 5: Reserved, must be 0
                Bit 4: Reserved, must be 0
                Bit 3: VIC-II cycles (EF3) - unused
                Bit 2: GAME mode, 1 = controlled by bit 0,
                                  0 = from jumper "boot" (EF) / buttons (EF3)
                Bit 1: EXROM state, 0 = /EXROM high
                Bit 0: GAME state if bit 2 = 1, 0 = /GAME high
            */
            case 0x02:
            {
                u32 mode = ef_mode[((data >> 5) & 0x04) | (data & 0x02) |
                                   (((data >> 2) & 0x01) & ~data)];
                C64_CRT_CONTROL(mode);
            }
            return;
        }

        return;
    }

    if (!(control & C64_IO2))
    {
        // EasyFlash RAM at $df00-$dfff
        CRT_RAM_BUF[addr & 0xff] = (u8)data;
        return;
    }
}

static void ef_init(void)
{
    C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_ULTIMAX);
}

// Needed for VIC-II and C128 read accesses at 2 MHz (e.g. for Prince of Persia)
C64_VIC_BUS_HANDLER(ef)
