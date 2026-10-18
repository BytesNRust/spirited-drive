#include "game/frontend/tgv.h"

static uint32_t dword_at(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* 004a2b90 */
uint32_t tgv_shape_type(uint32_t bits)
{
    switch (bits) {
    case 1: return 0x79;
    case 4: return 0x7a;
    case 8: return 0x7b;
    case 15: return 0x7e;
    case 16: return 0x78;
    case 24: return 0x7f;
    case 32: return 0x7d;
    default: return 0;
    }
}

/* 0048e1dc */
void tgv_expand_blocks(const uint8_t *src, uint8_t *dst, int32_t blocks)
{
    for (int32_t b = 0; b < blocks; b++, src += 8, dst += 16) {
        const uint32_t w = (uint32_t)src[4] | (uint32_t)src[5] << 8 | (uint32_t)src[6] << 16 | (uint32_t)src[7] << 24;
        for (int i = 0; i < 16; i++) dst[i] = src[w >> (30 - 2 * i) & 3];
    }
}

/* 0048e120 */
void tgv_unpack_fields(int32_t *out, const uint8_t *bits, int32_t count, int32_t width)
{
    const uint32_t sign = 1u << ((width - 1) & 31), mask = (1u << (width & 31)) - 1, high = 0xffffffffu << (width & 31);
    uint32_t at = (uint32_t)(count - 1) * (uint32_t)width;
    for (int32_t i = count - 1; i >= 0; i--, at -= (uint32_t)width) {
        const uint8_t *p = bits + ((int32_t)at >> 3);
        uint32_t v = dword_at(p) >> (at & 7) & mask;
        if (v & sign) v |= high;
        out[i] = (int32_t)v;
    }
}

