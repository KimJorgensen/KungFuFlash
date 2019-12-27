/*
Based on Universal "stm32.h" CMSIS include
https://github.com/dmitrystu/stm32h

MIT License

Copyright (c) 2017 Dmitry Filimonchuk

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef _STM32_H_
#define _STM32_H_

/* modify bitfield */
#define _BMD(reg, msk, val)     (reg) = (((reg) & ~(msk)) | (val))
/* set bitfield */
#define _BST(reg, bits)         (reg) = ((reg) | (bits))
/* clear bitfield */
#define _BCL(reg, bits)         (reg) = ((reg) & ~(bits))
/* wait until bitfield set */
#define _WBS(reg, bits)         while(((reg) & (bits)) == 0)
/* wait until bitfield clear */
#define _WBC(reg, bits)         while(((reg) & (bits)) != 0)
/* wait for bitfield value */
#define _WVL(reg, msk, val)     while(((reg) & (msk)) != (val))
/* bit value */
#define _BV(bit)                (0x01 << (bit))

#include "stm32f4xx.h"

#endif // _STM32_H_
