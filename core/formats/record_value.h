/* Internal: numeric field kinds shared by record_exchange.c and table_exchange.c. */
#ifndef TNFS_RECORD_VALUE_H
#define TNFS_RECORD_VALUE_H

#include "formats/record_exchange.h"

static const char *const rec_kind_names[] = { "i32", "u32", "i16", "u16", "i8", "u8", "bytes" };
static const int rec_kind_size[] = { 4, 4, 2, 2, 1, 1, 1 };
static const int rec_kind_signed[] = { 1, 0, 1, 0, 1, 0, 0 };

static inline int64_t rec_load(const uint8_t *p, int size, int is_signed)
{
    uint32_t v = 0;
    for (int i = 0; i < size; ++i) v |= (uint32_t)p[i] << (8 * i);
    if (!is_signed) return v;
    const int bits = 8 * size;
    return bits == 32 ? (int64_t)(int32_t)v : (int64_t)(v ^ (1u << (bits - 1))) - (int64_t)(1u << (bits - 1));
}

static inline void rec_store(uint8_t *p, int size, int64_t v)
{
    for (int i = 0; i < size; ++i) p[i] = (uint8_t)((uint64_t)v >> (8 * i));
}

static inline int rec_fits(int64_t v, int size, int is_signed)
{
    const int bits = 8 * size;
    if (is_signed) return v >= -((int64_t)1 << (bits - 1)) && v < ((int64_t)1 << (bits - 1));
    return v >= 0 && v < ((int64_t)1 << bits);
}
#endif
