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

#define KFF_BUF (CRT_DAT_BANK(0))
#define KFF_RAM (CRT_RAM_BUF)

// $de01 Command register in KFF RAM
#define KFF_COMMAND (*((volatile u8*)(KFF_RAM + 1)))

// $de04-$de05 Read buffer pointer register in KFF RAM
#define KFF_READ_PTR (*((u16*)(KFF_RAM + 4)))

// $de06-$de07 Write buffer pointer register in KFF RAM
#define KFF_WRITE_PTR (*((u16*)(KFF_RAM + 6)))

// $de09 ID register in KFF RAM (same address as EF3 USB Control register)
#define KFF_ID (*((u8*)(KFF_RAM + 9)))

static void kff_set_command(u8 cmd)
{
    KFF_READ_PTR = 0;
    KFF_WRITE_PTR = 0;

    COMPILER_BARRIER();
    KFF_COMMAND = cmd;
}

static bool kff_get_reply(u8 cmd, u8 *reply)
{
    *reply = KFF_COMMAND;
    if (cmd != *reply)
    {
        KFF_READ_PTR = 0;
        KFF_WRITE_PTR = 0;
        return true;
    }

    return false;
}

static u8 kff_receive_byte(void)
{
    return KFF_BUF[KFF_READ_PTR++];
}

static void kff_send_byte(u8 data)
{
    KFF_BUF[KFF_WRITE_PTR++] = data;
}

/*************************************************
* C64 bus read callback
*************************************************/
FORCE_INLINE bool kff_read_handler(u32 control, u32 addr)
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
            case 0x00:  // $de00 Data register
            {
                C64_DATA_WRITE(KFF_BUF[KFF_READ_PTR++]);
            }
            return true;

            default:    // RAM at rest of $de00-$deff
            {
                C64_DATA_WRITE(KFF_RAM[addr & 0xff]);
            }
            return true;
        }
    }

    return false;
}

/*************************************************
* C64 bus write callback
*************************************************/
FORCE_INLINE void kff_write_handler(u32 control, u32 addr, u32 data)
{
    if (!(control & C64_IO1))
    {
        switch (addr & 0xff)
        {
            case 0x00:  // $de00 Data register
            {
                KFF_BUF[KFF_WRITE_PTR++] = (u8)data;
            }
            break;

            case 0x02:  // $de02 Control register
            {
                u32 mode;
                if (data & 0x01)
                {
                    mode = STATUS_LED_ON|CRT_PORT_16K;
                }
                else
                {
                    mode = STATUS_LED_OFF|CRT_PORT_NONE;
                }
                C64_CRT_CONTROL(mode);
            }
            break;

            default:    // RAM at rest of $de00-$deff
            {
                KFF_RAM[addr & 0xff] = (u8)data;
            }
            break;
        }
    }
}

static void kff_init(void)
{
    C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_ULTIMAX);

    KFF_ID = KFF_ID_VALUE;
    kff_set_command(CMD_NONE);
}

// Allow SDIO and USB to be used while handling C64 bus access.
C64_BUS_HANDLER(kff)
