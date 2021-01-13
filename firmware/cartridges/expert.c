/*
 * Copyright (c) 2019-2020 Kim Jørgensen
 *
 * Expert cart support written and (c) 2020 Chris van Dongen
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

#define EXPERT_IDLE		0	// waiting for button
#define EXPERT_ACTIVE		1	// button pressed
#define EXPERT_ULTIMAX		2	// ultimax 
#define EXPERT_PROGRAM		3	// RAM can be programmed

static uint32_t expert_mode;

static uint32_t const expert_modes[4] =
{
    STATUS_LED_OFF|CRT_PORT_NONE,		// idle
    STATUS_LED_ON|CRT_PORT_NONE,		// active
    STATUS_LED_ON|CRT_PORT_ULTIMAX,		// memacces at 0xe000 or 0x8000
    STATUS_LED_OFF|CRT_PORT_NONE		// program
};


// we need some extra cycles to switch out ultimax mode
#define PHI2_CPU_END_EXPERT	(PHI2_CPU_END-3)

// we need a special cart handler 
#define C64_VIC_BUS_HANDLER_EXPERT(name)                                                \
        C64_VIC_BUS_HANDLER_EXPERT_(name##_handler, name##_vic_read_handler,            \
                                name##_read_handler, name##_early_write_handler,    \
                                name##_write_handler, C64_VIC_DELAY)


// This supports VIC-II reads from the cartridge (i.e. character and sprite data)
// but uses 100% CPU - other interrupts are not served due to the interrupt priority
#define C64_VIC_BUS_HANDLER_EXPERT_(name, vic_read_handler, read_handler,           \
                                early_write_handler, write_handler, vic_delay)  \
void name(void)                                                                 \
{                                                                               \
    /* As we don't return from this handler, we need to do this here */         \
    c64_reset(false);                                                           \
    /* Use debug cycle counter which is faster to access than timer */          \
    DWT->CYCCNT = TIM1->CNT;                                                    \
    __DMB();                                                                    \
    while (true)                                                                \
    {                                                                           \
        /* Wait for CPU cycle */                                                \
        while (DWT->CYCCNT < PHI2_CPU_START);                                   \
        uint32_t addr = c64_addr_read();                                        \
        COMPILER_BARRIER();                                                     \
        uint32_t control = c64_control_read();                                  \
        /* Check if CPU has the bus (no bad line) */                            \
        if ((control & (C64_BA|C64_WRITE)) == (C64_BA|C64_WRITE))               \
        {                                                                       \
            if (read_handler(control, addr))                                    \
            {                                                                   \
                /* Release bus when phi2 is going low */                        \
                while (DWT->CYCCNT < PHI2_CPU_END_EXPERT);                      \
                c64_data_input();                                               \
            }                                                                   \
        }                                                                       \
        else if (!(control & C64_WRITE))                                        \
        {                                                                       \
            early_write_handler();                                              \
            uint32_t data = c64_data_read();                                    \
            write_handler(control, addr, data);                                 \
            while (DWT->CYCCNT < PHI2_CPU_END_EXPERT);                          \
        }                                                                       \
        /* VIC-II has the bus */                                                \
        else                                                                    \
        {                                                                       \
            /* Wait for the control bus to become stable */                     \
            C64_CPU_VIC_DELAY()                                                 \
            control = c64_control_read();                                       \
            if (vic_read_handler(control, addr))                                \
            {                                                                   \
                /* Release bus when phi2 is going low */                        \
                while (DWT->CYCCNT < PHI2_CPU_END_EXPERT);                      \
                c64_data_input();                                               \
            }                                                                   \
        }                                                                       \
	c64_crt_control(expert_modes[expert_mode]); /* reset ultimax */         \
        if (control & MENU_BTN)                                                 \
        {                                                                       \
            /* Allow the menu button interrupt handler to run */                \
            c64_interface(false);                                               \
            break;                                                              \
        }                                                                       \
        /* Wait for VIC-II cycle */                                             \
        while (TIM1->CNT >= 80);                                                \
        DWT->CYCCNT = TIM1->CNT;                                                \
        __DMB();                                                                \
        while (DWT->CYCCNT < PHI2_VIC_START);                                   \
        addr = c64_addr_read();                                                 \
        COMPILER_BARRIER();                                                     \
        /* Ideally, we would always wait until PHI2_VIC_DELAY here which is */  \
        /* required when the VIC-II has the bus, but we need more cycles */     \
        /* in C128 2 MHz mode where data is read from flash */                  \
        vic_delay();                                                            \
        control = c64_control_read();                                           \
        if (vic_read_handler(control, addr))                                    \
        {                                                                       \
            /* Release bus when phi2 is going high */                           \
            while (DWT->CYCCNT < PHI2_VIC_END);                                 \
            c64_data_input();                                                   \
        }                                                                       \
    }                                                                           \
    TIM1->SR = ~TIM_SR_CC3IF;                                                   \
    __DMB();                                                                    \
}


