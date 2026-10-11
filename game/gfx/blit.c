/* The 2D shape draw (blit.h). */
#include "game/gfx/blit.h"

#include <stddef.h>
#include <string.h>

enum {
    SHAPE_TYPE_8_BIT = 0x7b,
    SHAPE_WIDTH = 4, SHAPE_HEIGHT = 6, SHAPE_X = 0xc, SHAPE_Y = 0xe, SHAPE_PIXELS = 0x10,
    TRANSPARENT_COLOUR = 0xff
};

static int32_t s16(const uint8_t *b)
{
    return (int16_t)(uint16_t)(b[0] | b[1] << 8);
}

/* 0048ec60 for 004c61b8; 00473904 for 004c61d4. */
static void row(uint8_t *dst, const uint8_t *src, int32_t count, BlitMode mode)
{
    if (mode == BLIT_OPAQUE) {
        memmove(dst, src, (size_t)count);
        return;
    }
    for (int32_t i = 0; i < count; i++)
        if (src[i] != TRANSPARENT_COLOUR) dst[i] = src[i];
}

int blit_shape(const QuadTarget *t, const uint8_t *shape, int32_t x, int32_t y, BlitMode mode)
{
    if (shape[0] != SHAPE_TYPE_8_BIT) return -1;
    int32_t width = s16(shape + SHAPE_WIDTH), height = s16(shape + SHAPE_HEIGHT);
    const uint8_t *src = shape + SHAPE_PIXELS;
    int32_t skip = 0;
    const int32_t above = t->clip_top - y;
    if (above > 0) {
        src += (ptrdiff_t)width * above;
        y += above;
        height -= above;
    }
    const int32_t below = y + height - t->clip_bottom;
    if (below > 0) height -= below;
    const int32_t before = t->clip_left - x;
    if (before > 0) {
        x += before;
        width -= before;
        skip = before;
        src += before;
    }
    const int32_t after = width + x - t->clip_right;
    if (after > 0) {
        skip += after;
        width -= after;
    }
    if (width <= 0 || height <= 0) return 0;
    uint8_t *dst = t->pixels + (ptrdiff_t)y * t->pitch + x;
    for (; height > 0; height--, src += width + skip, dst += t->pitch) row(dst, src, width, mode);
    return 0;
}

int blit_shape_placed(const QuadTarget *t, const uint8_t *shape)
{
    return blit_shape(t, shape, s16(shape + SHAPE_X), s16(shape + SHAPE_Y), BLIT_OPAQUE);
}
