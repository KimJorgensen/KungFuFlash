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

// Use EF3 USB registers to communicate with C64
static void c64_ef3_read(void *buffer, size_t size)
{
    uint8_t *buf_ptr = (uint8_t *)buffer;

    while (size--)
    {
        uint8_t c = ef3_getc();
        *buf_ptr++ = c;
    }
}

static void c64_ef3_write(const void *buffer, size_t size)
{
    const uint8_t *buf_ptr = (const uint8_t *)buffer;

    while (size--)
    {
        ef3_putc(*buf_ptr++);
    }
}

static bool c64_send_command(const char *cmd, const char *exp)
{
    char buffer[6];

    c64_ef3_write("EFSTART:", 8);
    c64_ef3_write(cmd, 4);

    c64_ef3_read(buffer, 5);
    buffer[5] = 0;

    if (strcmp(buffer, exp) != 0)
    {
        wrn("Command %s failed. Expected %s got: %s\n", cmd, exp, buffer);
        return false;
    }

    return true;
}

static bool c64_wait_for_close(void)
{
    char buffer[2];

    c64_ef3_read(buffer, 2);
    if(buffer[0] != 0 || buffer[1] != 0)
    {
        wrn("Expected close, but got: %02x %02x\n", buffer[0], buffer[1]);
        return false;
    }

    return true;
}

static bool c64_send_prg(const void *data, uint16_t size)
{
    uint16_t tx_size;

    dbg("Sending PRG file. Size: %u bytes\n", size);
    if(!c64_send_command("PRG", "LOAD"))
    {
        return false;
    }

    c64_ef3_read(&tx_size, 2);
    if(tx_size > size)
    {
        tx_size = size;
    }

    c64_ef3_write(&tx_size, 2);
    c64_ef3_write(data, tx_size);

    if(!c64_wait_for_close())
    {
        return false;
    }

    return tx_size == size;
}

static inline bool c64_send_mount_disk(void)
{
    dbg("Sending mount disk\n");
    return c64_send_command("MNT", "DONE");
}

static inline bool c64_send_reset_to_menu(void)
{
    dbg("Sending reset to menu\n");
    return c64_send_command("RST", "DONE");
}

static inline bool c64_send_wait_for_reset(void)
{
    dbg("Sending wait for reset\n");
    if (c64_send_command("WFR", "DONE"))
    {
        return c64_wait_for_close();
    }

    return false;
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
    else if(c == '_')
    {
        c = 0xa4;
    }

    return c;
}

static uint16_t convert_to_petscii(char *dest, const char *src)
{
    uint16_t i;
    for (i=0; i<800; i++)
    {
        char c = *src++;
        if (c == 0)
        {
            break;
        }

        dest[i] = ascii_to_petscii(c);
    }

    dest[i] = 0;
    return i + 1;
}

static bool c64_send_text(const char *cmd, const void *text)
{
    uint16_t text_len = convert_to_petscii(scratch_buf, text);

    if(!c64_send_command(cmd, "READ"))
    {
        return false;
    }

    c64_ef3_write(&text_len, 2);
    c64_ef3_write(scratch_buf, text_len);

    if(!c64_wait_for_close())
    {
        return false;
    }

    return true;
}

static bool c64_send_message(const char *text)
{
    dbg("Sending message \"%s\"\n", text);
    return c64_send_text("MSG", text);
}

static bool c64_send_warning(const char *text)
{
    dbg("Sending warning message \"%s\"\n", text);
    return c64_send_text("MSW", text);
}

static bool c64_send_error(const char *text)
{
    dbg("Sending error message \"%s\"\n", text);
    return c64_send_text("MSE", text);
}

static bool c64_send_prg_message(const char *text)
{
    dbg("Sending flash programming message \"%s\"\n", text);
    return c64_send_text("MSF", text);
}

static inline void c64_receive_data(void *buffer, size_t size)
{
    c64_ef3_read(buffer, size);
}

static inline bool c64_got_command(void)
{
    return ef3_gotc();
}

static uint8_t c64_receive_command(void)
{
    char cmd_buf[5];

    c64_receive_data(cmd_buf, sizeof(cmd_buf));
    if (memcmp("KFF:", cmd_buf, 4) != 0)
    {
        wrn("Got invalid command: %02x %02x %02x %02x %02x\n",
            cmd_buf[0], cmd_buf[1], cmd_buf[2], cmd_buf[3], cmd_buf[4]);
        return CMD_NONE;
    }

    return cmd_buf[4];
}

static inline void c64_send_data(const void *buffer, size_t size)
{
    c64_ef3_write(buffer, size);
}

static inline void c64_send_reply(uint8_t reply)
{
    c64_send_data(&reply, 1);
}

static void c64_send_exit_menu(void)
{
    c64_send_reply(REPLY_EXIT_MENU);
    c64_wait_for_close();
}
