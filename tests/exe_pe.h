/* The tests' invented exe: a PE with one section of zeros at a given address, built in memory, for the extractors' error
 * paths and the parsing rules (no byte of the real exe in it). */
#ifndef TNFS_TESTS_EXE_PE_H
#define TNFS_TESTS_EXE_PE_H

#include "game/exe/exe_image.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *data;
    size_t len;
    uint32_t va;     /* the section's first address */
    uint32_t size;
} TestExe;

static inline void exe_pe_put32(uint8_t *b, size_t at, uint32_t v)
{
    for (int i = 0; i < 4; i++) b[at + (size_t)i] = (uint8_t)(v >> (8 * i));
}

/* a section ".data" of `size` zero bytes at `va` (image base 0x400000) */
static inline TestExe exe_pe_make(uint32_t va, uint32_t size)
{
    enum { PE_AT = 0x80, OPT = 0xE0, RAW = 0x400 };
    TestExe t = { calloc(1, RAW + size), RAW + size, va, size };
    uint8_t *b = t.data;
    b[0] = 'M'; b[1] = 'Z';
    exe_pe_put32(b, 0x3C, PE_AT);
    memcpy(b + PE_AT, "PE\0\0", 4);
    b[PE_AT + 6] = 1;
    b[PE_AT + 20] = OPT;
    exe_pe_put32(b, PE_AT + 24 + 28, 0x400000);
    size_t h = PE_AT + 24 + OPT;
    memcpy(b + h, ".data", 5);
    exe_pe_put32(b, h + 8, size);
    exe_pe_put32(b, h + 12, va - 0x400000);
    exe_pe_put32(b, h + 16, size);
    exe_pe_put32(b, h + 20, RAW);
    return t;
}

static inline void exe_pe_poke(TestExe *t, uint32_t va, const void *bytes, size_t n)
{
    memcpy(t->data + 0x400 + (va - t->va), bytes, n);
}
static inline void exe_pe_poke16(TestExe *t, uint32_t va, uint16_t v) { uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) }; exe_pe_poke(t, va, b, 2); }
static inline void exe_pe_poke32(TestExe *t, uint32_t va, uint32_t v) { uint8_t b[4]; exe_pe_put32(b, 0, v); exe_pe_poke(t, va, b, 4); }
static inline void exe_pe_pokes(TestExe *t, uint32_t va, const char *s) { exe_pe_poke(t, va, s, strlen(s) + 1); }

#endif
