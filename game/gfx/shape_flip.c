/* A shape mirrored left to right in place (0048f7a0). */
#include "game/gfx/shape_flip.h"

#include "game/gfx/x86_int.h"

enum { TYPE_PACKED = 0x80, TYPE_MASK = 0x7f, MIN_BITS = 8, BITS_15 = 15, BITS_16 = 16, SHAPE_PIXELS = 0x10,
       SHAPE_WIDTH = 4, SHAPE_HEIGHT = 6, SHAPE_X = 8 };

/* 004c7c50: bits a pixel by type (0 for none); 0x68..0x6f repeat 0x78..0x7f. */
static uint8_t type_bits(uint8_t type)
{
    static const uint8_t high[8] = { 16, 1, 4, 8, 0, 32, 15, 24 };
    if (type == 0x22 || type == 0x24) return 24;
    if ((type & 0xf0) == 0x60 && type >= 0x68) return high[type - 0x68];
    if ((type & 0xf0) == 0x70 && type >= 0x78) return high[type - 0x78];
    return 0;
}

static int32_t s16(const uint8_t *b)
{
    return (int16_t)(uint16_t)(b[0] | b[1] << 8);
}

/* 0049f530: the row's bytes, 15 bits counted as 16. */
static int32_t pitch(int32_t bits, int32_t width)
{
    return x_add(x_mul(bits == BITS_15 ? BITS_16 : bits, width), 7) >> 3;
}

static void swap(uint8_t *a, uint8_t *b, int32_t size)
{
    for (int32_t i = 0; i < size; i++) {
        const uint8_t t = a[i];
        a[i] = b[i], b[i] = t;
    }
}

int shape_flip(uint8_t *shape)
{
    const uint8_t type = shape[0];
    if (type & TYPE_PACKED) return 0;
    const int32_t width = s16(shape + SHAPE_WIDTH), height = s16(shape + SHAPE_HEIGHT);
    const int32_t bits = type_bits(type & TYPE_MASK);
    if (bits < MIN_BITS) return 0;
    const int32_t size = ((bits + 1) & ~1) / 8;
    const int32_t span = x_mul(width - 1, size), step = pitch(bits, width);
    uint8_t *row = shape + SHAPE_PIXELS;
    for (int32_t y = 0; y < height; y++, row += step) {
        /* offsets in the row: a negative span (width 0 or less) swaps nothing, as the original's unsigned test */
        for (int32_t left = 0, right = span; left < right; left += size, right -= size) swap(row + left, row + right, size);
    }
    const int32_t x = (uint16_t)s16(shape + SHAPE_X);
    const uint16_t flipped = (uint16_t)x_sub(width, x);
    shape[SHAPE_X] = (uint8_t)flipped, shape[SHAPE_X + 1] = (uint8_t)(flipped >> 8);
    return 1;
}