/*************************************************
* Make it programmable the first boot; next boot
* it is activated
*************************************************/

#define EXPERT_SIGNATURE  "Expert:booted"

// we use scratch_buf+10 as scratch_buff seems to be overwritten
// probably we need a better way?

static inline void set_expert_signature(void)
{
    memcpy(scratch_buf+10, EXPERT_SIGNATURE, sizeof(EXPERT_SIGNATURE));
}

static inline bool expert_signature(void)
{
    return memcmp(scratch_buf+10, EXPERT_SIGNATURE, sizeof(EXPERT_SIGNATURE)) == 0;
}

static inline void invalidate_expert_signature(void)
{
    scratch_buf[10] = 0;
}




/*************************************************
* C64 bus read callback (VIC-II cycle)
*************************************************/
static inline bool expert_vic_read_handler(uint32_t control, uint32_t addr)
{

    // no vic read needed

    return false;
}

/*************************************************
* C64 bus read callback (CPU cycle)
*************************************************/
static inline bool expert_read_handler(uint32_t control, uint32_t addr)
{
    // when active, the kernal and $8000 is presented in ultimax mode
    if((expert_mode==EXPERT_ACTIVE)&&(((addr&0xe000)==0xe000)||((addr&0xe000)==0x8000)))
    {
	c64_crt_control(expert_modes[EXPERT_ULTIMAX]); /* switch to ultimax if we access kernal or 0x8000 */
        c64_data_write(crt_ptr[addr & 0x1fff]);
        return true;
    }

    // access to IO1 releases NMI
    if (!(control & C64_IO1))
    {
//	c64_irq_nmi(C64_IRQ_NMI_HIGH);
	expert_mode=EXPERT_IDLE;
    }

    // handle external NMI (freeze button doesn't work)
    if ((!((GPIOA->IDR)&GPIO_IDR_ID10))&&(expert_mode==EXPERT_IDLE)) 		// nmi is on gpioa-10
    {
        freezer_state = FREEZE_START;
    }

    return false;
}

/*************************************************
* C64 bus write callback (early)
*************************************************/
static inline void expert_early_write_handler(void)
{
    // Use 3 consecutive writes to detect IRQ/NMI
    if (freezer_state && ++freezer_state == FREEZE_3_WRITES)
    {
	expert_mode=EXPERT_ACTIVE;
	c64_crt_control(expert_modes[expert_mode]);
        freezer_state = FREEZE_RESET;
    }
}

/*************************************************
* C64 bus write callback
*************************************************/
static inline void expert_write_handler(uint32_t control, uint32_t addr, uint32_t data)
{
    if((expert_mode==EXPERT_ACTIVE)&&(((addr&0xe000)==0xe000)||((addr&0xe000)==0x8000)))
    {
	c64_crt_control(expert_modes[EXPERT_ULTIMAX]); /* switch to ultimax if we access kernal or roml */
        crt_ptr[addr & 0x1fff]=data;
    }

    // access to IO1 releases NMI
    if (!(control & C64_IO1))
    {
//	c64_irq_nmi(C64_IRQ_NMI_HIGH);
	expert_mode=EXPERT_IDLE;
    }

    // if we are in program mode we are writable at 0x8000
    if((expert_mode==EXPERT_PROGRAM)&&((addr&0xe000)==0x8000))
    {
	crt_ptr[addr & 0x1FFF]=data;
    }
}

/*************************************************
* init when loading an expert image
*************************************************/
static void expert_init(void)
{
	crt_ptr = crt_banks[0];

	expert_mode=EXPERT_ACTIVE;

	c64_crt_control(expert_modes[expert_mode]);
}

/*************************************************
* init when called as expansion
*************************************************/

static void expert_expansion_init(void)
{
	int i=0;

//	crt_ptr = crt_banks[0];
	crt_ptr = (uint8_t*)scratch_buf+8192;

	if(expert_signature())
	{
		expert_mode=EXPERT_ACTIVE;

	}
	else
	{
		expert_mode=EXPERT_PROGRAM;
		set_expert_signature();

		for(i=0; i<8192; i++) crt_ptr[i]=0x00;
	}

	c64_crt_control(expert_modes[expert_mode]);
}

// The freezer reads character data directly from the cartridge
C64_VIC_BUS_HANDLER_EXPERT(expert)

