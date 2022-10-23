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
#include "system_stm32f4xx.c"
#include "hal.h"
#include "c64_interface.c"
#include "diskio.c"
#include "usb.c"
#include "flash.c"

/*************************************************
* Configure system clock to 168 MHz
* Based on https://github.com/fcayci/stm32f4-bare-metal
*************************************************/
static void sysclk_config(void)
{
    // Enable HSE
    RCC->CR |= RCC_CR_HSEON;
    // Wait till HSE is ready
    while (!(RCC->CR & RCC_CR_HSERDY));

    // Enable power interface clock
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    __DSB();

    // set voltage scale to 1 for max frequency
    PWR->CR |= PWR_CR_VOS;

    // set AHB prescaler to /1, APB1 prescaler to /4 and ABP2 prescaper to /2
    RCC->CFGR |= ((0 << RCC_CFGR_HPRE_Pos) |
                 (0x5 << RCC_CFGR_PPRE1_Pos) |
                 (0x4 << RCC_CFGR_PPRE2_Pos));

    // Set M, N, P and Q PLL dividers and PLL source to HSE
    RCC->PLLCFGR = (PLL_M << RCC_PLLCFGR_PLLM_Pos) |
                   (PLL_N << RCC_PLLCFGR_PLLN_Pos) |
                   (((PLL_P >> 1) -1) << RCC_PLLCFGR_PLLP_Pos) |
                   (PLL_Q << RCC_PLLCFGR_PLLQ_Pos) |
                   RCC_PLLCFGR_PLLSRC;

    // Enable the main PLL
    RCC->CR |= RCC_CR_PLLON;
    // Wait till the main PLL is ready
    while (!(RCC->CR & RCC_CR_PLLRDY));

    // Enable prefetch, instruction cache and data cache
    // Set latency to 5 wait states
    FLASH->ACR = FLASH_ACR_PRFTEN|FLASH_ACR_ICEN|FLASH_ACR_DCEN|
                 FLASH_ACR_LATENCY_5WS;

    // Select the main PLL as system clock source
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SW_PLL);
    // Wait till the main PLL is used as system clock source
    while (!(RCC->CFGR & RCC_CFGR_SWS_PLL));
}

/*************************************************
* System tick timer
*************************************************/
static inline void timer_reset(void)
{
    SysTick->VAL = 0;
}

static inline void timer_start_us(u32 us)
{
    SysTick->LOAD = (168 / 8) * us;
    timer_reset();
}

// Max supported value is 798 ms
static inline void timer_start_ms(u32 ms)
{
    timer_start_us(1000 * ms);
}

static inline bool timer_elapsed()
{
    return (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk);
}

static inline void delay_us(u32 us)
{
    timer_start_us(us);
    while (!timer_elapsed());
}

static void delay_ms(u32 ms)
{
    for (u32 i=0; i<ms; i++)
    {
        delay_us(1000);
    }
}

static void systick_config(void)
{
    // Stop timer
    timer_start_us(0);

    // Enable SysTick timer and use HCLK/8 as clock source
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;
}

/*************************************************
* Floating-point unit
*************************************************/
static void fpu_config(void)
{
    // No automatic FPU state preservation
    MODIFY_REG(FPU->FPCCR, FPU_FPCCR_LSPEN_Msk|FPU_FPCCR_ASPEN_Msk, 0);

    // No floating-point context active
    __set_CONTROL(0);
}

/*************************************************
* Debug cycle counter
*************************************************/
static void dwt_cyccnt_config(void)
{
    // Enable DWT
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;

    // Enable CPU cycle counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/*************************************************
* Hardware CRC
*************************************************/
static inline void crc_reset(void)
{
    CRC->CR |= CRC_CR_RESET;
}

static void crc_calc(void *buf, size_t len)
{
    u32 *buf32 = (u32 *)buf;
    while (len)
    {
        CRC->DR = *buf32++;
        len -= 4;
    }
}

static inline u32 crc_get(void)
{
    return CRC->DR;
}

static void crc_config(void)
{
    // Enable CRC clock
    RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;
    __DSB();

    crc_reset();
}

/************************************************/

NO_RETURN system_restart(void)
{
    filesystem_unmount();
    led_off();

    c64_disable();
    NVIC_SystemReset();
    while (true);
}

NO_RETURN restart_to_menu(void)
{
    set_menu_signature();
    system_restart();
}

/************************************************/
static void configure_system(void)
{
    sysclk_config();
    fpu_config();
    dwt_cyccnt_config();
    systick_config();

    // Enable system configuration controller clock
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    __DSB();

    // Enable GPIOA, GPIOB, GPIOC, and GPIOD clock
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN|RCC_AHB1ENR_GPIOBEN|
                    RCC_AHB1ENR_GPIOCEN|RCC_AHB1ENR_GPIODEN;
    __DSB();

    crc_config();

    usb_config();
    c64_interface_config();
}
