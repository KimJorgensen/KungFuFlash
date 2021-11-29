/*
 * Copyright (c) 2019-2021 Kim Jørgensen
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

#define MENU_RAM_SIGNATURE  "KungFu:Menu"
#define MEMU_SIGNATURE_BUF  ((uint32_t *)scratch_buf)

#define FW_NAME         ("Kung Fu Flash v" VERSION)
#define FW_NAME_SIZE    20

#define MODULE_HEADER                                           \
struct                                                          \
{                                                               \
    char fw_name[FW_NAME_SIZE];                                 \
    void (*init)(void);                                         \
    bool (*crt_is_supported)(uint32_t cartridge_type);          \
    void (*crt_install_handler)(DAT_CRT_HEADER *crt_header);    \
} __attribute__((packed))

// 32kB scratch buffer
__attribute__((__section__(".uninit"))) static char scratch_buf[32*1024];

// 64kB data buffer
__attribute__((__section__(".sram"))) uint8_t dat_buffer[64*1024];
