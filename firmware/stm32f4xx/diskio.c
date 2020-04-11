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
 *
 ******************************************************************************
 * Based on https://github.com/colosimo/fatfs-stm32
 * 2017, Aurelio Colosimo <aurelio@aureliocolosimo.it>
 * MIT License
 ******************************************************************************
 * and a few bits from https://github.com/rogerclarkmelbourne/Arduino_STM32
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *****************************************************************************/

#include "ff.h"
#include "diskio.h"

#define BIT30 (1 << 30)
#define BIT31 ((uint32_t)(1 << 31))

#define RESP_NONE  0
#define RESP_SHORT 1
#define RESP_LONG  2

#define CT_MMC   0x01    /* MMC ver 3 */
#define CT_SD1   0x02    /* SD ver 1 */
#define CT_SD2   0x04    /* SD ver 2 */
#define CT_SDC   (CT_SD1|CT_SD2) /* SD */
#define CT_BLOCK 0x08    /* Block addressing */

#define ACMD(x)     ((x) | 0x80)
#define IS_ACMD(x)  ((x) & 0x80)

#define SDIO_ICR_CMD_FLAGS (SDIO_ICR_CEATAENDC | SDIO_ICR_SDIOITC | \
                            SDIO_ICR_CMDSENTC | SDIO_ICR_CMDRENDC | \
                            SDIO_ICR_CTIMEOUTC | SDIO_ICR_CCRCFAILC)

#define SDIO_ICR_DATA_FLAGS (SDIO_ICR_DBCKENDC | SDIO_ICR_STBITERRC |   \
                             SDIO_ICR_DATAENDC | SDIO_ICR_RXOVERRC |    \
                             SDIO_ICR_TXUNDERRC | SDIO_ICR_DTIMEOUTC |  \
                             SDIO_ICR_DCRCFAILC)

static uint16_t card_rca;
static uint8_t card_type;
static uint8_t card_info[36]; /* CSD, CID, OCR */

static DSTATUS dstatus = STA_NOINIT;


static void sdio_init(void)
{
    // Pull-up on SDIO_D0-D3 & SDIO_CMD
    MODIFY_REG(GPIOC->PUPDR,
    GPIO_PUPDR_PUPD8|GPIO_PUPDR_PUPD9|GPIO_PUPDR_PUPD10|
    GPIO_PUPDR_PUPD11|GPIO_PUPDR_PUPD12,
    GPIO_PUPDR_PUPD8_0|GPIO_PUPDR_PUPD9_0|GPIO_PUPDR_PUPD10_0|GPIO_PUPDR_PUPD11_0);
    MODIFY_REG(GPIOD->PUPDR, GPIO_PUPDR_PUPD2, GPIO_PUPDR_PUPD2_0);

    // Set output speed to high speed
    MODIFY_REG(GPIOC->OSPEEDR,
    GPIO_OSPEEDR_OSPEED8|GPIO_OSPEEDR_OSPEED9|GPIO_OSPEEDR_OSPEED10|
    GPIO_OSPEEDR_OSPEED11|GPIO_OSPEEDR_OSPEED12,
    GPIO_OSPEEDR_OSPEED8_1|GPIO_OSPEEDR_OSPEED9_1|GPIO_OSPEEDR_OSPEED10_1|
    GPIO_OSPEEDR_OSPEED11_1|GPIO_OSPEEDR_OSPEED12_1);
    MODIFY_REG(GPIOD->OSPEEDR, GPIO_OSPEEDR_OSPEED2, GPIO_OSPEEDR_OSPEED2_1);

    // Set PC8-PC12 and PD2 to alternate function 12 (SDIO_D0-D3, SDIO_CK & SDIO_CMD)
    MODIFY_REG(GPIOC->AFR[1],
    GPIO_AFRH_AFSEL8|GPIO_AFRH_AFSEL9|GPIO_AFRH_AFSEL10|
    GPIO_AFRH_AFSEL11|GPIO_AFRH_AFSEL12,
    GPIO_AFRH_AFSEL8_2|GPIO_AFRH_AFSEL8_3|GPIO_AFRH_AFSEL9_2|GPIO_AFRH_AFSEL9_3|
    GPIO_AFRH_AFSEL10_2|GPIO_AFRH_AFSEL10_3|GPIO_AFRH_AFSEL11_2|GPIO_AFRH_AFSEL11_3|
    GPIO_AFRH_AFSEL12_2|GPIO_AFRH_AFSEL12_3);
    MODIFY_REG(GPIOC->MODER,
    GPIO_MODER_MODER8|GPIO_MODER_MODER9|GPIO_MODER_MODER10|
    GPIO_MODER_MODER11|GPIO_MODER_MODER12,
    GPIO_MODER_MODER8_1|GPIO_MODER_MODER9_1|GPIO_MODER_MODER10_1|
    GPIO_MODER_MODER11_1|GPIO_MODER_MODER12_1);

    MODIFY_REG(GPIOD->AFR[0], GPIO_AFRL_AFSEL2, GPIO_AFRL_AFSEL2_2|GPIO_AFRL_AFSEL2_3);
    MODIFY_REG(GPIOD->MODER, GPIO_MODER_MODER2, GPIO_MODER_MODER2_1);

    // Enable SDIO and reset
    RCC->APB2ENR |= RCC_APB2ENR_SDIOEN;
    __DSB();
    RCC->APB2RSTR |= RCC_APB2RSTR_SDIORST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_SDIORST;

    SDIO->POWER |= SDIO_POWER_PWRCTRL;
    // After a data write, data cannot be written to this register for three SDIOCLK clock periods
    // plus two PCLK2 clock periods.
    delay_us(10);
    SDIO->CLKCR = SDIO_CLKCR_CLKEN | ((48000 / 400) - 2); /* clk set to 400kHz */
    delay_us(200);  // Wait for 80 cycles at 400kHz
}

