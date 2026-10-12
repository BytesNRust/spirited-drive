/* The rear-view mirror's sky (0044bddc). */
#include "game/race/mirror_sky.h"

#include "game/gfx/x86_int.h"
#include "game/sys/fixed_math.h"
#include "game/sys/shape_bank.h"

#include <stddef.h>

enum {
    NODE_BEHIND = 3,
    PROPERTY_ENCLOSED_A = 4, PROPERTY_ENCLOSED_B = 7, PROPERTY_ENCLOSED_C = 9, PROPERTY_8 = 8,
    YAW_TURN = 0x200, YAW_HALF = 0x100, PITCH_TURN = 0x400, PITCH_HALF = 0x200,
    PITCH_HIGH_MUL = 480, PITCH_HIGH_DIV = 200,
    SCALE_ONE = 0x1000000, TRACK_LOW_STRIP = 5, LOW_STRIP_TOP = -17,
    SHAPE_WIDTH = 4, SHAPE_HEIGHT = 6, SHAPE_HORIZON = 0xa, SHAPE_PIXELS = 0x10
};

static int32_t s16(const uint8_t *b)
{
    return (int16_t)(uint16_t)(b[0] | b[1] << 8);
}

/* The yaw as a column 0..0x1ff, the strip's left edge. */
static int32_t yaw_column(int32_t yaw)
{
    return x_sub(0, x_add(x_shl(yaw, 1) >> 16, YAW_HALF)) & (YAW_TURN - 1);
}

/* The pitch as rows, -0x200..0x200 (more at high resolution). */
static int32_t pitch_rows(int32_t pitch, int32_t high)
{
    int32_t rows = x_shl(pitch, 2) >> 16;
    if (rows > PITCH_HALF) rows -= PITCH_TURN;
    return high != 0 ? rows * PITCH_HIGH_MUL / PITCH_HIGH_DIV : rows;
}

static void tiles(const MirrorSkyOps *ops, const MirrorSkyInputs *in, const uint8_t *horz, int32_t x, int32_t tile,
                  int32_t top, int32_t bottom)
{
    int mirrored = 0;
    while (x > in->width) x = x_sub(x, tile), mirrored ^= 1;
    while (x > x_sub(0, tile)) {
        const int32_t right = x_add(x, tile);
        const int flip = in->flip != 0 && mirrored;
        const int32_t points[8] = { flip ? right : x, top, flip ? x : right, top,
                                    flip ? x : right, bottom, flip ? right : x, bottom };
        ops->quad(ops->ctx, horz, points);
        mirrored ^= 1;
        x = x_sub(x, tile);
    }
}

void mirror_sky(const MirrorSkyOps *ops, const MirrorSkyInputs *in)
{
    const uint8_t property = ops->property(ops->ctx, x_sub(in->node, NODE_BEHIND));
    if (property == PROPERTY_ENCLOSED_A || property == PROPERTY_ENCLOSED_B || property == PROPERTY_ENCLOSED_C) {
        ops->fill(ops->ctx, in->enclosed_colour);
        return;
    }
    if (property == PROPERTY_8) {
        ops->fill(ops->ctx, in->property8_colour);
        return;
    }
    int32_t x = yaw_column(in->yaw);
    int32_t top = x_add(pitch_rows(in->pitch, in->high), x_add(in->view_h / 2, in->view_top));
    const uint8_t *horz = shape_bank_find(in->bank, (const uint8_t *)"horz");
    if (horz == NULL) return;  /* port guard: the original aborts */
    const int32_t w = s16(horz + SHAPE_WIDTH), h = s16(horz + SHAPE_HEIGHT);
    if (w == 0) return;  /* port guard: the original's divide faults */
    int32_t scale = SCALE_ONE / w;
    if (in->high == 0) scale /= 2, x /= 2;
    const int32_t tile = fx_mul_round(scale, w);
    const int32_t rows = fx_mul_round(scale, h - 1);
    top = x_sub(top, fx_mul_round(scale, s16(horz + SHAPE_HORIZON)));
    if (in->track == TRACK_LOW_STRIP) top = LOW_STRIP_TOP;
    ops->rect(ops->ctx, 0, 0, in->width, top, horz[SHAPE_PIXELS]);
    const int32_t bottom = x_add(rows, top);
    tiles(ops, in, horz, x, tile, top, bottom);
    ops->rect(ops->ctx, 0, bottom, in->width, x_add(x_sub(in->height, top), rows),
              horz[SHAPE_PIXELS + (int32_t)((uint32_t)w * (uint32_t)h) - 1]);
}
