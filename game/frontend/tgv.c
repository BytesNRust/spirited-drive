#include "game/frontend/tgv.h"

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
