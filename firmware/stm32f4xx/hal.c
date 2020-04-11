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
	while(!(RCC->CR & RCC_CR_HSERDY));

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
	while(!(RCC->CR & RCC_CR_PLLRDY));

	// Enable prefetch, instruction cache and data cache
	// Set latency to 5 wait states
	FLASH->ACR = FLASH_ACR_PRFTEN|FLASH_ACR_ICEN|FLASH_ACR_DCEN|
                 FLASH_ACR_LATENCY_5WS;

	// Select the main PLL as system clock source
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SW_PLL);
	// Wait till the main PLL is used as system clock source
	while(!(RCC->CFGR & RCC_CFGR_SWS_PLL));
}

/*************************************************
* SysTick interrupt handler
*************************************************/
static volatile uint32_t ticks;

void SysTick_Handler(void)
{
	ticks++;
}

static inline uint32_t elapsed_ms(uint32_t tprev)
{
	return ticks - tprev;
}

static inline uint32_t tick_add_ms(uint32_t ms)
{
	return ticks + ms;
}

static inline void delay_ms(uint32_t ms)
{
	uint32_t wait_until = tick_add_ms(ms);
	while (ticks < wait_until);
}

static void systick_disable(void)
{
    SysTick->CTRL = 0;
}

/*************************************************
* Debug cycle counter
* Based on https://stackoverflow.com/questions/23612296/how-to-obtain-reliable-cortex-m4-short-delays
*************************************************/
static inline void delay_us(uint32_t us)
{
    DWT->CYCCNT = 0;
	while(DWT->CYCCNT < (168 * us));
}

static inline void delay_50ns()
{
    DWT->CYCCNT = 0;
	while(DWT->CYCCNT < 9);  // 9 / 168 MHz = 53.6 ns
}

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
    uint32_t *buf32 = (uint32_t *)buf;
    while (len)
    {
        CRC->DR = *buf32++;
        len -= 4;
    }
}

static inline uint32_t crc_get(void)
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

static void system_restart(void)
{
    filesystem_unmount();
    led_off();

    c64_disable();
    NVIC_SystemReset();
    while (true);
}

static void restart_to_menu(void)
{
    set_menu_signature();
    system_restart();
}

/************************************************/
static void configure_system(void)
{
    sysclk_config();
    dwt_cyccnt_config();
    // SysTick will be running at 168MHz
    // passing 168000 here will give us 1ms ticks
    SysTick_Config(168000);

    // Just higher priority than menu button interrupt
    NVIC_SetPriority(SysTick_IRQn, 0xe);

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
