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
static uint32_t const ef_mode[8] =
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

// EasyFlash 3 USB Control
#define EF3_RX_RDY  (0x80)
#define EF3_TX_RDY  (0x40)
#define EF3_NOT_RDY (0x00)

static volatile uint8_t ef3_usb_rx_rdy = EF3_NOT_RDY;
static volatile uint8_t ef3_usb_tx_rdy = EF3_TX_RDY;

// EasyFlash 3 USB Data
static volatile uint8_t ef3_usb_rx_data;
static volatile uint8_t ef3_usb_tx_data;

static inline bool ef3_can_putc(void)
{
    return !ef3_usb_rx_rdy;
}

static void ef3_putc(uint8_t data)
{
    while (ef3_usb_rx_rdy);

    ef3_usb_rx_data = data;
    ef3_usb_rx_rdy = EF3_RX_RDY;
}

static inline bool ef3_gotc(void)
{
    return !ef3_usb_tx_rdy;
}

static uint8_t ef3_getc(void)
{
    while (ef3_usb_tx_rdy);

    uint8_t data = ef3_usb_tx_data;
    COMPILER_BARRIER();
    ef3_usb_tx_rdy = EF3_TX_RDY;
    return data;
}

/*************************************************
* C64 bus read callback
*************************************************/
static inline bool ef_read_handler(uint8_t control, uint16_t addr)
{
    if ((control & (C64_ROML|C64_ROMH)) != (C64_ROML|C64_ROMH))
    {
        c64_data_write(crt_ptr[addr & 0x3fff]);
        return true;
    }

    if (!(control & C64_IO1))
    {
        switch (addr)
        {
            // $de09 EF3 USB Control register
            case 0xde09:
            {
                c64_data_write(ef3_usb_rx_rdy | ef3_usb_tx_rdy);
                return true;
            }

            // $de0a EF3 USB Data register
            case 0xde0a:
            {
                c64_data_write(ef3_usb_rx_data);
                ef3_usb_rx_rdy = EF3_NOT_RDY;
                return true;
            }
        }

        return false;
    }

    if (!(control & C64_IO2))
    {
        // EasyFlash RAM at $df00-$dfff
        c64_data_write(crt_ram_buf[addr & 0xff]);
        return true;
    }

    return false;
}

/*************************************************
* C64 bus write callback
*************************************************/
static inline void ef_write_handler(uint8_t control, uint16_t addr, uint8_t data)
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
                uint32_t mode = ef_mode[((data >> 5) & 0x04) | (data & 0x02) |
                                        (((data >> 2) & 0x01) & ~data)];
                c64_crt_control(mode);
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
        crt_ram_buf[addr & 0xff] = data;
        return;
    }
}

static void ef_init(void)
{
    c64_crt_control(STATUS_LED_ON|CRT_PORT_ULTIMAX);
}

// Allow SDIO and USB to be used while handling C64 bus access.
// Does not support VIC-II or C128 2 MHz reads from cartridge
C64_BUS_HANDLER_(ef_launcher_handler, ef_read_handler, ef_write_handler)

// Needed for C128 read accesses at 2 MHz (e.g. for Prince of Persia)
C64_C128_BUS_HANDLER(ef)
