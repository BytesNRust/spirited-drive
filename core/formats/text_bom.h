/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A text file a player or an author saved on Windows may start with a UTF-8 byte order mark (EF BB BF: Notepad's "UTF-8 with BOM", VS Code's, PowerShell's): it is not part of the
 * first line, and a line that began with it would read as an unknown key. */
#ifndef TNFS_TEXT_BOM_H
#define TNFS_TEXT_BOM_H

#include <stddef.h>
#include <stdint.h>

/* the start of `text` past a UTF-8 BOM, `size` reduced by it */
static inline const uint8_t *text_skip_bom(const uint8_t *text, size_t *size)
{
    if (text != NULL && *size >= 3 && text[0] == 0xEF && text[1] == 0xBB && text[2] == 0xBF) {
        *size -= 3;
        return text + 3;
    }
    return text;
}

/* a UTF-16 file (FF FE or FE FF: PowerShell 5's `>` and Out-File write it): the files here are UTF-8, and a UTF-16 one reads as binary */
static inline int text_is_utf16(const uint8_t *text, size_t size)
{
    return text != NULL && size >= 2 && ((text[0] == 0xFF && text[1] == 0xFE) || (text[0] == 0xFE && text[1] == 0xFF));
}

#endif
