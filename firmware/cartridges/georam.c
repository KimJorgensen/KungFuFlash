/*
 * Copyright (c) 2019-2020 Kim Jørgensen
 *
 * Georam cart support written and (c) 2020 Chris van Dongen
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

static uint32_t ramoffset;
static uint32_t bank, offset;


/*************************************************
* C64 bus read callback
*************************************************/
static inline bool georam_read_handler(uint32_t control, uint32_t addr)
{
    if (!(control & C64_IO1))
    {
        c64_data_write(crt_ptr[ramoffset+(addr & 0xff)]);
        return true;
    }


    return false;
}

/*************************************************
* C64 bus write callback
*************************************************/
static inline void georam_write_handler(uint32_t control, uint32_t addr, uint32_t data)
{
    if (!(control & C64_IO1))
    {
        crt_ptr[ramoffset+(addr & 0x0ff)]=(uint8_t)data;
    }


    if (!(control & C64_IO2))
    {
	if(addr==0xDFFF) bank=data;		// range (0-127)
	if(addr==0xDFFE) offset=data;		// range (0-63)

	bank&=0x03;				// we only have 64k avail.

	ramoffset=((bank&0x07F<<14)||((offset&0x03f)<<8));
    }
}

/*************************************************
* init georam
*************************************************/
void georam_init(void)
{
    // ram init
    for(int i=0; i<(64*1024); i++)
    {
	crt_ptr[i]=0x00;
    }

    c64_crt_control(STATUS_LED_ON|CRT_PORT_NONE);
    ramoffset=0;
    offset=0;
    bank=0;
}

// Support MAX cartridges where character and sprite data is read from the cartridge
C64_VIC_BUS_HANDLER(georam)
