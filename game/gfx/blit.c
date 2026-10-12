/* The 2D shape draw (blit.h). */
#include "game/gfx/blit.h"

#include <stddef.h>
#include <string.h>

enum {
    SHAPE_TYPE_8_BIT = 0x7b,
    SHAPE_WIDTH = 4, SHAPE_HEIGHT = 6, SHAPE_X = 0xc, SHAPE_Y = 0xe, SHAPE_PIXELS = 0x10,
    TRANSPARENT_COLOUR = 0xff,
    RUNS_BLOCKS = 8, RUNS_STEP = 0xc, RUNS_FIRST = 0x14, RUN_SIZE = 8,
    RUN_TO = 4, RUN_COUNT = 6, BLOCK_PIXELS_SHIFT = 16
};

static int32_t s16(const uint8_t *b)
{
    return (int16_t)(uint16_t)(b[0] | b[1] << 8);
}

static int32_t u16(const uint8_t *b)
{
    return b[0] | b[1] << 8;
}

static uint32_t u32(const uint8_t *b)
{
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
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

int blit_shape_placed_transparent(const QuadTarget *t, const uint8_t *shape)
{
    return blit_shape(t, shape, s16(shape + SHAPE_X), s16(shape + SHAPE_Y), BLIT_TRANSPARENT);
}

/* 0048cc49 on an 8-bit surface, through 0049f580's table (from 0, the step
 * across, nothing down): entry k is (k x step) >> 16. */
static void run_copy(uint8_t *dst, const uint8_t *src, int32_t count, uint32_t step)
{
    for (int32_t k = 0; k < count; k++) dst[k] = src[(int32_t)((uint32_t)k * step) >> 16];
}

int blit_runs(const QuadTarget *t, int32_t width, const uint8_t *shape, const uint8_t *runs)
{
    if (shape[0] != SHAPE_TYPE_8_BIT) return -1;
    const int32_t shape_width = s16(shape + SHAPE_WIDTH);  /* the stride too: 8 bits a pixel */
    const uint8_t *pixels = shape + SHAPE_PIXELS;
    const int32_t blocks = u16(runs + RUNS_BLOCKS);
    const uint32_t step = u32(runs + RUNS_STEP);
    const uint8_t *run = runs + RUNS_FIRST;
    for (int32_t block = 0; block < blocks; block++) {
        const int32_t n = u16(run);
        run += 2;
        for (int32_t i = 0; i < n; i++, run += RUN_SIZE) {
            const int32_t from = (int32_t)u32(run);
            if (shape_width == 0 || width == 0 || (shape_width == -1 && from == INT32_MIN)) return 0;
            int32_t src_row = from / shape_width;
            const int32_t to = u16(run + RUN_TO) + (block << BLOCK_PIXELS_SHIFT);
            int32_t dst_row = to / width, dst_col = to % width;
            int32_t count = u16(run + RUN_COUNT);
            const uint8_t *src = pixels + (ptrdiff_t)src_row * shape_width + from % shape_width;
            uint8_t *dst = t->pixels + (ptrdiff_t)dst_row * t->pitch + dst_col;
            while (dst_col + count > width) {
                run_copy(dst, src, width - dst_col, step);
                count -= width - dst_col;
                dst_col = 0;
                src = pixels + (ptrdiff_t)++src_row * shape_width;
                dst = t->pixels + (ptrdiff_t)++dst_row * t->pitch;
            }
            run_copy(dst, src, count, step);
        }
    }
    return 0;
}