static void sdio_deinit(void)
{
    SDIO->CLKCR &= ~SDIO_CLKCR_CLKEN;
    while (SDIO->STA & SDIO_STA_CMDACT);

    SDIO->POWER &= ~SDIO_POWER_PWRCTRL;
    // After a data write, data cannot be written to this register for three SDIOCLK clock periods
    // plus two PCLK2 clock periods.
    delay_us(10);

    RCC->APB2ENR &= ~RCC_APB2ENR_SDIOEN;
    __DSB();

    // Set PC8-PC12 and PD2 as input
    MODIFY_REG(GPIOC->MODER,
    GPIO_MODER_MODER8|GPIO_MODER_MODER9|GPIO_MODER_MODER10|
    GPIO_MODER_MODER11|GPIO_MODER_MODER12, 0);
    MODIFY_REG(GPIOC->AFR[1],
    GPIO_AFRH_AFSEL8|GPIO_AFRH_AFSEL9|GPIO_AFRH_AFSEL10|
    GPIO_AFRH_AFSEL11|GPIO_AFRH_AFSEL12, 0);

    MODIFY_REG(GPIOD->MODER, GPIO_MODER_MODER2, 0);
    MODIFY_REG(GPIOD->AFR[0], GPIO_AFRL_AFSEL2, 0);

    // No pull-up, pull-down on PC8-PC12 and PD2
    MODIFY_REG(GPIOC->PUPDR,
    GPIO_PUPDR_PUPD8|GPIO_PUPDR_PUPD9|GPIO_PUPDR_PUPD10|
    GPIO_PUPDR_PUPD11|GPIO_PUPDR_PUPD12, 0);
    MODIFY_REG(GPIOD->PUPDR, GPIO_PUPDR_PUPD2, 0);
}

static void byte_swap(uint8_t *dest, uint32_t src)
{
    int i;
    for (i = 0; i < 4; i ++)
        dest[i] = src >> (24 - 8 * i);
}

