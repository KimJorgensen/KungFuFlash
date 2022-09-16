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

#define CRT_PORT_NONE       (C64_EXROM_HIGH|C64_GAME_HIGH)  // No cartridge
#define CRT_PORT_8K         (C64_EXROM_LOW|C64_GAME_HIGH)   // 8k cartridge
#define CRT_PORT_16K        (C64_EXROM_LOW|C64_GAME_LOW)    // 16k cartridge
#define CRT_PORT_ULTIMAX    (C64_EXROM_HIGH|C64_GAME_LOW)   // Ultimax cartridge

#define CRT_FLASH_BANK(bank)    ((u8 *)FLASH_BASE + (u32)(16*1024 * bank))
#define CRT_LAUNCHER_BANK       CRT_FLASH_BANK(3)

#define CRT_DAT_BANK(bank)      (dat_buffer + (u32)(16*1024 * bank))

#define CRT_RAM_BUF             (crt_ram_buf)
#define CRT_RAM_BANK(bank)      (CRT_RAM_BUF + (u32)(8*1024 * bank))

// Fast look-up of 8k RAM bank address
static u8 * const crt_ram_banks[8] =
{
    CRT_RAM_BANK(0),
    CRT_RAM_BANK(1),
    CRT_RAM_BANK(2),
    CRT_RAM_BANK(3),
    CRT_RAM_BANK(0), // Mirror of first 32k
    CRT_RAM_BANK(1),
    CRT_RAM_BANK(2),
    CRT_RAM_BANK(3)
};

// Fast look-up of 16k ROM bank address
static u8 * const crt_banks[64] =
{
    CRT_DAT_BANK(0),
    CRT_DAT_BANK(1),
    CRT_DAT_BANK(2),
    CRT_DAT_BANK(3),
    CRT_FLASH_BANK(4),
    CRT_FLASH_BANK(5),
    CRT_FLASH_BANK(6),
    CRT_FLASH_BANK(7),
    CRT_FLASH_BANK(8),
    CRT_FLASH_BANK(9),
    CRT_FLASH_BANK(10),
    CRT_FLASH_BANK(11),
    CRT_FLASH_BANK(12),
    CRT_FLASH_BANK(13),
    CRT_FLASH_BANK(14),
    CRT_FLASH_BANK(15),
    CRT_FLASH_BANK(16),
    CRT_FLASH_BANK(17),
    CRT_FLASH_BANK(18),
    CRT_FLASH_BANK(19),
    CRT_FLASH_BANK(20),
    CRT_FLASH_BANK(21),
    CRT_FLASH_BANK(22),
    CRT_FLASH_BANK(23),
    CRT_FLASH_BANK(24),
    CRT_FLASH_BANK(25),
    CRT_FLASH_BANK(26),
    CRT_FLASH_BANK(27),
    CRT_FLASH_BANK(28),
    CRT_FLASH_BANK(29),
    CRT_FLASH_BANK(30),
    CRT_FLASH_BANK(31),
    CRT_FLASH_BANK(32),
    CRT_FLASH_BANK(33),
    CRT_FLASH_BANK(34),
    CRT_FLASH_BANK(35),
    CRT_FLASH_BANK(36),
    CRT_FLASH_BANK(37),
    CRT_FLASH_BANK(38),
    CRT_FLASH_BANK(39),
    CRT_FLASH_BANK(40),
    CRT_FLASH_BANK(41),
    CRT_FLASH_BANK(42),
    CRT_FLASH_BANK(43),
    CRT_FLASH_BANK(44),
    CRT_FLASH_BANK(45),
    CRT_FLASH_BANK(46),
    CRT_FLASH_BANK(47),
    CRT_FLASH_BANK(48),
    CRT_FLASH_BANK(49),
    CRT_FLASH_BANK(50),
    CRT_FLASH_BANK(51),
    CRT_FLASH_BANK(52),
    CRT_FLASH_BANK(53),
    CRT_FLASH_BANK(54),
    CRT_FLASH_BANK(55),
    CRT_FLASH_BANK(56),
    CRT_FLASH_BANK(57),
    CRT_FLASH_BANK(58),
    CRT_FLASH_BANK(59),
    CRT_FLASH_BANK(60),
    CRT_FLASH_BANK(61),
    CRT_FLASH_BANK(62),
    CRT_FLASH_BANK(63)
};

#define SPECIAL_RELEASED    0
#define SPECIAL_PRESSED     1

#define FREEZE_RESET    0
#define FREEZE_START    1
#define FREEZE_3_WRITES 4   // 3 Consecutive writes after FREEZE_START
