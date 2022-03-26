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
 * Derived from Draco Browser V1.0C 8 Bit (27.12.2009)
 * Created 2009 by Sascha Bader
 *
 * The code can be used freely as long as you retain
 * a notice describing original source and author.
 *
 * THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL,
 * BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
 *
 * Newer versions might be available here: http://www.sascha-bader.de/html/code.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "dir.h"
#include "kff_data.h"

/*
 * read a directory
 */
void readDir(Directory *dir)
{
    // Initialize directory
    dir->name[0] = ' ';
    dir->name[1] = 0;
    dir->no_of_elements = 0;
    dir->text_elements = 0;
    dir->selected = 0;

    kff_receive_data(dir->name, DIR_NAME_LENGTH);
    readDirPage(dir);
}

uint8_t readDirPage(Directory *dir)
{
    uint8_t element_no = 0;
    char first_element[ELEMENT_LENGTH+1];
    char *element = first_element;

    do
    {
        kff_receive_data(element, ELEMENT_LENGTH);
        if (element[0] == 0)
        {
            // end of dir
            break;
        }

        if (element[0] == TEXT_ELEMENT)
        {
            // assume that text is always before actionable elements
            dir->text_elements++;
        }
        else if (element[0] == SELECTED_ELEMENT)
        {
            dir->selected = element_no;
        }

        element = dir->elements[++element_no];
    }
    while (element_no < MAX_ELEMENTS_PAGE);

    if (element_no)
    {
        memcpy(&dir->elements[0], first_element, sizeof(first_element));
        dir->no_of_elements = element_no;
    }

    return element_no;
}