static bool sdio_cmd_send(uint16_t idx, uint32_t arg, int resp_type, uint32_t *buf)
{
    if (IS_ACMD(idx)) // ACMD class
    {
        if (!sdio_cmd_send(55, ((uint32_t)card_rca) << 16, RESP_SHORT, buf) ||
            !(buf[0] & 0x00000020))
        {
            return false;
        }
    }

    idx &= SDIO_CMD_CMDINDEX;

    uint32_t cmd = SDIO_CMD_CPSMEN | idx;
    if (resp_type == RESP_SHORT)
    {
        cmd |= SDIO_CMD_WAITRESP_0;
    }
    else if (resp_type == RESP_LONG)
    {
        cmd |= SDIO_CMD_WAITRESP_0|SDIO_CMD_WAITRESP_1;
    }

    SDIO->ICR = SDIO_ICR_CMD_FLAGS;
    SDIO->ARG = arg;
    SDIO->CMD = cmd;

    // Wait for command transfer to finish
    while (SDIO->STA & SDIO_STA_CMDACT);

    if (resp_type == RESP_NONE)
    {
        return SDIO->STA & SDIO_STA_CMDSENT ? true : false;
    }
    else
    {
        uint32_t sta;
        // Wait for response
        do
        {
            sta = SDIO->STA;
        }
        while (!(sta & (SDIO_STA_CMDREND|SDIO_STA_CTIMEOUT|SDIO_STA_CCRCFAIL)));

        // Check if timeout
        if (sta & SDIO_STA_CTIMEOUT)
        {
            // Don't get spammed if no card is inserted
            if (idx != 1 && idx != 41)
            {
                err("%s timeout idx=%d arg=%08x\n", __func__, idx, arg);
            }
            return false;
        }

        // Check if crc error
        if (sta & SDIO_STA_CCRCFAIL)
        {
            // Ignore CRC error for these commands
            if (idx != 1 && idx != 12 && idx != 41)
            {
                err("%s crcfail idx=%d arg=%08x\n", __func__, idx, arg);
                return false;
            }
        }
    }

    buf[0] = SDIO->RESP1;
    if (resp_type == RESP_LONG)
    {
        buf[1] = SDIO->RESP2;
        buf[2] = SDIO->RESP3;
        buf[3] = SDIO->RESP4;
    }

    return true;
}

