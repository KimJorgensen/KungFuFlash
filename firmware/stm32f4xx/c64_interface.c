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

#include "c64_interface.h"

static inline void set_menu_signature(void)
{
    memcpy(MEMU_SIGNATURE_BUF, MENU_RAM_SIGNATURE, sizeof(MENU_RAM_SIGNATURE));
}

static inline bool menu_signature(void)
{
    return memcmp(MEMU_SIGNATURE_BUF, MENU_RAM_SIGNATURE,
                  sizeof(MENU_RAM_SIGNATURE)) == 0;
}

static inline void invalidate_menu_signature(void)
{
    *MEMU_SIGNATURE_BUF = 0;
}

/*************************************************
* C64 address bus on PB0-PB15
*************************************************/
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
*************************************************/
static void c64_data_config(void)
{
    // Make PC0-PC7 input
    C64_DATA_INPUT();
}

/*************************************************
* C64 control bus on PA0-PA3 and PA6-PA7
*************************************************/
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
        if (!(C64_CONTROL_READ() & C64_BA))
        {
            timer_reset();
        }
    }
}

/*************************************************
* C64 IRQ and NMI on PA9 & PA10
*************************************************/
static void c64_irq_config(void)
{
    C64_IRQ_NMI(C64_IRQ_NMI_HIGH);

    // Set PA9 & PA10 as open-drain
    GPIOA->OTYPER |= GPIO_OTYPER_OT9|GPIO_OTYPER_OT10;

    // Set PA9 & PA10 as output
    MODIFY_REG(GPIOA->MODER, GPIO_MODER_MODER9|GPIO_MODER_MODER10,
                GPIO_MODER_MODER9_0|GPIO_MODER_MODER10_0);
}

/*************************************************
* C64 GAME and EXROM on PC14 & PC15
* Status LED on PC13
*************************************************/
static void c64_crt_config(void)
{
    // Cartridge inactive
    C64_CRT_CONTROL(STATUS_LED_OFF|C64_GAME_HIGH|C64_EXROM_HIGH);

    // Set PC14 & PC15 as open-drain
    GPIOC->OTYPER |= GPIO_OTYPER_OT14|GPIO_OTYPER_OT15;

    // Set PC13-PC15 as output
    MODIFY_REG(GPIOC->MODER, GPIO_MODER_MODER13|GPIO_MODER_MODER14|GPIO_MODER_MODER15,
               GPIO_MODER_MODER13_0|GPIO_MODER_MODER14_0|GPIO_MODER_MODER15_0);
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
    TIM1->CCMR1 = TIM_CCMR1_CC1S_0|TIM_CCMR1_CC2S_1;

    // TI1FP1 active on falling edge. TI1FP2 active on rising edge
    TIM1->CCER = TIM_CCER_CC1P;

    // Select TI1FP1 as trigger
    TIM1->SMCR = TIM_SMCR_TS_2|TIM_SMCR_TS_0;

    // Set reset mode
    TIM1->SMCR |= TIM_SMCR_SMS_2;

    // Enable capture 1 and 2
    TIM1->CCER |= TIM_CCER_CC1E|TIM_CCER_CC2E;

    /**** Setup phi2 interrupt ****/

    // Generate CC3 interrupt just before the C64 CPU clock cycle begings
    // Value set by c64_interface()
    TIM1->CCR3 = 52;

    // Set CC4IF on timer owerflow
    TIM1->CCR4 = -1;

    // Enable compare mode 1. OC3REF is high when TIM1_CNT == TIM1_CCR3
    TIM1->CCMR2 = TIM_CCMR2_OC3M_0;

    // Disable all TIM1 interrupts
    TIM1->DIER = 0;

    // Enable TIM1_CC_IRQn, highest priority
    NVIC_SetPriority(TIM1_CC_IRQn, 0);
    NVIC_EnableIRQ(TIM1_CC_IRQn);

    // Enable counter
    TIM1->CR1 = TIM_CR1_CEN;
}

/*************************************************
* C64 diagnostic
*************************************************/
#define DIAG_INIT   (0x00)
#define DIAG_RUN    (0x01)
#define DIAG_STOP   (0x02)

static volatile u32 diag_state;
static volatile u32 diag_phi2_freq;

static void c64_diag_handler(void)
{
    // Clear the interrupt flag
    TIM1->SR = ~TIM_SR_CC3IF;
    // Ensure interrupt flag is cleared
    (void)TIM1->SR;

    if (diag_state == DIAG_RUN)
    {
        // Count number of interrupts within 1 second (168 MHz)
        if (DWT->CYCCNT < 168000000)
        {
            // Check for timer overflow (no phi2 clock)
            if (TIM1->SR & TIM_SR_CC4IF)
            {
                TIM1->SR = ~TIM_SR_CC4IF;
            }
            else
            {
                diag_phi2_freq++;
            }
        }
        else
        {
            diag_state = DIAG_STOP;
        }
    }
    else if (diag_state == DIAG_INIT)
    {
        // Start measuring
        DWT->CYCCNT = 0;
        diag_phi2_freq = 0;
        diag_state = DIAG_RUN;
    }
}

