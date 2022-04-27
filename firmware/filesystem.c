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

static FATFS fs;

static bool filesystem_mount(void)
{
    FRESULT res = f_mount(&fs, "", 1);
    if (res != FR_OK)
    {
        err("f_mount failed (%x)", res);
    }

    led_on();
    return res == FR_OK;
}

static bool filesystem_unmount(void)
{
    FRESULT res = f_unmount("");
    return res == FR_OK;
}

static size_t filesystem_getfree(void)
{
    FATFS *fs_ptr = &fs;
    DWORD fre_clust = 0;

    FRESULT res = f_getfree("", &fre_clust, &fs_ptr);
    if (res != FR_OK)
    {
        err("f_getfree failed (%x)", res);
    }

    led_on();

    return fre_clust * fs.csize;
}

static bool filesystem_getlabel(char *label)
{
    FRESULT res = f_getlabel("", label, NULL);
    if (res != FR_OK)
    {
        err("f_getlabel failed (%x)", res);
    }

    led_on();
    return res == FR_OK;
}

static bool file_open(FIL *file, const char *file_name, u8 mode)
{
    FRESULT res = f_open(file, file_name, mode);
    if (res != FR_OK)
    {
        err("f_open '%s' failed (%u)", file_name, res);
        led_on();
    }

    return res == FR_OK;
}

static u32 file_read(FIL *file, void *buffer, size_t bytes)
{
    UINT bytes_read;
    FRESULT res = f_read(file, buffer, bytes, &bytes_read);
    if (res != FR_OK)
    {
        err("f_read failed (%u)", res);
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
        err("f_lseek failed (%u)", res);
    }

    led_on();
    return res == FR_OK;
}

static u32 file_write(FIL *file, void *buffer, size_t bytes)
{
    UINT bytes_written;
    FRESULT res = f_write(file, buffer, bytes, &bytes_written);
    if (res != FR_OK)
    {
        err("f_write failed (%u)", res);
        bytes_written = 0;
    }

    led_on();
    return bytes_written;
}

static bool file_truncate(FIL *file)
{
    FRESULT res = f_truncate(file);
    if (res != FR_OK)
    {
        err("f_truncate failed (%u)", res);
    }

    led_on();
    return res == FR_OK;
}

static bool file_sync(FIL *file)
{
    FRESULT res = f_sync(file);
    if (res != FR_OK)
    {
        err("f_sync failed (%u)", res);
    }

    led_on();
    return res == FR_OK;
}

static bool file_close(FIL *file)
{
    FRESULT res = f_close(file);
    if (res != FR_OK)
    {
        err("f_close failed (%u)", res);
    }

    led_on();
    return res == FR_OK;
}

static bool file_stat(const char *file_name, FILINFO *file_info)
{
    FRESULT res = f_stat(file_name, file_info);
    if (res != FR_OK)
    {
        err("f_stat failed (%u)", res);
    }

    led_on();
    return res == FR_OK;
}

static bool file_delete(const char *file_name)
{
    FRESULT res = f_unlink(file_name);
    if (res != FR_OK)
    {
        err("f_unlink failed (%u)", res);
    }

    led_on();
    return res == FR_OK;
}

static bool dir_change(const char *path)
{
    FRESULT res = f_chdir(path);
    if (res != FR_OK)
    {
        err("f_chdir '%s' failed (%u)", path, res);
    }

    led_on();
    return res == FR_OK;
}

static bool dir_current(char *path, size_t path_size)
{
    FRESULT res = f_getcwd(path, path_size);
    if (res != FR_OK)
    {
        err("f_getcwd failed (%u)", res);
    }

    led_on();
    return res == FR_OK;
}

static bool dir_open(DIR *dir, const char *pattern)
{
    if (!pattern || !pattern[0])
    {
        pattern = "*";
    }
    dir->pat = pattern;

    FRESULT res = f_opendir(dir, "");
    if (res != FR_OK)
    {
        err("f_opendir failed (%u)", res);
        led_on();
    }

    return res == FR_OK;
}

static bool dir_read(DIR *dir, FILINFO *file_info)
{
    FRESULT res;
    do
    {
        res = f_findnext(dir, file_info);

        // Ignore dot files
        if (file_info->fname[0] != '.')
        {
            break;
        }
    }
    while (res == FR_OK);

    if (res != FR_OK)
    {
        err("f_findnext failed (%u)", res);
    }

    led_on();
    return res == FR_OK;
}

static bool dir_close(DIR *dir)
{
    FRESULT res = f_closedir(dir);
    if (res != FR_OK)
    {
        err("f_closedir failed (%u)", res);
    }

    led_on();
    return res == FR_OK;
}
