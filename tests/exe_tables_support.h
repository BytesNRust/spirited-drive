/* What the exe-table extractors' tests share: the real NFS_WIN.EXE read from original/ (a test skips its real part without it)
 * and a tiny PE whose only section has no table in it, for the error path. */
#ifndef TNFS_TESTS_EXE_TABLES_SUPPORT_H
#define TNFS_TESTS_EXE_TABLES_SUPPORT_H

#include "game/exe/exe_tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The real exe, or NULL when original/NFS_WIN.EXE is not there. */
static uint8_t *real_exe(size_t *len)
{
#ifdef TNFS_SOURCE_DIR
    char path[1024];
    snprintf(path, sizeof path, "%s/original/NFS_WIN.EXE", TNFS_SOURCE_DIR);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n);
    if (b == NULL || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f);
    *len = (size_t)n;
    return b;
#else
    (void)len;
    return NULL;
#endif
}

/* A PE with one .data section at 0x402000 (0x40 bytes): none of the tables' addresses is inside it. */
static uint8_t *tiny_exe(size_t *len)
{
    enum { PE_AT = 0x80, OPT_SIZE = 0xE0, SIZE = 0x300 };
    uint8_t *b = calloc(1, SIZE);
    b[0] = 'M', b[1] = 'Z', b[0x3C] = PE_AT;
    memcpy(b + PE_AT, "PE\0\0", 4);
    b[PE_AT + 6] = 1;
    b[PE_AT + 20] = OPT_SIZE;
    b[PE_AT + 24 + 30] = 0x40;   /* the image base 0x400000: the byte at +2 of the field */
    const size_t at = PE_AT + 24 + OPT_SIZE;
    memcpy(b + at, ".data", 5);
    b[at + 8] = 0x40, b[at + 13] = 0x20, b[at + 16] = 0x40, b[at + 21] = 0x02;   /* vsize 0x40, rva 0x2000, raw size 0x40, raw ptr 0x200 */
    *len = SIZE;
    return b;
}

#endif
