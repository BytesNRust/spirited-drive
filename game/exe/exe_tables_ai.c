/* port: the AI drivers' table read from NFS_WIN.EXE (the 1:1 port of tools/gen_ai_profiles.py): the driver letters (004c2850,
 * 7 rows of 8) and the 9 profiles right after them (004c2888, 0x8d bytes each), one block because row 7 of the letters reads
 * into the first profile. */
#include "game/exe/exe_tables.h"

#include <stdio.h>

int exe_tables_ai(const ExeImage *img, ExeTables *t, char *err, size_t errsz)
{
    enum { BASE = 0x4C2850 };
    if (exe_va_read(img, BASE, t->ai_driver_data, AI_DRIVER_DATA_SIZE) != 0) {
        if (err != NULL && errsz > 0) snprintf(err, errsz, "the exe has no data at %08x (the AI drivers)", BASE);
        return -1;
    }
    return 0;
}
