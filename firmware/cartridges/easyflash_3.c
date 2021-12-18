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

// EasyFlash 3 USB Control
#define EF3_RX_RDY  (0x80)
#define EF3_TX_RDY  (0x40)
#define EF3_NOT_RDY (0x00)

static volatile u32 ef3_usb_rx_rdy = EF3_NOT_RDY;
static volatile u32 ef3_usb_tx_rdy = EF3_TX_RDY;

// EasyFlash 3 USB Data
static volatile u8 ef3_usb_rx_data;
static volatile u32 ef3_usb_tx_data;

static inline bool ef3_can_putc(void)
{
    return !ef3_usb_rx_rdy;
}

static void ef3_putc(u8 data)
{
    while (ef3_usb_rx_rdy);

    ef3_usb_rx_data = data;
    ef3_usb_rx_rdy = EF3_RX_RDY;
}

static inline bool ef3_gotc(void)
{
    return !ef3_usb_tx_rdy;
}

static u8 ef3_getc(void)
{
    while (ef3_usb_tx_rdy);

    u32 data = ef3_usb_tx_data;
    COMPILER_BARRIER();
    ef3_usb_tx_rdy = EF3_TX_RDY;
    return (u8)data;
}

/*************************************************
* C64 bus read callback
*************************************************/
FORCE_INLINE bool ef3_read_handler(u32 control, u32 addr)
{
    if ((control & (C64_ROML|C64_ROMH)) != (C64_ROML|C64_ROMH))
    {
        C64_DATA_WRITE(crt_ptr[addr & 0x3fff]);
        return true;
    }

    if (!(control & C64_IO1))
    {
        switch (addr & 0xff)
        {
            // $de09 EF3 USB Control register
            case 0x09:
            {
                C64_DATA_WRITE((u8)(ef3_usb_rx_rdy | ef3_usb_tx_rdy));
            }
            return true;

            // $de0a EF3 USB Data register
            case 0x0a:
            {
                C64_DATA_WRITE(ef3_usb_rx_data);
                ef3_usb_rx_rdy = EF3_NOT_RDY;
            }
            return true;
        }

        return false;
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
FORCE_INLINE void ef3_write_handler(u32 control, u32 addr, u32 data)
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

            // $de0a EF3 USB Data register
            case 0x0a:
            {
                ef3_usb_tx_data = data;
                ef3_usb_tx_rdy = EF3_NOT_RDY;
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

// Allow SDIO and USB to be used while handling C64 bus access.
// Does not support VIC-II or C128 2 MHz reads from cartridge
C64_BUS_HANDLER(ef3)
