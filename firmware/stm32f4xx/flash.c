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
#define FLASH_KEYR_KEY1     0x45670123
#define FLASH_KEYR_KEY2     0xCDEF89AB

static void flash_unlock()
{
    FLASH->KEYR = FLASH_KEYR_KEY1;
    FLASH->KEYR = FLASH_KEYR_KEY2;
}

static inline void flash_lock()
{
    FLASH->CR |= FLASH_CR_LOCK;
}

/* For STM32F40x
Sector  Size
    0   16 Kbytes
    1   16 Kbytes
    2   16 Kbytes
    3   16 Kbytes
    4   64 Kbytes
    5   128 Kbytes
    6   128 Kbytes
    .   .
   11   128 Kbytes
*/
static void flash_sector_erase(u8 sector)
{
    // Wait if a flash memory operation is in progress
    while (FLASH->SR & FLASH_SR_BSY);

    // Set sector number and parallelism to x32
    MODIFY_REG(FLASH->CR, FLASH_CR_PSIZE|FLASH_CR_SNB|FLASH_CR_MER|FLASH_CR_PG,
               FLASH_CR_PSIZE_1|FLASH_CR_SER|
               ((sector << FLASH_CR_SNB_Pos) & FLASH_CR_SNB_Msk));

    // Start the erase operation
    FLASH->CR |= FLASH_CR_STRT;

    // Wait for the operation to complete
    while (FLASH->SR & FLASH_SR_BSY);
}

static void flash_program(void *dest, void *src, size_t bytes)
{
    volatile u32 *dest_ptr = (u32 *)dest;
    u32 *src_ptr = (u32 *)src;

    // Wait if a flash memory operation is in progress
    while (FLASH->SR & FLASH_SR_BSY);

    // Activate flash programming
    MODIFY_REG(FLASH->CR, FLASH_CR_SER, FLASH_CR_PG);

	for (size_t i=0; i<bytes; i+=4)
    {
		*dest_ptr++ = *src_ptr++;
		while (FLASH->SR & FLASH_SR_BSY);
	}

    // Deactivate flash programming
    FLASH->CR &= ~FLASH_CR_PG;
}

static void flash_program_byte(u8 *dest, u8 byte)
{
    flash_unlock();
    // Wait if a flash memory operation is in progress
    while (FLASH->SR & FLASH_SR_BSY);

    // Activate flash programming and set parallelism to x8
    MODIFY_REG(FLASH->CR, FLASH_CR_PSIZE|FLASH_CR_MER|FLASH_CR_SER, FLASH_CR_PG);

    *dest = byte;
    while (FLASH->SR & FLASH_SR_BSY);

    // Deactivate flash programming
    FLASH->CR &= ~FLASH_CR_PG;
    flash_lock();
}

static void flash_sector_program(s8 sector, void *dest, void *src, size_t bytes)
{
    // Prevent anything executing from flash
    __disable_irq();

    flash_unlock();
    if (sector >= 0 && sector <= 11)
    {
        flash_sector_erase(sector);
    }
    flash_program(dest, src, bytes);
    flash_lock();

    __enable_irq();
}