static bool sdio_check_tran(uint32_t tout_ms)
{
    uint32_t resp;

    timer_start_ms(tout_ms);
    while (!timer_elapsed())
    {
        if (sdio_cmd_send(13, ((uint32_t)card_rca) << 16, RESP_SHORT, &resp)
            && ((resp & 0x01e00) == 0x00800))
        {
            return true;
        }
    }

    return false;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    uint32_t resp[4];
    uint16_t cmd;
    uint8_t timeouts;
    int i;

    card_rca = 0;
    dstatus = STA_NOINIT;
    sdio_init();

    // Note: No support for card detect (SDIO_CD)

    if(!sdio_cmd_send(0, 0, RESP_NONE, NULL))
    {
        err("could not reset card\n");
        goto fail;
    }

    timeouts = 4;
    timer_start_ms(250);

    if (sdio_cmd_send(8, 0x1AA, RESP_SHORT, resp) && ((resp[0] & 0xfff) == 0x1aa))
    {
        // sdc v2
        card_type = 0;
        do
        {
            if (sdio_cmd_send(ACMD(41), 0x40ff8000, RESP_SHORT, resp) &&
                (resp[0] & BIT31))
            {
                card_type = (resp[0] & BIT30) ? CT_SD2 | CT_BLOCK : CT_SD2;
                log("card type: SD2\n");
                break;
            }
        }
        while (!timer_elapsed() || --timeouts);

        if (!card_type)
        {
            err("could not read card type\n");
            goto fail;
        }
    }
    else
    {
        // sdc v1 or mmc
        if (sdio_cmd_send(ACMD(41), 0x00ff8000, RESP_SHORT, resp))
        {
            // ACMD41 is accepted -> sdc v1
            card_type = CT_SD1;
            cmd = ACMD(41);
        }
        else
        {
            // ACMD41 is rejected -> mmc
            card_type = CT_MMC;
            cmd = 1;
        }

        while (true)
        {
            if (sdio_cmd_send(cmd, 0x00FF8000, RESP_SHORT, resp) &&
                (resp[0] & BIT31))
            {
                break;
            }

            if (timer_elapsed() && --timeouts)
            {
                err("cmd %d failed\n", cmd);
                goto fail;
            }
        }
    }

    byte_swap(&card_info[32], resp[0]);
    log("card OCR: %08x\n", ((uint32_t*)card_info)[8]);

    // card state 'ready'
    if (!sdio_cmd_send(2, 0, RESP_LONG, resp)) // enter ident state
    {
        goto fail;
    }

    for (i = 0; i < 4; i++)
    {
        byte_swap(&card_info[16 + i * 4], resp[i]);
    }

    // card state 'ident'
    if (card_type & CT_SDC)
    {
        if (!sdio_cmd_send(3, 0, RESP_SHORT, resp))
        {
            goto fail;
        }

        card_rca = (uint16_t)(resp[0] >> 16);
    }
    else
    {
        if (!sdio_cmd_send(3, 1 << 16, RESP_SHORT, resp))
        {
            goto fail;
        }

        card_rca = 1;
    }

    // card state 'standby'
    if (!sdio_cmd_send(9, ((uint32_t)card_rca) << 16, RESP_LONG, resp))
    {
        goto fail;
    }

    for (i = 0; i < 4; i++)
    {
        byte_swap(&card_info[i * 4], resp[i]);
    }

    if (!sdio_cmd_send(7, ((uint32_t)card_rca) << 16, RESP_SHORT, resp))
    {
        goto fail;
    }

    // card state 'tran'
    if (!(card_type & CT_BLOCK))
    {
        if (!sdio_cmd_send(16, 512, RESP_SHORT, resp) || (resp[0] & 0xfdf90000))
        {
            goto fail;
        }
    }

    uint32_t clkcr = SDIO->CLKCR;
    if (card_type & CT_SDC)
    {
        // Set wide bus
        if (!sdio_cmd_send(ACMD(6), 2, RESP_SHORT, resp) || (resp[0] & 0xfdf90000))
        {
            goto fail;
        }

        clkcr = (clkcr & ~SDIO_CLKCR_WIDBUS) | SDIO_CLKCR_WIDBUS_0;
    }

    // Increase clock up to 9.6MHz
    // We will get timeouts at higher frequencies when the C64 bus interface is active
    SDIO->CLKCR = (clkcr & ~SDIO_CLKCR_CLKDIV) | 3;
    delay_us(9);  // Wait for 80 cycles at 9.6MHz

    dstatus &= ~STA_NOINIT;
    return RES_OK;

fail:
    dstatus = STA_NOINIT;
    sdio_deinit();
    return RES_ERROR;
}

DSTATUS disk_status(BYTE pdrv)
{
    return dstatus;
}

