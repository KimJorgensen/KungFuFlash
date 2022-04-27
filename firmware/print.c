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
 * Based on kprint.c
 * Author: Aurelio Colosimo, 2016
 *
 * This file is part of kim-os project: https://github.com/colosimo/kim-os
 * According to kim-os license, you can do whatever you want with it,
 * as long as you retain this notice.
 */
#include <stdarg.h>

static const char hex[] = "0123456789ABCDEF";

static void printhex(int x, int ndigits, void (*_putchar)(char))
{
    unsigned char *p;
    int i;
    char c;
    int started = 0;

    p = &((unsigned char *)&x)[3];

    for (i = 0; i < sizeof(x); i++)
    {
        if (*p != 0 || (ndigits && i >= sizeof(x) - ndigits / 2) ||
            i == sizeof(x) - 1)
        {
            started = 1;
        }

        if (started)
        {
            c = hex[*p >> 4];
            _putchar(c);
            c = hex[*p & 0xf];
            _putchar(c);
        }

        p--;
    }
}

static void printint(int _x, int sgnd, int ndigits, void (*_putchar)(char))
{
    char buf[20];
    unsigned int x;
    int i = sizeof(buf) - 1;
    int d = 0;

    if (!_x && !ndigits)
    {
        buf[i--] = '0';
    }

    if (sgnd)
    {
        x = (_x < 0) ? -_x : _x;
    }
    else
    {
        x = _x;
    }

    while ((x || d < ndigits) && i > 1)
    {
        buf[i--] = '0' + x % 10;
        x /= 10;
        d++;
    }

    if (sgnd && _x < 0)
    {
        buf[i] = '-';
    }
    else
    {
        i++;
    }

    for (; i < sizeof(buf); i++)
    {
        _putchar(buf[i]);
    }
}

static void vkprint(const char *fmt, va_list args, void (*_putchar)(char))
{
    char *s;
    int ndigits = 0;
    int perc = 0;

    for (; *fmt; fmt++)
    {
        if (*fmt == '%' && !perc)
        {
            perc = 1;
            continue;
        }

        if (!perc)
        {
            _putchar(*fmt);
            continue;
        }

        switch (*fmt)
        {
        case '%':
            _putchar('%');
            break;
        case 'c':
            _putchar(va_arg(args, int));
            break;
        case 's':
            s = va_arg(args, char *);

            ndigits -= strlen(s);

            while (*s)
            {
                _putchar(*s++);
            }

            while (ndigits-- > 0)
            {
                _putchar(' ');
            }

            ndigits = 0;
            break;

        case 'p':
            _putchar('0');
            _putchar('x');
        case 'x':
        case 'X':
            printhex(va_arg(args, int), ndigits, _putchar);
            ndigits = 0;
            break;

        case 'd':
            printint(va_arg(args, unsigned int), 1, ndigits, _putchar);
            ndigits = 0;
            break;

        case 'u':
            printint(va_arg(args, unsigned int), 0, ndigits, _putchar);
            ndigits = 0;
            break;

        default:
            if (isdigit(*fmt))
            {
                ndigits = ndigits * 10 + *fmt - '0';
            }
            break;
        }

        if (!isdigit(*fmt))
        {
            perc = 0;
        }
    }
}

static char *buf_putchar_buf;
static int buf_putchar_buflen;

static void buf_putchar(char c)
{
    buf_putchar_buf[buf_putchar_buflen++] = c;
}

/* Minimal printf functions. Supports strings, chars and hex numbers. */
static void print(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vkprint(fmt, args, put_char);
	va_end(args);
}

static void print_log(const char *level, const char *fmt, ...)
{
	va_list args;
    while (*level)
    {
        put_char(*level++);
    }

    va_start(args, fmt);
	vkprint(fmt, args, put_char);
	va_end(args);
    put_char('\n');
}

static void sprint(char *buf, const char *fmt, ...)
{
	va_list args;
	buf_putchar_buf = buf;
	buf_putchar_buflen = 0;

	va_start(args, fmt);
	vkprint(fmt, args, buf_putchar);
	va_end(args);
	buf_putchar('\0');
}
