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

// Use menu signature buf to store the reset toggle
#define RESET_TOOGLE *MEMU_SIGNATURE_BUF
static u32 io1_state;

/*************************************************
* C64 bus read callback (VIC-II cycle)
*************************************************/
FORCE_INLINE bool fm_vic_read_handler(u32 control, u32 addr)
{
    // Not needed
    return false;
}

/*************************************************
* C64 bus read callback (CPU cycle)
*************************************************/
FORCE_INLINE bool fm_read_handler(u32 control, u32 addr)
{
    if ((control & (C64_ROML|C64_ROMH)) != (C64_ROML|C64_ROMH))
    {
        C64_DATA_WRITE(crt_ptr[addr & 0x1fff]);
        return true;
    }

    if (!(control & C64_IO1))
    {
        C64_CRT_CONTROL(io1_state);
        C64_IRQ_NMI(C64_NMI_HIGH);
        return false;
    }

    if (!(control & C64_IO2))
    {
        C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_NONE);
        crt_ptr = crt_rom_ptr;
        return false;
    }

    if (control & SPECIAL_BTN)
    {
        special_button = SPECIAL_PRESSED;
    }
    else if (special_button)
    {
        C64_IRQ_NMI(C64_NMI_LOW);
        special_button = SPECIAL_RELEASED;
        freezer_state = FREEZE_START;
    }

    return false;
}

/*************************************************
* C64 bus write callback (early)
*************************************************/
FORCE_INLINE void fm_early_write_handler(void)
{
    // Use 3 consecutive writes to detect IRQ/NMI
    if (freezer_state && ++freezer_state == FREEZE_3_WRITES)
    {
        C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_ULTIMAX);
        freezer_state = FREEZE_RESET;
        io1_state = STATUS_LED_ON|CRT_PORT_8K;
    }
}

/*************************************************
* C64 bus write callback
*************************************************/
FORCE_INLINE void fm_write_handler(u32 control, u32 addr, u32 data)
{
    // Not needed
}

static void fm_init(DAT_CRT_HEADER *crt_header)
{
    C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_8K);
    io1_state = STATUS_LED_OFF|CRT_PORT_NONE;

    // Toggle ROM A14 on reset if 32 kb cartridge
    if (RESET_TOOGLE == 1 && crt_header->banks > 1)
    {
        RESET_TOOGLE = 0;
        crt_ptr = crt_banks[1];
    }
    else
    {
        RESET_TOOGLE = 1;
        crt_ptr = crt_banks[0];

    }

    // Set ROM A13 on IO2 read if freeze machine
    if (crt_header->type == CRT_FREEZE_MACHINE)
    {
        crt_rom_ptr = crt_ptr + 0x2000;
    }
    else
    {
        crt_rom_ptr = crt_ptr;
    }
}

// VIC-II read support is not needed, but it is less timing critical
C64_VIC_BUS_HANDLER_EX(fm)
