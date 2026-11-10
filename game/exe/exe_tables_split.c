/* port: the split times' table read from NFS_WIN.EXE (the 1:1 port of tools/gen_split_tables.py): the checkpoint templates
 * (004c584c, rows of 5 words by track x 3 + variant) and right after them the rival's time factors (004c5a68), one block
 * because the last tracks' checkpoint rows read into the factors. */
#include "game/exe/exe_tables.h"

#include <stdio.h>

int exe_tables_split(const ExeImage *img, ExeTables *t, char *err, size_t errsz)
{
    enum { BASE = 0x4C584C };
    if (exe_va_read(img, BASE, t->race_split_tables, RACE_SPLIT_TABLES_SIZE) != 0) {
        if (err != NULL && errsz > 0) snprintf(err, errsz, "the exe has no data at %08x (the split times)", BASE);
        return -1;
    }
    return 0;
}
