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

static bool (*c64_wait_handler)(void);

// Use KFF registers to communicate with C64
static inline bool c64_get_reply(u8 cmd, u8 *reply)
{
    return kff_get_reply(cmd, reply);
}

static inline void c64_set_command(u8 cmd)
{
    kff_set_command(cmd);
}

static inline u8 c64_receive_byte(void)
{
    return kff_receive_byte();
}

static inline void c64_send_byte(u8 data)
{
    kff_send_byte(data);
}

static void c64_receive_data(void *buffer, size_t size)
{
    u8 *buf_ptr = (u8 *)buffer;

    while (size--)
    {
        u8 c = c64_receive_byte();
        *buf_ptr++ = c;
    }
}

static void c64_send_data(const void *buffer, size_t size)
{
    const u8 *buf_ptr = (const u8 *)buffer;

    while (size--)
    {
        c64_send_byte(*buf_ptr++);
    }
}

static void c64_wait_for_command(u8 cmd)
{
    u8 reply;
    while (!c64_get_reply(cmd, &reply))
    {
        if (c64_wait_handler && !c64_wait_handler())
        {
            c64_set_command(CMD_NONE);
            break;
        }
    }
}

static void c64_send_command(u8 cmd)
{
    c64_set_command(cmd);
    c64_wait_for_command(cmd);
}

static void c64_interface_sync(void)
{
    c64_set_command(CMD_SYNC);
    c64_interface(true);

    c64_wait_for_command(CMD_SYNC);
}

static char sanitize_char(char c)
{
    if (c == '\r' || c == '\n')
    {
        c = '_';
    }

    return c;
}

// From https://sta.c64.org/cbm64pettoscr.html
static char * convert_to_screen_code(char *dest, const char *src)
{
    while (*src)
    {
        char c = *src++;
        if (c <= 0x1f)
        {
            c |= 0x80;
        }
        else if (c <= 0x3f)
        {
            // No conversion
        }
        else if (c <= 0x5f)
        {
            c &= ~0x40;
        }
        else if (c <= 0x7f)
        {
            c &= ~0x20;
        }
        else if (c <= 0x9f)
        {
            c |= 0x40;
        }
        else if (c <= 0xbf)
        {
            c &= ~0x40;
        }
        else if (c <= 0xfe)
        {
            c &= ~0x80;
        }
        else
        {
            c = 0x5e;
        }

        *dest++ = c;
    }

    return dest;
}

static char petscii_to_ascii(char c)
{
    if (c & 0x80)
    {
        c &= 0x7f;
    }
    else if (c >= 'A' && c <= 'Z')
    {
        c += 0x20;
    }

    return c;
}

static u16 convert_to_ascii(char *dest, const u8 *src, u8 size)
{
    u16 i;
    for (i=0; i<size-1; i++)
    {
        char c = *src++;
        if (c == 0)
        {
            break;
        }

        dest[i] = petscii_to_ascii(c);
    }

    dest[i] = 0;
    return i + 1;
}

static char ascii_to_petscii(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        c += 0x20;
    }
    else if (c >= 'a' && c <= 'z')
    {
        c -= 0x20;
    }
    else if (c == '_')
    {
        c = 0xa4;
    }

    return c;
}

static void c64_send_petscii(const char *text)
{
    dbg("Sending text \"%s\"", text);

    u16 text_len = strlen(text) + 1;
    if (text_len > 800)
    {
        text_len = 800;
    }
    c64_send_data(&text_len, 2);

    for (int i=0; i<(text_len-1); i++)
    {
        char c = ascii_to_petscii(text[i]);
        c64_send_byte(c);
    }
    c64_send_byte(0);
}

static void c64_send_cxy_text(u8 color, u8 x, u8 y, const char *text)
{
    c64_send_byte(color);
    c64_send_byte(x);
    c64_send_byte(y);
    c64_send_petscii(text);
}

static void c64_send_text(u8 color, u8 x, u8 y, const char *text)
{
    c64_send_cxy_text(color, x, y, text);
    c64_send_command(CMD_TEXT);
}

static void c64_send_text_wait(u8 color, u8 x, u8 y, const char *text)
{
    c64_send_cxy_text(color, x, y, text);
    c64_send_command(CMD_TEXT_WAIT);
}

static void c64_send_message_command(u8 cmd, const char *text)
{
    c64_send_petscii(text);
    c64_send_command(cmd);
}

static inline void c64_send_message(const char *text)
{
    c64_send_message_command(CMD_MESSAGE, text);
}

static inline void c64_send_warning(const char *text)
{
    c64_send_message_command(CMD_WARNING, text);
}

static inline void c64_send_prg_message(const char *text)
{
    c64_send_message_command(CMD_FLASH_MESSAGE, text);
}

static void c64_receive_string(char *buffer)
{
    u8 size = c64_receive_byte();
    c64_receive_data(buffer, size);
    buffer[size] = 0;
}

// Use EF3 USB registers to communicate with C64
static void ef3_receive_data(void *buffer, size_t size)
{
    u8 *buf_ptr = (u8 *)buffer;

    while (size--)
    {
        *buf_ptr++ = ef3_getc();
    }
}

static void ef3_send_data(const void *buffer, size_t size)
{
    const u8 *buf_ptr = (const u8 *)buffer;

    while (size--)
    {
        ef3_putc(*buf_ptr++);
    }
}

static inline u8 ef3_receive_byte(void)
{
    return ef3_getc();
}

static inline void ef3_send_reply(u8 reply)
{
    ef3_putc(reply);
}

static bool ef3_send_command(const char *cmd, const char *exp)
{
    char buffer[6];

    ef3_send_data("EFSTART:", 8);
    ef3_send_data(cmd, 4);

    ef3_receive_data(buffer, 5);
    buffer[5] = 0;

    if (strcmp(buffer, exp) != 0)
    {
        wrn("Command %s failed. Expected %s got: %s", cmd, exp, buffer);
        return false;
    }

    return true;
}

static u8 ef3_receive_command(void)
{
    char cmd_buf[5];

    ef3_receive_data(cmd_buf, sizeof(cmd_buf));
    if (memcmp("KFF:", cmd_buf, 4) != 0)
    {
        wrn("Got invalid command: %x %x %x %x %x",
            cmd_buf[0], cmd_buf[1], cmd_buf[2], cmd_buf[3], cmd_buf[4]);
        return CMD_NONE;
    }

    return cmd_buf[4];
}

static bool ef3_wait_for_close(void)
{
    char buffer[2];

    ef3_receive_data(buffer, 2);
    if (buffer[0] != 0 || buffer[1] != 0)
    {
        wrn("Expected close, but got: %x %x", buffer[0], buffer[1]);
        return false;
    }

    return true;
}

static bool c64_send_prg(const void *data, u16 size)
{
    u16 tx_size;

    dbg("Sending PRG file. Size: %u bytes", size);
    if (!ef3_send_command("PRG", "LOAD"))
    {
        return false;
    }

    ef3_receive_data(&tx_size, 2);
    if (tx_size > size)
    {
        tx_size = size;
    }

    ef3_send_data(&tx_size, 2);
    ef3_send_data(data, tx_size);

    if (!ef3_wait_for_close())
    {
        return false;
    }

    return tx_size == size;
}
