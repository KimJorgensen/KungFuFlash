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

static uint8_t rgcd_bank_xor;

/*************************************************
* C64 bus read callback
*************************************************/
static inline bool rgcd_read_handler(uint32_t control, uint32_t addr)
{
    if (!(control & C64_ROML))
    {
        c64_data_write(crt_ptr[addr & 0x1fff]);
        return true;
    }

    return false;
}

/*************************************************
* C64 bus write callback
*************************************************/
static inline void rgcd_write_handler(uint32_t control, uint32_t addr, uint32_t data)
{
    if (!(control & C64_IO1))
    {
        if (!(data & 0x8))
        {
            uint8_t bank = (data ^ rgcd_bank_xor) & 0x7;
            crt_ptr = crt_banks[bank];
        }
        else
        {
            // Disable cartridge
            c64_crt_control(STATUS_LED_OFF|CRT_PORT_NONE);
        }
    }
}

static void rgcd_init(DAT_CRT_HEADER *crt_header)
{
    // Hucky cartridge has the banks in reverse order
    if (crt_header->hw_rev)
    {
        crt_ptr = crt_banks[0x7];
        rgcd_bank_xor = 0x7;
    }

    c64_crt_control(STATUS_LED_ON|CRT_PORT_8K);
}

C64_BUS_HANDLER(rgcd)