DRESULT disk_read(BYTE pdrv, BYTE* buf, DWORD sector, UINT count)
{
    led_toggle();

    if (count < 1 || count > 127)
    {
        return RES_PARERR;
    }

    if (dstatus & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    if (!(card_type & CT_BLOCK))
    {
        sector *= 512;
    }

    if (!sdio_check_tran(500))
    {
        return RES_ERROR;
    }

    SDIO->DCTRL = SDIO_DCTRL_DBLOCKSIZE_0|SDIO_DCTRL_DBLOCKSIZE_3|SDIO_DCTRL_DTDIR;
    SDIO->DTIMER = 24000000;
    SDIO->DLEN = 512 * count;

    int cmd = (count > 1) ? 18 : 17;
    uint32_t resp;
    if (!sdio_cmd_send(cmd, sector, RESP_SHORT, &resp) || (resp & 0xc0580000))
    {
        return RES_ERROR;
    }

    uint32_t *buf32 = (uint32_t *)buf;
    int rd = 0;

    SDIO->ICR = SDIO_ICR_DATA_FLAGS;
    SDIO->DCTRL |= SDIO_DCTRL_DTEN;

    while (true)
    {
        uint32_t sta = SDIO->STA;
        if (sta & (SDIO_STA_DCRCFAIL|SDIO_STA_DTIMEOUT|SDIO_STA_RXOVERR|SDIO_STA_STBITERR))
        {
            err("%s SDIO_STA: %08x\n", __func__, sta);
            break;
        }

        if (sta & SDIO_STA_RXDAVL)
        {
            *buf32++ = SDIO->FIFO;
            rd += 4;

            if (rd == 512 * count)
            {
                break;
            }
        }
    }

    if (rd < 512 * count || cmd == 18)
    {
        // Empty FIFO buffer
        while (SDIO->STA & SDIO_STA_RXDAVL)
        {
            (void)SDIO->FIFO;
        }

        sdio_cmd_send(12, 0, RESP_SHORT, &resp);
    }

    return rd < 512 * count ? RES_ERROR : RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buf, DWORD sector, UINT count)
{
    led_toggle();

    if (count < 1 || count > 127)
    {
        return RES_PARERR;
    }

    if (dstatus & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    // Note: No check of Write Protect Pin

    if (!(card_type & CT_BLOCK))
    {
        sector *= 512;
    }

    if (!sdio_check_tran(500))
    {
        return RES_ERROR;
    }

    uint32_t resp;
    int cmd;

    if (count == 1) // Single block write
    {
        cmd = 24;
    }
    else // Multiple block write
    {
        cmd = (card_type & CT_SDC) ? ACMD(23) : 23;
        if (!sdio_cmd_send(cmd, count, RESP_SHORT, &resp) || (resp & 0xC0580000))
        {
            return RES_ERROR;
        }

        cmd = 25;
    }

    SDIO->DCTRL = SDIO_DCTRL_DBLOCKSIZE_0|SDIO_DCTRL_DBLOCKSIZE_3;
    SDIO->DTIMER = 24000000;
    SDIO->DLEN = 512 * count;

    if (!sdio_cmd_send(cmd, sector, RESP_SHORT, &resp) || (resp & 0xC0580000))
    {
        err("%s %d\n", __func__, __LINE__);
        return RES_ERROR;
    }

    const uint32_t *buf32 = (const uint32_t *)buf;
    int wr = 0;

    SDIO->ICR = SDIO_ICR_DATA_FLAGS;

    // Note: Will not work while the C64 interrupt handler is enabled
    __disable_irq();
    uint32_t first_word = *buf32++;
    __DSB();
    SDIO->DCTRL |= SDIO_DCTRL_DTEN;

    // Send the first 8 words to avoid TX FIFO underrun
    SDIO->FIFO = first_word;
    for (int i=0; i<7; i++)
    {
        SDIO->FIFO = *buf32++;
    }
    wr += 4*8;

    while (true)
    {
        uint32_t sta = SDIO->STA;
        if (sta & (SDIO_STA_DCRCFAIL|SDIO_STA_DTIMEOUT|SDIO_STA_TXUNDERR|SDIO_STA_STBITERR))
        {
            err("%s SDIO_STA: %08x\n", __func__, sta);
            break;
        }

        if (sta & SDIO_STA_TXFIFOF)
        {
            continue;
        }

        if (wr < 512 * count)
        {
            SDIO->FIFO = *buf32++;
            wr += 4;
            continue;
        }

        // Wait until all data has been sent
        if ((sta & (SDIO_STA_TXACT|SDIO_STA_DATAEND)) == SDIO_STA_DATAEND)
        {
            break;
        }
    }
    __enable_irq();

    if (wr < 512 * count || (cmd == 25 && (card_type & CT_SDC)))
    {
        sdio_cmd_send(12, 0, RESP_SHORT, &resp);
    }

    return wr < 512 * count ? RES_ERROR : RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (dstatus & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    // Complete pending write process
    if (cmd == CTRL_SYNC)
    {
        return RES_OK;
    }

    return RES_ERROR;
}
