/*
 * Copyright (c) 2019-2020 Kim Jørgensen
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

static uint32_t const kcs_mode[4] =
{
    STATUS_LED_ON|CRT_PORT_16K,
    STATUS_LED_ON|CRT_PORT_8K,
    STATUS_LED_ON|CRT_PORT_ULTIMAX,
    STATUS_LED_OFF|CRT_PORT_NONE
};

static uint8_t kcs_register;

/*************************************************
* C64 bus read callback (VIC-II cycle)
*************************************************/
static inline bool kcs_vic_read_handler(uint8_t control, uint16_t addr)
{
    if ((control & (C64_ROML|C64_ROMH)) != (C64_ROML|C64_ROMH))
    {
        c64_data_write(crt_ptr[addr & 0x3fff]);
        return true;
    }

    return false;
}

/*************************************************
* C64 bus read callback (CPU cycle)
*************************************************/
static inline bool kcs_read_handler(uint8_t control, uint16_t addr)
{
    if ((control & (C64_IO1|C64_ROML|C64_ROMH)) != (C64_IO1|C64_ROML|C64_ROMH))
    {
        c64_data_write(crt_ptr[addr & 0x3fff]);
        if (!(control & C64_IO1))
        {
            kcs_register = (addr & 0x02) | 0x01;
            c64_crt_control(kcs_mode[kcs_register]);
        }

        return true;
    }

    if (!(control & C64_IO2))
    {
        if (!(addr & 0x80))
        {
            c64_data_write(crt_ram_buf[addr & 0x7f]);
        }
        else
        {
            c64_data_write(kcs_register << 6);
        }

        return true;
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
static inline void kcs_early_write_handler(void)
{
    // Use 3 consecutive writes to detect IRQ/NMI
    if (freezer_state && ++freezer_state == FREEZE_3_WRITES)
    {
        kcs_register = 0x02;
        c64_crt_control(kcs_mode[kcs_register]);
        freezer_state = FREEZE_RESET;
    }
}

/*************************************************
* C64 bus write callback
*************************************************/
static inline void kcs_write_handler(uint8_t control, uint16_t addr, uint8_t data)
{
    if (!(control & C64_IO1))
    {
        kcs_register = addr & 0x02;
        c64_crt_control(kcs_mode[kcs_register]);
        return;
    }

    if (!(control & C64_IO2))
    {
        if (!(addr & 0x80))
        {
            crt_ram_buf[addr & 0x7f] = data;
        }

        c64_irq_nmi(C64_IRQ_NMI_HIGH);
        return;
    }
}

static void kcs_init(void)
{
    kcs_register = 0;
    c64_crt_control(kcs_mode[kcs_register]);
    crt_ptr = crt_banks[0];
}

// The freezer reads character data directly from the cartridge
C64_VIC_BUS_HANDLER_EX(kcs)
