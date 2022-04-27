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
 *
 ******************************************************************************
 * Based on log.h
 * Author: Aurelio Colosimo, 2016
 *
 * This file is part of kim-os project: https://github.com/colosimo/kim-os
 * According to kim-os license, you can do whatever you want with it,
 * as long as you retain this notice.
 */

#define V_NON 0 /* No log */
#define V_CRT 1 /* Log critical */
#define V_ERR 2 /* Log errors */
#define V_WRN 3 /* Log warnings */
#define V_LOG 4 /* Log normal behaviour */
#define V_DBG 5 /* Log debug */

#ifndef VERB
#define VERB V_NON
#endif

#if (VERB >= V_CRT)
    #define crt(...) print_log("[CRT] ", __VA_ARGS__)
#else
    #define crt(...)
#endif

#if (VERB >= V_ERR)
    #define err(...) print_log("[ERR] ", __VA_ARGS__)
#else
    #define err(...)
#endif

#if (VERB >= V_WRN)
    #define wrn(...) print_log("[WRN] ", __VA_ARGS__)
#else
    #define wrn(...)
#endif

#if (VERB >= V_LOG)
    #define log(...) print_log("[LOG] ", __VA_ARGS__)
#else
    #define log(...)
#endif

#if (VERB >= V_DBG)
    #define dbg(...) print_log("[DBG] ", __VA_ARGS__)
#else
    #define dbg(...)
#endif

#define put_char	usb_putc
#define isdigit(c)	((c) >= '0' && (c) <= '9')

static void print(const char *fmt, ...);
static void print_log(const char *level, const char *fmt, ...);
static void sprint(char *buf, const char *fmt, ...);
