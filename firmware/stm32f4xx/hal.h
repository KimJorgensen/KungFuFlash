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
#define COMPILER_BARRIER() asm volatile("" ::: "memory")

#define FIRMWARE_SIZE   (48*1024)
#define LAUNCHER_ADDR   (FLASH_BASE + FIRMWARE_SIZE)

/* Clock PLLs for 405 and 407 chip */
#if defined (STM32F405xx) || defined (STM32F407xx)
// Main PLL = N * (source_clock / M) / P
// HSE = 8 MHz external oscillator
// fCLK = 336 * (8MHz / 8) / 2 = 168MHz
#define PLL_M	8
#define PLL_Q 	7
#define PLL_P 	2
#define PLL_N 	336
#endif

// 16kB scratch buffer
__attribute__((__section__(".sram"))) static char scratch_buf[16*1024];

static bool filesystem_unmount(void);
static void system_restart(void);
static void restart_to_menu(void);

static void delay_us(uint32_t us);
static void delay_ms(uint32_t ms);

static void timer_start_us(uint32_t us);
static void timer_start_ms(uint32_t ms);
static void timer_reset(void);
static bool timer_elapsed(void);
