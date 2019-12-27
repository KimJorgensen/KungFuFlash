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

static const char hex[] = "0123456789abcdef";

static void printhex(int x, int ndigits, void (*_putchar)(char))
{
	unsigned char *p;
	int i;
	int c;
	int started = 0;

	p = &((unsigned char *)&x)[3];

	for (i = 0; i < sizeof(x); i++) {

		if (*p != 0 || (ndigits && i >= sizeof(x) - ndigits / 2) ||
		    i == sizeof(x) - 1) {
			started = 1;
		}

		if (started) {
			c = hex[*p >> 4];
			_putchar(c);
			c = hex[*p & 0xf];
			_putchar(c);
		}

		p--;
	}
}

static void printint(int x, int sgnd, void (*_putchar)(char))
{
	int div = 1000000000;
	int started = 0;
	if (x == 0) {
		_putchar('0');
		return;
	}
	if (sgnd && x < 0) {
		_putchar('-');
		x = -x;
	}
	while (div > 0) {
		if (x / div || started) {
			started = 1;
			_putchar('0' + (x / div));
		}
		x = x % div;
		div /= 10;
	}
}

/* Minimal printf function. Supports strings, chars and hex numbers. */
static void print(const char *fmt, ...)
{
	va_list args;
	char *s;
	va_start(args, fmt);
	int ndigits = 0;
	int fmt_prefix = 0;

	for (; *fmt; fmt++) {

		if (*fmt == '%' && !fmt_prefix) {
			fmt_prefix = 1;
			continue;
		}

		if (!fmt_prefix) {
			put_char(*fmt);
			continue;
		}

		fmt_prefix = 0;

		switch (*fmt) {
		case '%':
			put_char('%');
			break;
		case 'c':
			put_char(va_arg(args, int));
			break;
		case 's':
			while (ndigits--)
				put_char(' ');
			s = va_arg(args, char *);
			while (*s)
				put_char(*s++);
			ndigits = 0;
			break;

		case 'p':
			put_char('0');
			put_char('x');
			printhex(va_arg(args, int), ndigits, put_char);
			break;
		case 'u':
			printint(va_arg(args, unsigned int), 0, put_char);
			break;
		case 'd':
			printint(va_arg(args, int), 1, put_char);
			break;
		case 'x':
			printhex(va_arg(args, int), ndigits, put_char);
			ndigits = 0;
			break;

		default:
			if (isdigit(*fmt)) {
				fmt_prefix = 1;
				ndigits = ndigits * 10 + *fmt - '0';
			}
			break;
		}
	}

	va_end(args);
}

static char *tmpbuf;
static int cnt;

static void buf_putchar(char c)
{
	tmpbuf[cnt++] = c;
}

/* Minimal sprintf function. Supports strings, chars and hex numbers. */
static void sprint(char *buf, const char *fmt, ...)
{
	va_list args;
	char *s;
	va_start(args, fmt);
	int ndigits = 0;
	int fmt_prefix = 0;

	cnt = 0;
	tmpbuf = buf;

	for (; *fmt; fmt++) {

		if (*fmt == '%' && !fmt_prefix) {
			fmt_prefix = 1;
			continue;
		}

		if (!fmt_prefix) {
			buf_putchar(*fmt);
			continue;
		}

		fmt_prefix = 0;

		switch (*fmt) {
		case '%':
			buf_putchar('%');
			break;
		case 'c':
			buf_putchar(va_arg(args, int));
			break;
		case 's':
			while (ndigits--)
				buf_putchar(' ');
			s = va_arg(args, char *);
			while (*s)
				buf_putchar(*s++);
			ndigits = 0;
			break;

		case 'p':
			buf_putchar('0');
			buf_putchar('x');
			printhex(va_arg(args, int), ndigits, buf_putchar);
			break;
		case 'u':
			printint(va_arg(args, unsigned int), 0, buf_putchar);
			break;
		case 'd':
			printint(va_arg(args, int), 1, buf_putchar);
			break;
		case 'x':
		case 'X':
			printhex(va_arg(args, int), ndigits, buf_putchar);
			ndigits = 0;
			break;

		default:
			if (isdigit(*fmt)) {
				fmt_prefix = 1;
				ndigits = ndigits * 10 + *fmt - '0';
			}
			break;
		}
	}

	va_end(args);
	buf_putchar('\0');
}
