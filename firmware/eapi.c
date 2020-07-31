
static void eapi_disable_interface(void)
{
    c64_send_reply(REPLY_WRITE_WAIT);
    c64_receive_byte(); // Wait for C64
    c64_interface(false);
}

static void eapi_enable_interface(void)
{
    c64_interface(true);
    c64_send_data("DONE", 4);
}

static void eapi_set_updated(void)
{
    dat_file.crt.flags |= CRT_FLAG_UPDATED;
    if (!save_dat())
    {
        restart_to_menu();
    }
}

static void eapi_handle_write_flash(uint16_t addr, uint8_t value)
{
    if (crt_ptr >= crt_banks[0] && crt_ptr <= crt_banks[3])
    {
        // TODO: Write to dat_file
        dbg("Got unsupported address to program: crt_ptr %x\n", crt_ptr);
        c64_send_reply(REPLY_WRITE_ERROR);
    }
    else
    {
        if (!(dat_file.crt.flags & CRT_FLAG_UPDATED))
        {
            eapi_disable_interface();
            eapi_set_updated();
            eapi_enable_interface();
        }

        uint8_t *dest = crt_ptr + (addr & 0x3fff);
        flash_program_byte(dest, value);

        uint8_t result = REPLY_OK;
        if (*((volatile uint8_t *)dest) != value)
        {
            wrn("Flash write failed at $%04x (%x)\n", addr, crt_ptr);
            result = REPLY_WRITE_ERROR;
        }
        c64_send_reply(result);
    }
}

static void eapi_handle_erase_sector(uint8_t bank, uint16_t addr)
{
    if (bank >= 64 || (bank % 8))
    {
        wrn("Got invalid sector to erase: bank %d\n", bank);
        c64_send_reply(REPLY_WRITE_ERROR);
        return;
    }

    uint8_t sector = bank / 8;
    int8_t sector_to_erase = sector + 4;
    uint16_t other_offset = (addr & 0xff00) == 0x8000 ? 8*1024 : 0;
    eapi_disable_interface();

    uint8_t result = REPLY_OK;
    if (!sector)
    {
        // TODO: Erase part of file and part of sector 4
        dbg("Got unsupported sector to erase: bank %d\n", bank);
        result = REPLY_WRITE_ERROR;
    }
    else
    {
        if (!(dat_file.crt.flags & CRT_FLAG_UPDATED))
        {
            eapi_set_updated();
        }

        // Backup other 64k of 128k flash sector in dat_buffer
        for (uint8_t i=0; i<8; i++)
        {
            memcpy(dat_buffer + (i * 8*1024),
                   crt_banks[bank + i] + other_offset, 8*1024);
        }

        // Erase 128k sector and restore other 64k
        for (uint8_t i=0; i<8; i++)
        {
            flash_sector_program(sector_to_erase,
                                 crt_banks[bank + i] + other_offset,
                                 dat_buffer + (i * 8*1024), 8*1024);
            sector_to_erase = -1;
        }

        // Restore dat_buffer
        if (!load_dat())
        {
            restart_to_menu();
        }
    }

    eapi_enable_interface();
    c64_send_reply(result);
}

static void eapi_loop(void)
{
    uint16_t addr;
    uint8_t value;

    while (true)
    {
        uint8_t command = c64_receive_command();
        switch (command)
        {
            case CMD_NONE:
            break;

            case CMD_EAPI_INIT:
            {
                c64_send_reply(REPLY_OK);
            }
            break;

            case CMD_WRITE_FLASH:
            {
                c64_receive_data(&addr, 2);
                value = c64_receive_byte();
                eapi_handle_write_flash(addr, value);
            }
            break;

            case CMD_ERASE_SECTOR:
            {
                c64_receive_data(&addr, 2);
                value = c64_receive_byte();
                dbg("Got ERASE_SECTOR command (%d:$%04x)\n", value, addr);
                eapi_handle_erase_sector(value, addr);
            }
            break;

            default:
            {
                wrn("Got unknown EAPI command: %02x\n", command);
                c64_send_reply(REPLY_OK);
            }
            break;
        }
    }
}
