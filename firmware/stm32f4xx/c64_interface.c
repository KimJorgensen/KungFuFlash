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

#define MENU_RAM_SIGNATURE  "KungFu:Menu"

static inline void set_menu_signature(void)
{
    memcpy(scratch_buf, MENU_RAM_SIGNATURE, sizeof(MENU_RAM_SIGNATURE));
}

static inline bool menu_signature(void)
{
    return memcmp(scratch_buf, MENU_RAM_SIGNATURE, sizeof(MENU_RAM_SIGNATURE)) == 0;
}

static inline void invalidate_menu_signature(void)
{
    scratch_buf[0] = 0;
}

/*************************************************
* C64 address bus on PB0-PB15
* Returned as 32 bit value for performance
*************************************************/
static inline uint32_t c64_addr_read()
{
    return GPIOB->IDR;
}

static void c64_address_config(void)
{
    // Make GPIOB input
    GPIOB->MODER = 0;

    // Set output low
    GPIOB->ODR = 0;

    // Set GPIOB to low speed
    GPIOB->OSPEEDR = 0;

    // No pull-up, pull-down
    GPIOB->PUPDR = 0;
}

/*************************************************
* C64 data bus on PC0-PC7
* Returned as 32 bit value for performance
*************************************************/
static inline uint32_t c64_data_read()
{
    return GPIOC->IDR;
}

static inline void c64_data_write(uint8_t data)
{
    // Make PC0-PC7 outout
    *((volatile uint16_t *)&GPIOC->MODER) = 0x5555;

    *((volatile uint8_t *)&GPIOC->ODR) = data;
    __DMB();
}

static inline void c64_data_input(void)
{
    // Make PC0-PC7 input
    *((volatile uint16_t *)&GPIOC->MODER) = 0x0000;
    __DMB();
}

static void c64_data_config(void)
{
    // Make PC0-PC7 input
    c64_data_input();
}

/*************************************************
* C64 control bus on PA0-PA3 and PA6-PA7
* Menu button & special button on PA4 & PA5
* Returned as 32 bit value for performance
*************************************************/
#define C64_WRITE   0x01    // R/W on PA0
#define C64_IO1     0x02    // IO1 on PA1
#define C64_IO2     0x04    // IO1 on PA2
#define C64_BA      0x08    // BA on PA3
#define MENU_BTN    0x10    // Menu button on PA4
#define SPECIAL_BTN 0x20    // Special button on PA5
#define C64_ROML    0x40    // ROML on PA6
#define C64_ROMH    0x80    // ROMH on PA7

static inline uint32_t c64_control_read()
{
    return GPIOA->IDR;
}

static void c64_control_config(void)
{
    // Make PA0-PA3 and PA6-PA7 input
    GPIOA->MODER &= ~(GPIO_MODER_MODER0|GPIO_MODER_MODER1|GPIO_MODER_MODER2|
                      GPIO_MODER_MODER3|GPIO_MODER_MODER6|GPIO_MODER_MODER7);
}

// Wait until the raster beam is in the upper or lower border (if VIC-II is enabled)
static void c64_sync_with_vic(void)
{
    timer_start_us(1000);
    while (!timer_elapsed())
    {
        if (!(c64_control_read() & C64_BA))
        {
            timer_reset();
        }
    }
}

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

static inline void c64_crt_control(uint32_t state)
{
    GPIOC->BSRR = state;
}

static inline void led_off(void)
{
    c64_crt_control(STATUS_LED_OFF);
}

static inline void led_on(void)
{
    c64_crt_control(STATUS_LED_ON);
}

static inline void led_toggle(void)
{
    GPIOC->ODR ^= GPIO_ODR_OD13;
}

static void c64_crt_config(void)
{
    // Cartridge inactive
    c64_crt_control(STATUS_LED_OFF|C64_GAME_HIGH|C64_EXROM_HIGH);

    // Set PC14 & PC15 as open-drain
    GPIOC->OTYPER |= GPIO_OTYPER_OT14|GPIO_OTYPER_OT15;

    // Set PC13-PC15 as output
    MODIFY_REG(GPIOC->MODER, GPIO_MODER_MODER13|GPIO_MODER_MODER14|GPIO_MODER_MODER15,
               GPIO_MODER_MODER13_0|GPIO_MODER_MODER14_0|GPIO_MODER_MODER15_0);
}

/*************************************************
* C64 IRQ and NMI on PA9 & PA10
*************************************************/
#define C64_IRQ_HIGH        GPIO_BSRR_BS9
#define C64_IRQ_LOW         GPIO_BSRR_BR9
#define C64_NMI_HIGH        GPIO_BSRR_BS10
#define C64_NMI_LOW         GPIO_BSRR_BR10
#define C64_IRQ_NMI_HIGH    (C64_IRQ_HIGH|C64_NMI_HIGH)
#define C64_IRQ_NMI_LOW     (C64_IRQ_LOW|C64_NMI_LOW)

