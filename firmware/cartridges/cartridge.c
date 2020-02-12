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
#include "cartridge.h"
#include "crt_normal.c"
#include "action_replay_4x.c"
#include "kcs_power_cartridge.c"
#include "final_cartridge_3.c"
#include "simons_basic.c"
#include "epyx_fastload.c"
#include "c64gs_system_3.c"
#include "dinamic.c"
#include "zaxxon.c"
#include "magic_desk.c"
#include "super_snapshot_5.c"
#include "easyflash.c"
#include "comal80.c"

static void (*crt_get_handler(uint16_t cartridge_type)) (void)
{
    switch (cartridge_type)
    {
        case CRT_NORMAL_CARTRIDGE:
            return crt_handler;

        case CRT_ACTION_REPLAY:
            return ar4x_handler;

        case CRT_KCS_POWER_CARTRIDGE:
            return kcs_handler;

        case CRT_FINAL_CARTRIDGE_III:
            return fc3_handler;

        case CRT_SIMONS_BASIC:
            return simons_basic_handler;

        case CRT_EPYX_FASTLOAD:
            return epyx_handler;

        case CRT_C64_GAME_SYSTEM_SYSTEM_3:
            return c64gs_handler;

        case CRT_DINAMIC:
            return dinamic_handler;

        case CRT_ZAXXON_SUPER_ZAXXON:
            return zaxxon_handler;

        case CRT_FUN_PLAY_POWER_PLAY:
        case CRT_MAGIC_DESK_DOMARK_HES_AUSTRALIA:
            return magic_desk_handler;

        case CRT_SUPER_SNAPSHOT_V5:
            return ss5_handler;

        case CRT_COMAL_80:
            return comal80_handler;

        case CRT_OCEAN_TYPE_1:
        case CRT_EASYFLASH:
            return ef_handler;
    }

    return NULL;
}

static void (*crt_get_init(uint16_t cartridge_type)) (void)
{
    switch (cartridge_type)
    {
        case CRT_ACTION_REPLAY:
            return ar4x_init;

        case CRT_KCS_POWER_CARTRIDGE:
            return kcs_init;

        case CRT_FINAL_CARTRIDGE_III:
            return fc3_init;

        case CRT_EPYX_FASTLOAD:
            return epyx_init;

        case CRT_ZAXXON_SUPER_ZAXXON:
            return zaxxon_init;

        case CRT_FUN_PLAY_POWER_PLAY:
        case CRT_MAGIC_DESK_DOMARK_HES_AUSTRALIA:
            return magic_desk_init;

        case CRT_SUPER_SNAPSHOT_V5:
            return ss5_init;

        case CRT_COMAL_80:
            return comal80_init;

        case CRT_EASYFLASH:
            return ef_init;
    }

    return NULL;
}

static void crt_install_handler(uint16_t cartridge_type)
{
    void (*init)(void) = crt_get_init(cartridge_type);
    if (init)
    {
        init();
    }

    void (*handler)(void) = crt_get_handler(cartridge_type);
    C64_INSTALL_HANDLER(handler);
}

static bool crt_is_supported(uint16_t cartridge_type)
{
    return crt_get_handler(cartridge_type) != NULL;
}
