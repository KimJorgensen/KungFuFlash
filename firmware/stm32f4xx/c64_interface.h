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

/*************************************************
* C64 address bus on PB0-PB15
* Returned as 32 bit value for performance
*************************************************/
#define C64_ADDR_READ() (GPIOB->IDR)

/*************************************************
* C64 data bus on PC0-PC7
* Returned as 32 bit value for performance
*************************************************/
#define C64_DATA_READ() (GPIOC->IDR)

#define C64_DATA_WRITE(data)                    \
    /* Make PC0-PC7 outout */                   \
    *((volatile u16 *)&GPIOC->MODER) = 0x5555;  \
    *((volatile u8 *)&GPIOC->ODR) = (data);     \
    COMPILER_BARRIER()

#define C64_DATA_INPUT()                        \
    /* Make PC0-PC7 input */                    \
    *((volatile u16 *)&GPIOC->MODER) = 0x0000;  \
    COMPILER_BARRIER()

/*************************************************
* C64 control bus on PA0-PA3 and PA6-PA7
* Menu button & special button on PA4 & PA5
* Returned as 32 bit value for performance
*************************************************/
#define C64_WRITE   0x0001  // R/W on PA0
#define C64_IO1     0x0002  // IO1 on PA1
#define C64_IO2     0x0004  // IO1 on PA2
#define C64_BA      0x0008  // BA on PA3
#define MENU_BTN    0x0010  // Menu button on PA4
#define SPECIAL_BTN 0x0020  // Special button on PA5
#define C64_ROML    0x0040  // ROML on PA6
#define C64_ROMH    0x0080  // ROMH on PA7

#define C64_CONTROL_READ() (GPIOA->IDR)

/*************************************************
* C64 IRQ and NMI on PA9 & PA10
*************************************************/
#define C64_IRQ     0x0200  // IRQ on PA9
#define C64_NMI     0x0400  // NMI on PA10

#define C64_IRQ_HIGH        GPIO_BSRR_BS9
#define C64_IRQ_LOW         GPIO_BSRR_BR9
#define C64_NMI_HIGH        GPIO_BSRR_BS10
#define C64_NMI_LOW         GPIO_BSRR_BR10
#define C64_IRQ_NMI_HIGH    (C64_IRQ_HIGH|C64_NMI_HIGH)
#define C64_IRQ_NMI_LOW     (C64_IRQ_LOW|C64_NMI_LOW)

#define C64_IRQ_NMI(state) GPIOA->BSRR = (state)

/*************************************************
* C64 GAME and EXROM on PC14 & PC15
* Status LED on PC13
*************************************************/
#define C64_GAME_HIGH   GPIO_BSRR_BS14
#define C64_GAME_LOW    GPIO_BSRR_BR14
#define C64_EXROM_HIGH  GPIO_BSRR_BS15
#define C64_EXROM_LOW   GPIO_BSRR_BR15

#define STATUS_LED_ON   GPIO_BSRR_BR13
#define STATUS_LED_OFF  GPIO_BSRR_BS13

#define C64_CRT_CONTROL(state) GPIOC->BSRR = (state)

static inline void led_off(void)
{
    C64_CRT_CONTROL(STATUS_LED_OFF);
}

static inline void led_on(void)
{
    C64_CRT_CONTROL(STATUS_LED_ON);
}

static inline void led_toggle(void)
{
    GPIOC->ODR ^= GPIO_ODR_OD13;
}

/*************************************************
* C64 clock input on PA8 (timer 1)
*************************************************/

// C64_BUS_HANDLER timing
#define NTSC_PHI2_HIGH      93
#define NTSC_PHI2_INT       (NTSC_PHI2_HIGH - 41)
#define NTSC_PHI2_LOW       142

#define PAL_PHI2_HIGH       98
#define PAL_PHI2_INT        (PAL_PHI2_HIGH - 43)
#define PAL_PHI2_LOW        149