static inline void c64_irq_nmi(uint32_t state)
{
    GPIOA->BSRR = state;
}

static void c64_irq_config(void)
{
    c64_irq_nmi(C64_IRQ_NMI_HIGH);

    // Set PA9 & PA10 as open-drain
    GPIOA->OTYPER |= GPIO_OTYPER_OT9|GPIO_OTYPER_OT10;

    // Set PA9 & PA10 as output
    MODIFY_REG(GPIOA->MODER, GPIO_MODER_MODER9|GPIO_MODER_MODER10,
                GPIO_MODER_MODER9_0|GPIO_MODER_MODER10_0);
}

/*************************************************
* C64 clock input on PA8 (timer 1)
* C64 clock is 0.985 MHz (PAL) / 1.023 MHz (NTSC)
*************************************************/
static void c64_clock_config()
{
     // Enable TIM1 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    __DSB();

    // Set PA8 as alternate mode 1 (TIM1_CH1)
    MODIFY_REG(GPIOA->AFR[1], GPIO_AFRH_AFSEL8, GPIO_AFRH_AFSEL8_0);
    MODIFY_REG(GPIOA->MODER, GPIO_MODER_MODER8, GPIO_MODER_MODER8_1);

    // No prescaler, timer runs at ABP2 timer clock speed (168 MHz)
    TIM1->PSC = 0;

    /**** Setup timer 1 to measure clock speed in CCR1 and duty cycle in CCR2 ****/

    // CC1 and CC2 channel is input, IC1 and IC2 is mapped on TI1
    MODIFY_REG(TIM1->CCMR1,
               TIM_CCMR1_CC1S|TIM_CCMR1_CC2S,
               TIM_CCMR1_CC1S_0|TIM_CCMR1_CC2S_1);

    // TI1FP1 active on falling edge. TI1FP2 active on rising edge
    MODIFY_REG(TIM1->CCER,
               TIM_CCER_CC1P|TIM_CCER_CC1NP|TIM_CCER_CC2P|TIM_CCER_CC2NP,
               TIM_CCER_CC1P);

    // Select TI1FP1 as trigger
    MODIFY_REG(TIM1->SMCR, TIM_SMCR_TS|TIM_SMCR_SMS, TIM_SMCR_TS_2|TIM_SMCR_TS_0);

    // Set reset mode
    TIM1->SMCR |= TIM_SMCR_SMS_2;

    // Enable capture 1 and 2
    TIM1->CCER |= TIM_CCER_CC1E|TIM_CCER_CC2E;

    // Disable UEV events and enable counter
    TIM1->CR1 |= TIM_CR1_UDIS|TIM_CR1_CEN;

    /**** Setup phi2 interrupt ****/

    // Generate OC3 interrupt before 0.5 of the C64 clock cycle
    // Value set in c64_interface()
    TIM1->CCR3 = 0;

    // Enable compare mode 1. OC3 is high when TIM1_CNT == TIM1_CCR3
    MODIFY_REG(TIM1->CCMR2, TIM_CCMR2_OC3M, TIM_CCMR2_OC3M_0);

    // Disable all TIM1 (and TIM8) interrupts
    TIM1->DIER = 0;

    // Enable TIM1_CC_IRQn, highest priority
    NVIC_SetPriority(TIM1_CC_IRQn, 0);
    NVIC_EnableIRQ(TIM1_CC_IRQn);
}

// C64_BUS_HANDLER timing
#define NTSC_PHI2_HIGH      93
#define NTSC_PHI2_INT       (NTSC_PHI2_HIGH - 41)
#define NTSC_PHI2_LOW       142

#define PAL_PHI2_HIGH       98
#define PAL_PHI2_INT        (PAL_PHI2_HIGH - 43)
#define PAL_PHI2_LOW        149

// C64_VIC_BUS_HANDLER timing
#ifdef NTSC

#define PHI2_CPU_START      96
#define PHI2_WRITE_DELAY    126
#define PHI2_CPU_END        NTSC_PHI2_LOW

#define PHI2_VIC_START      17
#define PHI2_VIC_DELAY      32
#define PHI2_VIC_END        58

#define C64_CPU_VIC_DELAY() \
    __NOP();                \
    __NOP();                \
    __NOP();                \
    __NOP();                \
    __NOP();                \
    __NOP();

#else // PAL

#define PHI2_CPU_START      102
#define PHI2_WRITE_DELAY    130
#define PHI2_CPU_END        PAL_PHI2_LOW

#define PHI2_VIC_START      18
#define PHI2_VIC_DELAY      33
#define PHI2_VIC_END        61

