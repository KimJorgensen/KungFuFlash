/*
 * Copyright (c) 2019 Kim Jørgensen
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
static uint32_t const ss5_mode[8] =
{
    STATUS_LED_ON|CRT_PORT_ULTIMAX,
    STATUS_LED_ON|CRT_PORT_NONE,
    STATUS_LED_ON|CRT_PORT_16K,
    STATUS_LED_ON|CRT_PORT_8K,

    STATUS_LED_OFF|CRT_PORT_ULTIMAX,
    STATUS_LED_OFF|CRT_PORT_NONE,
    STATUS_LED_OFF|CRT_PORT_16K,
    STATUS_LED_OFF|CRT_PORT_8K
};

/*************************************************
* C64 bus read callback (VIC-II cycle)
*************************************************/
static inline bool ss5_vic_read_handler(uint8_t control, uint16_t addr)
{
    // Not needed
    return false;
}

/*************************************************
* C64 bus read callback (CPU cycle)
*************************************************/
static inline bool ss5_read_handler(uint8_t control, uint16_t addr)
{
    if (crt_ptr)
    {
        if (!(control & C64_ROML))
        {
            c64_data_write(crt_ptr[addr & 0x3fff]);
            return true;
        }

        if ((control & (C64_IO1|C64_ROMH)) != (C64_IO1|C64_ROMH))
        {
            c64_data_write(crt_rom_ptr[addr & 0x3fff]);
            return true;
        }
    }

    if (control & SPECIAL_BTN)
    {
        freezer_button = FREEZE_PRESSED;
    }
    else if (freezer_button)
    {
        c64_irq_nmi(C64_IRQ_NMI_LOW);
        freezer_button = FREEZE_RELEASED;
        freezer_state = FREEZE_START;
    }

    return false;
}

/*************************************************
* C64 bus write callback (early)
*************************************************/
static inline void ss5_early_write_handler(void)
{
    // Use 3 consecutive writes to detect IRQ/NMI
    if (freezer_state && ++freezer_state == FREEZE_3_WRITES)
    {
        c64_crt_control(STATUS_LED_ON|CRT_PORT_ULTIMAX);
        freezer_state = FREEZE_RESET;

        crt_rom_ptr = crt_banks[0];
        crt_ptr = crt_ram_banks[0];
    }
}

/*************************************************
* C64 bus write callback
*************************************************/
static inline void ss5_write_handler(uint8_t control, uint16_t addr, uint8_t data)
{
    /*  The SS5 register is reset to $00 on reset.
        Bit 5-7: Unused
        Bit 4:   Bank address 14 for ROM/RAM
        Bit 3:   1 = Kill cartridge, register and ROM inactive
        Bit 2:   Bank address 13 for ROM/RAM
        Bit 1:   EXROM line, 1 = low, 0 additionally selects RAM for ROML
        Bit 0:   GAME line, 0 = low, 1 additionally exits freeze mode
    */
    if (crt_ptr)
    {
        if (!(control & C64_IO1))
        {
            uint8_t bank = ((data >> 3) & 0x02) | ((data >> 2) & 0x01);
            crt_rom_ptr = crt_banks[bank];

            uint32_t mode = ss5_mode[((data >> 1) & 0x04) | (data & 0x03)];
            c64_crt_control(mode);

            if (mode & STATUS_LED_OFF)
            {
                // Disable cartridge
                crt_ptr = NULL;
            }
            else if (mode & C64_EXROM_LOW)
            {
                crt_ptr = crt_rom_ptr;
            }
            else
            {
                crt_ptr = crt_ram_banks[bank];
            }

            if (mode & C64_GAME_HIGH)
            {
                c64_irq_nmi(C64_IRQ_NMI_HIGH);
            }

            return;
        }

        if (!(control & C64_ROML) && crt_ptr != crt_rom_ptr)
        {
            crt_ptr[addr & 0x1fff] = data;
        }
    }
}

static void ss5_init(void)
{
    c64_crt_control(STATUS_LED_ON|CRT_PORT_ULTIMAX);
    crt_rom_ptr = crt_banks[0];
    crt_ptr = crt_ram_banks[0];
}

// VIC-II read support is not needed, but it is less timing critical
C64_VIC_BUS_HANDLER_EX(ss5)