#define C64_BUS_HANDLER(name)                                                   \
    C64_BUS_HANDLER_(name##_handler, name##_read_handler, name##_write_handler)

#define C64_BUS_HANDLER_READ(name, read_handler)                                \
    C64_BUS_HANDLER_(name##_handler, read_handler, name##_write_handler)

#define C64_BUS_HANDLER_(handler, read_handler, write_handler)                  \
__attribute__((optimize("O2")))                                                 \
static void handler(void)                                                       \
{                                                                               \
    /* We need to clear the interrupt flag early otherwise the next */          \
    /* interrupt may be delayed */                                              \
    TIM1->SR = ~TIM_SR_CC3IF;                                                   \
    u32 phi2_high = DWT->COMP0;                                                 \
    COMPILER_BARRIER();                                                         \
    /* Use debug cycle counter which is faster to access than timer */          \
    DWT->CYCCNT = TIM1->CNT;                                                    \
    COMPILER_BARRIER();                                                         \
    while (DWT->CYCCNT < phi2_high);                                            \
    u32 addr = C64_ADDR_READ();                                                 \
    u32 control = C64_CONTROL_READ();                                           \
    COMPILER_BARRIER();                                                         \
    if (control & C64_WRITE)                                                    \
    {                                                                           \
        if (read_handler(control, addr))                                        \
        {                                                                       \
            /* Wait for phi2 to go low */                                       \
            u32 phi2_low = DWT->COMP1;                                          \
            while (DWT->CYCCNT < phi2_low);                                     \
            /* We releases the bus as fast as possible when phi2 is low */      \
            C64_DATA_INPUT();                                                   \
        }                                                                       \
    }                                                                           \
    else                                                                        \
    {                                                                           \
        u32 data = C64_DATA_READ();                                             \
        write_handler(control, addr, data);                                     \
    }                                                                           \
}

// C64_VIC_BUS_HANDLER timing
// NTSC
#define NTSC_PHI2_CPU_START     96
#define NTSC_PHI2_WRITE_DELAY   126
#define NTSC_PHI2_CPU_END       NTSC_PHI2_LOW

#define NTSC_PHI2_VIC_START     17
#define NTSC_PHI2_VIC_DELAY     32
#define NTSC_PHI2_VIC_END       58

#define NTSC_WRITE_DELAY()                              \
    /* Wait for data to become ready on the data bus */ \
    while (DWT->CYCCNT < NTSC_PHI2_WRITE_DELAY);

#define NTSC_VIC_DELAY()                                \
    /* Wait for the control bus to become stable */     \
    while (DWT->CYCCNT < NTSC_PHI2_VIC_DELAY);

#define NTSC_VIC_DELAY_SHORT()  \
    __NOP();                    \
    __NOP();                    \
    __NOP();                    \
    __NOP();                    \
    __NOP();

#define NTSC_CPU_VIC_DELAY()    \
    NTSC_VIC_DELAY_SHORT();     \
    __NOP();

// PAL
#define PAL_PHI2_CPU_START      102
#define PAL_PHI2_WRITE_DELAY    130
#define PAL_PHI2_CPU_END        PAL_PHI2_LOW

#define PAL_PHI2_VIC_START      18
#define PAL_PHI2_VIC_DELAY      33
#define PAL_PHI2_VIC_END        61

#define PAL_WRITE_DELAY()                               \
    /* Wait for data to become ready on the data bus */ \
    while (DWT->CYCCNT < PAL_PHI2_WRITE_DELAY);

#define PAL_VIC_DELAY()                                 \
    /* Wait for the control bus to become stable */     \
    while (DWT->CYCCNT < PAL_PHI2_VIC_DELAY);

#define PAL_VIC_DELAY_SHORT()   \
    __NOP();                    \
    __NOP();                    \
    __NOP();                    \
    __NOP();                    \
    __NOP();                    \
    __NOP();                    \
    __NOP();

#define PAL_CPU_VIC_DELAY()     \
    PAL_VIC_DELAY_SHORT();      \
    __NOP();

#define C64_NO_DELAY()

#define C64_EARLY_CPU_VIC_HANDLER(name)                                         \
    /* Note: addr gets overwritten here */                                      \
    addr = name##_early_vic_handler(addr);                                      \
    COMPILER_BARRIER();

#define C64_EARLY_VIC_HANDLER(name, timing)                                     \
    C64_EARLY_CPU_VIC_HANDLER(name);                                            \
    timing##_VIC_DELAY_SHORT();

#define C64_VIC_BUS_HANDLER(name)                                               \
    C64_VIC_BUS_HANDLER_(name##_ntsc_handler, NTSC, name)                       \
    C64_VIC_BUS_HANDLER_(name##_pal_handler, PAL, name)

#define C64_VIC_BUS_HANDLER_(handler, timing, name)                             \
    C64_VIC_BUS_HANDLER_EX__(handler,                                           \
                            C64_EARLY_CPU_VIC_HANDLER(name),                    \
                            name##_vic_read_handler, name##_read_handler,       \
                            timing##_WRITE_DELAY(), name##_write_handler,       \
                            C64_EARLY_VIC_HANDLER(name, timing), timing)

#define C64_VIC_BUS_HANDLER_EX(name)                                            \
    C64_VIC_BUS_HANDLER_EX_(name##_ntsc_handler, NTSC, name)                    \
    C64_VIC_BUS_HANDLER_EX_(name##_pal_handler, PAL, name)

#define C64_VIC_BUS_HANDLER_EX_(handler, timing, name)                          \
    C64_VIC_BUS_HANDLER_EX__(handler, timing##_CPU_VIC_DELAY(),                 \
                            name##_vic_read_handler, name##_read_handler,       \
                            name##_early_write_handler(), name##_write_handler, \
                            timing##_VIC_DELAY(), timing)

#define C64_C128_BUS_HANDLER(name)                                              \
    C64_C128_BUS_HANDLER_(name##_ntsc_handler, NTSC, name)                      \
    C64_C128_BUS_HANDLER_(name##_pal_handler, PAL, name)

#define C64_C128_BUS_HANDLER_(handler, timing, name)                            \
    C64_VIC_BUS_HANDLER_EX__(handler, timing##_CPU_VIC_DELAY(),                 \
                            name##_read_handler, name##_read_handler,           \
                            timing##_WRITE_DELAY(), name##_write_handler,       \
                            C64_NO_DELAY(), timing)

// This supports VIC-II reads from the cartridge (i.e. character and sprite data)
// but uses 100% CPU - other interrupts are not served due to the interrupt priority
#define C64_VIC_BUS_HANDLER_EX__(handler, early_cpu_vic_handler,                \
                                 vic_read_handler, read_handler,                \
                                 early_write_handler, write_handler,            \
                                 early_vic_handler, timing)                     \
__attribute__((optimize("O2")))                                                 \
void handler(void)                                                              \
{                                                                               \
    /* As we don't return from this handler, we need to do this here */         \
    C64_RESET_RELEASE();                                                        \
    /* Use debug cycle counter which is faster to access than timer */          \
    DWT->CYCCNT = TIM1->CNT;                                                    \
    COMPILER_BARRIER();                                                         \
    while (true)                                                                \
    {                                                                           \
        /* Wait for CPU cycle */                                                \
        while (DWT->CYCCNT < timing##_PHI2_CPU_START);                          \
        u32 addr = C64_ADDR_READ();                                             \
        COMPILER_BARRIER();                                                     \
        u32 control = C64_CONTROL_READ();                                       \
        /* Check if CPU has the bus (no bad line) */                            \
        if ((control & (C64_BA|C64_WRITE)) == (C64_BA|C64_WRITE))               \
        {                                                                       \
            if (read_handler(control, addr))                                    \
            {                                                                   \
                /* Release bus when phi2 is going low */                        \
                while (DWT->CYCCNT < timing##_PHI2_CPU_END);                    \
                C64_DATA_INPUT();                                               \
            }                                                                   \
        }                                                                       \
        else if (!(control & C64_WRITE))                                        \
        {                                                                       \
            early_write_handler;                                                \
            u32 data = C64_DATA_READ();                                         \
            write_handler(control, addr, data);                                 \
        }                                                                       \
        /* VIC-II has the bus */                                                \
        else                                                                    \
        {                                                                       \
            /* Wait for the control bus to become stable */                     \
            early_cpu_vic_handler;                                              \
            control = C64_CONTROL_READ();                                       \
            if (vic_read_handler(control, addr))                                \
            {                                                                   \
                /* Release bus when phi2 is going low */                        \
                while (DWT->CYCCNT < timing##_PHI2_CPU_END);                    \
                C64_DATA_INPUT();                                               \
            }                                                                   \
        }                                                                       \
        if (control & MENU_BTN)                                                 \
        {                                                                       \
            /* Allow the menu button interrupt handler to run */                \
            break;                                                              \
        }                                                                       \
        /* Wait for VIC-II cycle */                                             \
        while (TIM1->CNT >= 80);                                                \
        DWT->CYCCNT = TIM1->CNT;                                                \
        COMPILER_BARRIER();                                                     \
        while (DWT->CYCCNT < timing##_PHI2_VIC_START);                          \
        addr = C64_ADDR_READ();                                                 \
        COMPILER_BARRIER();                                                     \
        /* Ideally, we would always wait until PHI2_VIC_DELAY here which is */  \
        /* required when the VIC-II has the bus, but we need more cycles */     \
        /* in C128 2 MHz mode where data is read from flash */                  \
        early_vic_handler;                                                      \
        control = C64_CONTROL_READ();                                           \
        if (vic_read_handler(control, addr))                                    \
        {                                                                       \
            /* Release bus when phi2 is going high */                           \
            while (DWT->CYCCNT < timing##_PHI2_VIC_END);                        \
            C64_DATA_INPUT();                                                   \
        }                                                                       \
    }                                                                           \
    C64_INTERFACE_DISABLE();                                                    \
    /* Ensure interrupt flag is cleared before leaving the handler */           \
    (void)TIM1->SR;                                                             \
}

#define C64_INSTALL_HANDLER(handler)            \
    /* Set TIM1_CC_IRQHandler vector */         \
    ((u32 *)0x00000000)[43] = (u32)(handler)

static inline bool c64_is_ntsc(void)
{
    return TIM1->CCR1 < 167;
}

/*************************************************
* C64 interface status
*************************************************/
static inline bool c64_interface_active(void)
{
    return (TIM1->DIER & TIM_DIER_CC3IE) != 0;
}

#define C64_INTERFACE_DISABLE()                 \
    /* Capture/Compare 3 interrupt disable */   \
    TIM1->DIER &= ~TIM_DIER_CC3IE;              \
    TIM1->SR = ~TIM_SR_CC3IF

/*************************************************
* C64 reset on PA15
*************************************************/
#define C64_RESET_RELEASE()                     \
    GPIOA->BSRR = GPIO_BSRR_BR15

/*************************************************
* Menu button and special button on PA4 & PA5
*************************************************/
static inline bool menu_button_pressed(void)
{
    return (GPIOA->IDR & GPIO_IDR_ID4) != 0;
}

static inline bool special_button_pressed(void)
{
    return (GPIOA->IDR & GPIO_IDR_ID5) != 0;
}
