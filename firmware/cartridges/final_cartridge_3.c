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

static u32 const fc3_mode[8] =
{
    STATUS_LED_ON|CRT_PORT_16K,
    STATUS_LED_ON|CRT_PORT_ULTIMAX,
    STATUS_LED_ON|CRT_PORT_8K,
    STATUS_LED_ON|CRT_PORT_NONE,

    STATUS_LED_OFF|CRT_PORT_16K,
    STATUS_LED_OFF|CRT_PORT_ULTIMAX,
    STATUS_LED_OFF|CRT_PORT_8K,
    STATUS_LED_OFF|CRT_PORT_NONE
};

#define FC3_REGISTER_ADDR (0xdfff)
#define FC3_REGISTER_HIDE (0)
static u32 fc3_register;

/*************************************************
* C64 bus read callback (VIC-II cycle)
*************************************************/
FORCE_INLINE bool fc3_vic_read_handler(u32 control, u32 addr)
{
    if ((control & (C64_IO1|C64_IO2|C64_ROML|C64_ROMH)) != (C64_IO1|C64_IO2|C64_ROML|C64_ROMH))
    {
        C64_DATA_WRITE(crt_ptr[addr & 0x3fff]);
        return true;
    }

    return false;
}

/*************************************************
* C64 bus read callback (CPU cycle)
*************************************************/
FORCE_INLINE bool fc3_read_handler(u32 control, u32 addr)
{
    if ((control & (C64_IO1|C64_IO2|C64_ROML|C64_ROMH)) != (C64_IO1|C64_IO2|C64_ROML|C64_ROMH))
    {
        C64_DATA_WRITE(crt_ptr[addr & 0x3fff]);
        return true;
    }

    if (control & SPECIAL_BTN)
    {
        C64_IRQ_NMI(C64_NMI_LOW);
        freezer_state = FREEZE_START;
    }
    else if (freezer_state)
    {
        freezer_state = FREEZE_START;
    }

    return false;
}

/*************************************************
* C64 bus write callback (early)
*************************************************/
FORCE_INLINE void fc3_early_write_handler(void)
{
    // Use 3 consecutive writes to detect NMI
    if (freezer_state && ++freezer_state == FREEZE_3_WRITES)
    {
        C64_CRT_CONTROL(STATUS_LED_ON|C64_GAME_LOW);
        freezer_state = FREEZE_RESET;
        fc3_register = FC3_REGISTER_ADDR;
    }
}

/*************************************************
* C64 bus write callback
*************************************************/
FORCE_INLINE void fc3_write_handler(u32 control, u32 addr, u32 data)
{
    /*  The FC3 register is reset to $00 on reset.
        Bit 7: Hide register, 1 = disable write to register
        Bit 6: NMI line, 0 = low
        Bit 5: GAME line, 0 = low
        Bit 4: EXROM line, 0 = low
        Bit 3: Bank address 17 for ROM (only FC3+)
        Bit 2: Bank address 16 for ROM (only FC3+)
        Bit 1: Bank address 15 for ROM
        Bit 0: Bank address 14 for ROM
    */
    if (!(control & C64_IO2) && addr == fc3_register)
    {
        crt_ptr = crt_banks[data & 0x0f];

        if (data & 0x40)
        {
            C64_IRQ_NMI(C64_NMI_HIGH);
        }
        else
        {
            C64_IRQ_NMI(C64_NMI_LOW);
        }

        u32 mode = fc3_mode[((data >> 5) & 0x04) | ((data >> 4) & 0x03)];
        C64_CRT_CONTROL(mode);
        if (mode & STATUS_LED_OFF)
        {
            fc3_register = FC3_REGISTER_HIDE;
        }

        return;
    }
}

static void fc3_init(void)
{
    C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_16K);
    C64_IRQ_NMI(C64_NMI_LOW);
    fc3_register = FC3_REGISTER_ADDR;
}

// The freezer reads character data directly from the cartridge
C64_VIC_BUS_HANDLER_EX(fc3)
