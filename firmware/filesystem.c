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
 */
static FATFS fs;

static bool filesystem_mount(void)
{
    FRESULT res = f_mount(&fs, "", 1);
    if (res != FR_OK)
    {
        err("f_mount failed (%x)\n", res);
    }

    led_on();
    return res == FR_OK;
}

static bool filesystem_unmount(void)
{
    FRESULT res = f_unmount("");
    return res == FR_OK;
}

static bool file_open(FIL *file, const char *file_name, uint8_t mode)
{
    FRESULT res = f_open(file, file_name, mode);
    if (res != FR_OK)
    {
        err("f_open '%s' failed (%u)\n", file_name, res);
        led_on();
    }

    return res == FR_OK;
}

static uint32_t file_read(FIL *file, void *buffer, size_t bytes)
{
    UINT bytes_read;
    FRESULT res = f_read(file, buffer, bytes, &bytes_read);
    if (res != FR_OK)
    {
        err("f_read failed (%u)\n", res);
        bytes_read = 0;
    }

    led_on();
    return bytes_read;
}

static bool file_seek(FIL *file, FSIZE_t offset)
{
    FRESULT res = f_lseek(file, offset);
    if (res != FR_OK)
    {
        err("f_lseek failed (%u)\n", res);
    }

    led_on();
    return res == FR_OK;
}

static uint32_t file_write(FIL *file, void *buffer, size_t bytes)
{
    UINT bytes_written;
    FRESULT res = f_write(file, buffer, bytes, &bytes_written);
    if (res != FR_OK)
    {
        err("f_write failed (%u)\n", res);
        bytes_written = 0;
    }

    led_on();
    return bytes_written;
}

static bool file_close(FIL *file)
{
    FRESULT res = f_close(file);
    if (res != FR_OK)
    {
        err("f_close failed (%u)\n", res);
    }

    led_on();
    return res == FR_OK;
}

static bool dir_change(const char *path)
{
    FRESULT res = f_chdir(path);
    if (res != FR_OK)
    {
        err("f_chdir '%s' failed (%u)\n", path, res);
    }

    led_on();
    return res == FR_OK;
}

static bool dir_current(char *path, size_t path_size)
{
    FRESULT res = f_getcwd(path, path_size);
    if (res != FR_OK)
    {
        err("f_getcwd failed (%u)\n", res);
    }

    led_on();
    return res == FR_OK;
}

static bool dir_open(DIR *dir, const char *path)
{
    FRESULT res = f_opendir(dir, path);
    if (res != FR_OK)
    {
        err("f_opendir failed (%u)\n", res);
        led_on();
    }

    return res == FR_OK;
}

static bool dir_read(DIR *dir, FILINFO *file_info)
{
    FRESULT res = f_readdir(dir, file_info);
    if (res != FR_OK)
    {
        err("f_readdir failed (%u)\n", res);
    }

    led_on();
    return res == FR_OK;
}

static bool dir_close(DIR *dir)
{
    FRESULT res = f_closedir(dir);
    if (res != FR_OK)
    {
        err("f_closedir failed (%u)\n", res);
    }

    led_on();
    return res == FR_OK;
}
