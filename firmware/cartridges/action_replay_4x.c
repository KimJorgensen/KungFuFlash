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

static u32 const ar4x_mode[4] =
{
    STATUS_LED_ON|CRT_PORT_8K,
    STATUS_LED_ON|CRT_PORT_16K,
    STATUS_LED_ON|CRT_PORT_NONE,
    STATUS_LED_ON|CRT_PORT_ULTIMAX
};

/*************************************************
* C64 bus read callback (VIC-II cycle)
*************************************************/
FORCE_INLINE bool ar4x_vic_read_handler(u32 control, u32 addr)
{
    // Not needed
    return false;
}

/*************************************************
* C64 bus read callback (CPU cycle)
*************************************************/
FORCE_INLINE bool ar4x_read_handler(u32 control, u32 addr)
{
    // Not 100% accurate: ROM should not be accessible at IO2
    // in 16k & Ultimax mode nor at ROML in 16k mode
    if ((control & (C64_IO2|C64_ROML)) != (C64_IO2|C64_ROML))
    {
        if (crt_ptr)
        {
            C64_DATA_WRITE(crt_ptr[addr & 0x1fff]);
            return true;
        }

        return false;
    }

    if (!(control & C64_ROMH))
    {
        if (crt_rom_ptr)
        {
            C64_DATA_WRITE(crt_rom_ptr[addr & 0x1fff]);
            return true;
        }

        return false;
    }

    if (control & SPECIAL_BTN)
    {
        special_button = SPECIAL_PRESSED;
    }
    else if (special_button)
    {
        C64_IRQ_NMI(C64_IRQ_NMI_LOW);
        special_button = SPECIAL_RELEASED;
        freezer_state = FREEZE_START;
    }

    return false;
}

/*************************************************
* C64 bus write callback (early)
*************************************************/
FORCE_INLINE void ar4x_early_write_handler(void)
{
    // Use 3 consecutive writes to detect IRQ/NMI
    if (freezer_state && ++freezer_state == FREEZE_3_WRITES)
    {
        C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_ULTIMAX);
        freezer_state = FREEZE_RESET;

        crt_rom_ptr = crt_banks[0];
        crt_ptr = crt_rom_ptr;
    }
}

/*************************************************
* C64 bus write callback
*************************************************/
FORCE_INLINE void ar4x_write_handler(u32 control, u32 addr, u32 data)
{
    if (crt_ptr)
    {
        /* The register is reset to $00 on reset.
            Bit 7: Bank address 15 for ROM (unused)
            Bit 6: Write 1 to exit freeze mode
            Bit 5: Switches between ROM and RAM: 0 = ROM, 1 = RAM
            Bit 4: Bank address 14 for ROM/RAM
            Bit 3: Bank address 13 for ROM/RAM
            Bit 2: 1 = Kill cartridge, registers and memory inactive
            Bit 1: EXROM line, 0 = low
            Bit 0: GAME line, 1 = low
        */
        if (!(control & C64_IO1))
        {
            if (data & 0x40)
            {
                C64_IRQ_NMI(C64_IRQ_NMI_HIGH);
            }

            if (!(data & 0x04))
            {
                C64_CRT_CONTROL(ar4x_mode[data & 0x03]);

                u8 bank = (data >> 3) & 0x03;
                crt_rom_ptr = crt_banks[bank];

                if (data & 0x20)
                {
                    crt_ptr = crt_ram_banks[bank];
                }
                else
                {
                    crt_ptr = crt_rom_ptr;
                }
            }
            else
            {
                // Disable cartridge
                C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_NONE);
                crt_ptr = NULL;
                crt_rom_ptr = NULL;
            }

            return;
        }

        if ((control & (C64_IO2|C64_ROML)) != (C64_IO2|C64_ROML) && crt_ptr != crt_rom_ptr)
        {
            crt_ptr[addr & 0x1fff] = (u8)data;
            return;
        }
    }
}

static void ar4x_init(void)
{
    C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_8K);
    crt_rom_ptr = crt_ptr;
}

// VIC-II read support is not needed, but it is less timing critical
C64_VIC_BUS_HANDLER_EX(ar4x)