#define C64_CPU_VIC_DELAY() \
    __NOP();                \
    __NOP();                \
    __NOP();                \
    __NOP();                \
    __NOP();                \
    __NOP();                \
    __NOP();                \
    __NOP();

#endif

#define C64_BUS_HANDLER(name)                                                       \
        C64_BUS_HANDLER_(name##_handler, name##_read_handler, name##_write_handler)

#define C64_BUS_HANDLER_READ(name, read_handler)                                    \
        C64_BUS_HANDLER_(name##_handler, read_handler, name##_write_handler)

#define C64_BUS_HANDLER_(name, read_handler, write_handler)                     \
static void name(void)                                                          \
{                                                                               \
    /* We need to clear the interrupt flag early otherwise the next */          \
    /* interrupt may be delayed */                                              \
    TIM1->SR = ~TIM_SR_CC3IF;                                                   \
    __DSB();                                                                    \
    /* Use debug cycle counter which is faster to access than timer */          \
    DWT->CYCCNT = TIM1->CNT;                                                    \
    __DMB();                                                                    \
    uint32_t phi2_high = DWT->COMP0;                                            \
    while (DWT->CYCCNT < phi2_high);                                            \
    uint32_t addr = c64_addr_read();                                            \
    uint32_t control = c64_control_read();                                      \
    if (control & C64_WRITE)                                                    \
    {                                                                           \
        COMPILER_BARRIER();                                                     \
        if (read_handler(control, addr))                                        \
        {                                                                       \
            /* Wait for phi2 to go low */                                       \
            uint32_t phi2_low = DWT->COMP1;                                     \
            while (DWT->CYCCNT < phi2_low);                                     \
            /* We releases the bus as fast as possible when phi2 is low */      \
            c64_data_input();                                                   \
        }                                                                       \
    }                                                                           \
    else                                                                        \
    {                                                                           \
        COMPILER_BARRIER();                                                     \
        uint32_t data = c64_data_read();                                        \
        write_handler(control, addr, data);                                     \
    }                                                                           \
}

#define C64_VIC_BUS_HANDLER_EX(name)                                                \
        C64_VIC_BUS_HANDLER_EX_(name##_handler, name##_vic_read_handler,            \
                                name##_read_handler, name##_early_write_handler,    \
                                name##_write_handler, C64_VIC_DELAY)

#define C64_VIC_BUS_HANDLER(name)                                                   \
        C64_VIC_BUS_HANDLER_EX_(name##_handler, name##_read_handler,                \
                                name##_read_handler, C64_WRITE_DELAY,               \
                                name##_write_handler, C64_VIC_DELAY)

#define C64_C128_BUS_HANDLER(name)                                                  \
        C64_VIC_BUS_HANDLER_EX_(name##_handler, name##_read_handler,                \
                                name##_read_handler, C64_WRITE_DELAY,               \
                                name##_write_handler, C64_NO_DELAY)

#define C64_NO_DELAY()
#define C64_WRITE_DELAY()                                                           \
    /* Wait for data to become ready on the data bus */                             \
    while (DWT->CYCCNT < PHI2_WRITE_DELAY);

#define C64_VIC_DELAY()                                                             \
    /* Wait for the control bus to become stable */                                 \
    while (DWT->CYCCNT < PHI2_VIC_DELAY);

// This supports VIC-II reads from the cartridge (i.e. character and sprite data)
// but uses 100% CPU - other interrupts are not served due to the interrupt priority
#define C64_VIC_BUS_HANDLER_EX_(name, vic_read_handler, read_handler,           \
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
                while (DWT->CYCCNT < PHI2_CPU_END);                             \
                c64_data_input();                                               \
            }                                                                   \
        }                                                                       \
        else if (!(control & C64_WRITE))                                        \
        {                                                                       \
            early_write_handler();                                              \
            uint32_t data = c64_data_read();                                    \
            write_handler(control, addr, data);                                 \
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
                while (DWT->CYCCNT < PHI2_CPU_END);                             \
                c64_data_input();                                               \
            }                                                                   \
        }                                                                       \
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

#define C64_INSTALL_HANDLER(handler)                    \
    /* Set TIM1_CC_IRQHandler vector */                 \
    ((uint32_t *)0x00000000)[43] = (uint32_t)handler

static inline bool c64_is_ntsc(void)
{
    return TIM1->CCR1 < 167;
}

static inline bool c64_fw_supports_crt(void)
{
#ifdef NTSC
    return c64_is_ntsc();
#else
    return !c64_is_ntsc();
#endif
}

/*************************************************
* C64 interface status
*************************************************/
static inline bool c64_interface_active(void)
{
    return (TIM1->DIER & TIM_DIER_CC3IE) != 0;
}

static void c64_interface(bool state)
{
    if (!state)
    {
        // Capture/Compare 3 interrupt disable
        TIM1->DIER &= ~TIM_DIER_CC3IE;
        TIM1->SR = ~TIM_SR_CC3IF;
        return;
    }

    if (c64_interface_active())
    {
        return;
    }

    uint8_t valid_clock_count = 0;
    uint32_t led_activity = 0;

    // Wait for a valid C64 clock signal
    while (valid_clock_count < 3)
    {
        // NTSC: 161-164, PAL: 168-169
        if(TIM1->CCR1 < 161 || TIM1->CCR1 > 169)
        {
            valid_clock_count = 0;

            // Fast blink if no valid clock
            if (led_activity++ > 15000)
            {
                led_activity = 0;
                led_toggle();
            }
        }
        else
        {
            valid_clock_count++;
            led_on();
        }

        delay_us(2); // Wait more than a clock cycle
    }

    if (c64_is_ntsc())
    {
        // NTSC timing
        TIM1->CCR3 = NTSC_PHI2_INT;     // generate interrupt before phi2 is high

        // Abuse COMPx registers for better performance
        DWT->COMP0 = NTSC_PHI2_HIGH;    // after phi2 is high
        DWT->COMP1 = NTSC_PHI2_LOW;     // before phi2 is low
    }
    else
    {
        // PAL timing
        TIM1->CCR3 = PAL_PHI2_INT;     // generate interrupt before phi2 is high

        // Abuse COMPx registers for better performance
        DWT->COMP0 = PAL_PHI2_HIGH;    // after phi2 is high
        DWT->COMP1 = PAL_PHI2_LOW;     // before phi2 is low
    }

    // Capture/Compare 3 interrupt enable
    TIM1->SR = ~TIM_SR_CC3IF;
    TIM1->DIER |= TIM_DIER_CC3IE;
}

/*************************************************
* C64 reset on PA15
*************************************************/
static inline void c64_reset(bool state)
{
    if (state)
    {
        GPIOA->BSRR = GPIO_BSRR_BS15;
        delay_us(200); // Make sure that the C64 is reset
    }
    else
    {
        GPIOA->BSRR = GPIO_BSRR_BR15;
    }
}

static inline bool c64_is_reset(void)
{
    return (GPIOA->ODR & GPIO_ODR_OD15) != 0;
}

static void c64_enable(void)
{
    c64_interface(true);
    c64_reset(false);
}

static void c64_disable(void)
{
    c64_interface(false);
    c64_reset(true);
}

static void c64_reset_config(void)
{
    c64_reset(true);

    // Set PA15 as output
    MODIFY_REG(GPIOA->MODER, GPIO_MODER_MODER15, GPIO_MODER_MODER15_0);
}

/*************************************************
* Menu button and special button on PA4 & PA5
*************************************************/
static inline bool menu_button(void)
{
    return (GPIOA->IDR & GPIO_IDR_ID4) != 0;
}

static void menu_button_wait_release(void)
{
    while (menu_button());
}

static inline bool special_button(void)
{
    return (GPIOA->IDR & GPIO_IDR_ID5) != 0;
}

static void special_button_wait_release(void)
{
    while (special_button());
}

void EXTI4_IRQHandler(void)
{
    if (EXTI->PR & EXTI_PR_PR4)
    {
        c64_disable();
        restart_to_menu();
    }
}

static void button_config(void)
{
    // Make PA4 and PA5 input
    GPIOA->MODER &= ~(GPIO_MODER_MODER4|GPIO_MODER_MODER5);

    // Enable pull-down
    MODIFY_REG(GPIOA->PUPDR, GPIO_PUPDR_PUPD4|GPIO_PUPDR_PUPD5,
               GPIO_PUPDR_PUPD4_1|GPIO_PUPDR_PUPD5_1);

    // Enable EXTI4 interrupt on PA4
    MODIFY_REG(SYSCFG->EXTICR[1], SYSCFG_EXTICR2_EXTI4, SYSCFG_EXTICR2_EXTI4_PA);
    EXTI->IMR |= EXTI_IMR_MR4;

    // Rising edge trigger on PA4
    EXTI->RTSR |= EXTI_RTSR_TR4;
    EXTI->FTSR &= ~EXTI_FTSR_TR4;

    // Enable EXTI4_IRQHandler, lowest priority
    EXTI->PR = EXTI_PR_PR4;
    NVIC_SetPriority(EXTI4_IRQn, 0x0f);
    NVIC_EnableIRQ(EXTI4_IRQn);
}

/*************************************************
* Configure C64 interface
*************************************************/
static void c64_interface_config(void)
{
    c64_address_config();
    c64_data_config();

    c64_crt_config();
    c64_irq_config();
    c64_reset_config();
    c64_clock_config();

    button_config();
}