/*************************************************
* C64 interface status
*************************************************/
static void c64_interface_enable_no_check(void)
{
    s8 phi2_offset = dat_file.phi2_offset;

    // Limit offset
    if (phi2_offset > 20)
    {
        phi2_offset = 20;
    }
    else if (phi2_offset < -20)
    {
        phi2_offset = -20;
    }
    dat_file.phi2_offset = phi2_offset;

    if (c64_is_ntsc())  // NTSC timing
    {
        // Generate interrupt before phi2 is high
        TIM1->CCR3 = NTSC_PHI2_INT + phi2_offset;

        // Abuse COMPx registers for better performance
        DWT->COMP0 = NTSC_PHI2_HIGH + phi2_offset;  // After phi2 is high
        DWT->COMP1 = NTSC_PHI2_LOW + phi2_offset;   // Before phi2 is low
    }
    else    // PAL timing
    {
        // Generate interrupt before phi2 is high
        TIM1->CCR3 = PAL_PHI2_INT + phi2_offset;

        // Abuse COMPx registers for better performance
        DWT->COMP0 = PAL_PHI2_HIGH + phi2_offset;   // After phi2 is high
        DWT->COMP1 = PAL_PHI2_LOW + phi2_offset;    // Before phi2 is low
    }
    DWT->COMP2 = -phi2_offset;

    COMPILER_BARRIER();
    __disable_irq();
    // Wait for next interrupt
    TIM1->SR = 0;
    while (!(TIM1->SR & TIM_SR_CC3IF));

    // Capture/Compare 3 interrupt enable
    TIM1->SR = 0;
    TIM1->DIER = TIM_DIER_CC3IE;

    // Release reset here for C64_VIC_BUS_HANDLER
    C64_RESET_RELEASE();
    __enable_irq();
}

static void c64_interface(bool state)
{
    if (!state)
    {
        C64_INTERFACE_DISABLE();
        return;
    }

    if (c64_interface_active())
    {
        return;
    }

    u32 valid_clock_count = 0;
    u32 led_activity = 0;

    // Wait for a valid C64 clock signal
    while (valid_clock_count < 100)
    {
        // NTSC: 161-164, PAL: 168-169
        if (!(TIM1->SR & TIM_SR_CC1IF) || TIM1->CCR1 < 160 || TIM1->CCR1 > 170)
        {
            valid_clock_count = 0;
        }
        else
        {
            valid_clock_count++;
        }

        // Blink rapidly if no valid clock
        if (led_activity++ > 15000)
        {
            led_activity = 0;
            led_toggle();
        }

        delay_us(2); // Wait more than a clock cycle
    }
    led_on();

    c64_interface_enable_no_check();
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
        C64_RESET_RELEASE();
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
static u32 button_pressed(void)
{
    u32 control = 0;
    for (u32 i=0; i<24; i++)    // Debounce
    {
        control |= C64_CONTROL_READ();
        delay_us(500);
    }

    return control & (MENU_BTN|SPECIAL_BTN);
}

static inline bool menu_button_pressed(void)
{
    return (button_pressed() & MENU_BTN) != 0;
}

static inline bool special_button_pressed(void)
{
    return (button_pressed() & SPECIAL_BTN) != 0;
}

void EXTI4_IRQHandler(void)
{
    if (EXTI->PR & EXTI_PR_PR4)
    {
        c64_disable();
        restart_to_menu();
    }
}

static void menu_button_enable(void)
{
    // Enable PA4 interrupt
    EXTI->PR = EXTI_PR_PR4;
    EXTI->IMR |= EXTI_IMR_MR4;
}

static void button_config(void)
{
    // Make PA4 and PA5 input
    GPIOA->MODER &= ~(GPIO_MODER_MODER4|GPIO_MODER_MODER5);

    // Enable pull-down
    MODIFY_REG(GPIOA->PUPDR, GPIO_PUPDR_PUPD4|GPIO_PUPDR_PUPD5,
               GPIO_PUPDR_PUPD4_1|GPIO_PUPDR_PUPD5_1);

    // Rising edge trigger on PA4
    EXTI->RTSR |= EXTI_RTSR_TR4;
    EXTI->FTSR &= ~EXTI_FTSR_TR4;

    // EXTI4 interrupt on PA4
    MODIFY_REG(SYSCFG->EXTICR[1], SYSCFG_EXTICR2_EXTI4, SYSCFG_EXTICR2_EXTI4_PA);

    // Enable EXTI4_IRQHandler, lowest priority
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
