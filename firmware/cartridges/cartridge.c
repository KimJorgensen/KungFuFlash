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

#include "cartridge.h"

// Current ROM or RAM bank pointer
static uint8_t *crt_ptr;

#ifdef MODULE
// Current ROM bank pointer (only used by some cartridges)
static uint8_t *crt_rom_ptr;

static uint32_t special_button;
static uint32_t freezer_state;
#endif

/* ordered by cartridge id */
#include "crt_normal.c"
#ifdef MODULE
#include "action_replay_4x.c"
#include "kcs_power_cartridge.c"
#include "final_cartridge_3.c"
#include "simons_basic.c"
#include "super_games.c"
#include "epyx_fastload.c"
#include "c64gs_system_3.c"
#include "warpspeed.c"
#include "dinamic.c"
#include "zaxxon.c"
#include "magic_desk.c"
#include "super_snapshot_5.c"
#include "comal80.c"
#else
#include "easyflash.c"
#include "easyflash_3.c"
#endif
#ifdef MODULE
#include "prophet64.c"
#include "freeze_machine.c"
#include "rgcd.c"
#include "c128_normal.c"
#endif

#define NTSC_OR_PAL_HANDLER(name)   \
    ntsc ? name##_ntsc_handler : name##_pal_handler

static void (*crt_get_handler(uint32_t cartridge_type, bool vic_support)) (void)
{
    bool ntsc = c64_is_ntsc();
    switch (cartridge_type)
    {
#ifndef MODULE
        case CRT_NORMAL_CARTRIDGE:
            return NTSC_OR_PAL_HANDLER(crt);
#else
        case CRT_ACTION_REPLAY:
            return NTSC_OR_PAL_HANDLER(ar4x);

        case CRT_KCS_POWER_CARTRIDGE:
            return NTSC_OR_PAL_HANDLER(kcs);

        case CRT_FINAL_CARTRIDGE_III:
            return NTSC_OR_PAL_HANDLER(fc3);

        case CRT_SIMONS_BASIC:
            return simons_basic_handler;

        case CRT_SUPER_GAMES:
            return super_games_handler;

        case CRT_EPYX_FASTLOAD:
            return epyx_handler;

        case CRT_C64_GAME_SYSTEM_SYSTEM_3:
            return c64gs_handler;

        case CRT_WARP_SPEED:
            return warpspeed_handler;

        case CRT_DINAMIC:
            return dinamic_handler;

        case CRT_ZAXXON_SUPER_ZAXXON:
            return zaxxon_handler;

        case CRT_FUN_PLAY_POWER_PLAY:
        case CRT_MAGIC_DESK_DOMARK_HES_AUSTRALIA:
            return magic_desk_handler;

        case CRT_SUPER_SNAPSHOT_V5:
            return NTSC_OR_PAL_HANDLER(ss5);

        case CRT_COMAL_80:
            return comal80_handler;
#endif
#ifndef MODULE
        case CRT_OCEAN_TYPE_1:
        case CRT_EASYFLASH:
            if (cartridge_type != CRT_EASYFLASH || vic_support)
            {
                return NTSC_OR_PAL_HANDLER(ef);
            }
            return ef3_handler;
#else
        case CRT_PROPHET64:
        case CRT_DREAN:
            return prophet64_handler;

        case CRT_FREEZE_FRAME:
        case CRT_FREEZE_MACHINE:
            return NTSC_OR_PAL_HANDLER(fm);

        case CRT_RGCD:
            return rgcd_handler;

        case CRT_C128_NORMAL_CARTRIDGE:
            return NTSC_OR_PAL_HANDLER(c128);
#endif
    }

    return NULL;
}

static void crt_init(DAT_CRT_HEADER *crt_header)
{
    switch (crt_header->type)
    {
#ifdef MODULE
        case CRT_ACTION_REPLAY:
            ar4x_init();
            break;

        case CRT_KCS_POWER_CARTRIDGE:
            kcs_init();
            break;

        case CRT_FINAL_CARTRIDGE_III:
            fc3_init();
            break;

        case CRT_EPYX_FASTLOAD:
            epyx_init();
            break;

        case CRT_WARP_SPEED:
            warpspeed_init();
            break;

        case CRT_ZAXXON_SUPER_ZAXXON:
            zaxxon_init();
            break;

        case CRT_FUN_PLAY_POWER_PLAY:
        case CRT_MAGIC_DESK_DOMARK_HES_AUSTRALIA:
            magic_desk_init();
            break;

        case CRT_SUPER_SNAPSHOT_V5:
            ss5_init();
            break;

        case CRT_COMAL_80:
            comal80_init();
            break;
#else
        case CRT_EASYFLASH:
            ef_init();
            break;
#endif
#ifdef MODULE
        case CRT_FREEZE_FRAME:
        case CRT_FREEZE_MACHINE:
            fm_init(crt_header);
            break;

        case CRT_RGCD:
            rgcd_init(crt_header);
            break;
#endif
    }
}

static void crt_install_handler_(DAT_CRT_HEADER *crt_header)
{
    crt_ptr = crt_banks[0];
    crt_init(crt_header);

    uint32_t cartridge_type = crt_header->type;
    bool vic_support = (crt_header->flags & CRT_FLAG_VIC) != 0;
    void (*handler)(void) = crt_get_handler(cartridge_type, vic_support);
    C64_INSTALL_HANDLER(handler);
}

static bool crt_is_supported_(uint32_t cartridge_type)
{
    return crt_get_handler(cartridge_type, false) != NULL;
}
