/* The race screen's full redraw (hud_redraw.h). */
#include "game/race/hud_redraw.h"

#include "game/gfx/blit.h"
#include "game/gfx/clip.h"
#include "game/sys/shape_bank.h"

#include <stddef.h>

enum {
    TILE_NAMES = 4, TILE_TABLE = 30,    /* 004b94e0: '0000' '0001' '0002' '0003' over and over */
    TILES_TOP_LOW = 0xd, TILES_TOP_HIGH = 0x20,
    SHAPE_WIDTH = 4, SHAPE_HEIGHT = 6, SHAPE_HOT_Y = 0xa,
    BELOW_X_HIGH = 0x206, BELOW_W_HIGH = 0x74, BELOW_END_HIGH = 0x1bd,
    BELOW_X_LOW = 0xfe, BELOW_W_LOW = 0x50, BELOW_END_LOW = 0xb3,
    RIGHT_Y_HIGH = 0x130, RIGHT_W_HIGH = 0x74, RIGHT_H_HIGH = 0x84,
    RIGHT_Y_LOW = 0x75, RIGHT_W_LOW = 0x50, RIGHT_H_LOW = 0x3f,
    BOTTOM_PIECES = 5, SIDE_PIECES = 4
};

static const char TILE_NAME[TILE_NAMES][5] = { "0000", "0001", "0002", "0003" };

static int32_t s16(const uint8_t *b)
{
    return (int16_t)(uint16_t)(b[0] | b[1] << 8);
}

static uint8_t *find(const DashArt *art, const char *name)
{
    return shape_bank_find(art->bank, (const uint8_t *)name);
}

/* The tiles; 0 where the original faults. */
static int tiles(const QuadTarget *t, const DashArt *art, const HudRedrawInputs *in)
{
    const uint8_t *first = find(art, TILE_NAME[0]);
    if (first == NULL) return 0;
    const int32_t w = s16(first + SHAPE_WIDTH), h = s16(first + SHAPE_HEIGHT);
    if (w == 0 || (in->width == INT32_MIN && w == -1)) return 0;
    const int32_t top = in->high_screen != 0 ? TILES_TOP_HIGH : TILES_TOP_LOW;
    for (int32_t column = 0; column < in->width / w; column++) {
        if (h == 0 || (in->height == INT32_MIN && h == -1)) return 0;
        for (int32_t row = 0; row < in->height / h; row++) {
            const int32_t k = (column & 1) + 2 * row;
            if (k >= TILE_TABLE) return 0;
            const uint8_t *tile = find(art, TILE_NAME[k % TILE_NAMES]);
            if (tile == NULL) return 0;
            blit_shape_at(t, tile, (int32_t)((uint32_t)column * (uint32_t)w),
                          (int32_t)((uint32_t)top + (uint32_t)row * (uint32_t)h));
        }
    }
    return 1;
}

/* 0047380c with the shape found by name; 0 when it is missing. */
static int piece(const QuadTarget *t, const DashArt *art, const char *name, int32_t x, int32_t y)
{
    const uint8_t *shape = find(art, name);
    if (shape == NULL) return 0;
    blit_shape_centred(t, shape, x, y);
    return 1;
}

static int frame(const QuadTarget *t, const DashArt *art, const HudFrame *f)
{
    const uint8_t *top_piece = find(art, "b00t"), *left_piece = find(art, "b00l");
    if (top_piece == NULL || left_piece == NULL) return 0;
    const int32_t step_x = s16(top_piece + SHAPE_WIDTH), step_y = s16(left_piece + SHAPE_HEIGHT);
    const int32_t left = f->left, top = f->top;
    const int32_t right = f->width + left - 1, bottom = top + f->height - 1;
    if ((step_x <= 0 && left < right) || (step_y <= 0 && top < bottom - step_y)) return 0;  /* runs on */
    for (int32_t x = left; x < right; x += step_x)
        if (!piece(t, art, "b00t", x, top) || !piece(t, art, "b00b", x, bottom)) return 0;
    for (int32_t y = top; y < bottom - step_y; y += step_y)
        if (!piece(t, art, "b00l", left, y) || !piece(t, art, "b00r", right, y)) return 0;
    const int32_t last = bottom + 1 - step_y;
    const uint8_t *right_piece = find(art, "b00r");
    if (right_piece == NULL) return 0;
    return piece(t, art, "b00l", left, last + s16(left_piece + SHAPE_HOT_Y))
        && piece(t, art, "b00r", right, last + s16(right_piece + SHAPE_HOT_Y))
        && piece(t, art, "b000", left, top) && piece(t, art, "b001", right, top)
        && piece(t, art, "b002", right, bottom) && piece(t, art, "b003", left, bottom);
}

void hud_redraw(const QuadTarget *t, HudState *hud, const DashArt *art, const RaceState *race,
                const HudRedrawInputs *in)
{
    if (!tiles(t, art, in)) return;
    hud_draw_labels(t, hud, art, race, in->gap, in->gap_ticks, in->high_res, in->strip_colour);
    if (!frame(t, art, &in->frame)) return;
    const uint8_t *tach = find(art, "tach");
    if (tach == NULL) return;
    blit_shape_placed_transparent(t, tach);
    hud_reset(hud);
}

/* The tiles from column `column0` and row `row0` on; 0 where the original
 * faults. */
static int tile_quarter(const QuadTarget *t, const DashArt *art, const HudPartsInputs *in, int32_t w, int32_t h,
                        int32_t column0, int32_t row0)
{
    const int32_t top = in->high_screen != 0 ? TILES_TOP_HIGH : TILES_TOP_LOW;
    for (int32_t column = column0; column < in->width / w; column++) {
        for (int32_t row = row0; row < in->height / h; row++) {
            const int32_t k = (column & 1) + 2 * row;
            if (k < 0 || k >= TILE_TABLE) return 0;
            const uint8_t *tile = find(art, TILE_NAME[k % TILE_NAMES]);
            if (tile == NULL) return 0;
            blit_shape_at(t, tile, (int32_t)((uint32_t)column * (uint32_t)w),
                          (int32_t)((uint32_t)top + (uint32_t)row * (uint32_t)h));
        }
    }
    return 1;
}

void hud_parts_redraw(QuadTarget *t, const DashArt *art, const HudPartsInputs *in)
{
    const uint8_t *first = find(art, TILE_NAME[0]);
    if (first == NULL) return;
    const int32_t w = s16(first + SHAPE_WIDTH), h = s16(first + SHAPE_HEIGHT);
    if (w == 0 || h == 0 || (in->width == INT32_MIN && w == -1) || (in->height == INT32_MIN && h == -1)) return;
    const int32_t column0 = in->width / w / 2, row0 = in->height / h / 2;
    const HudFrame *f = &in->frame;
    const int32_t below = f->top + f->height;
    if (in->high_res != 0)
        clip_box(t, BELOW_X_HIGH, below + 2, BELOW_W_HIGH, BELOW_END_HIGH - f->top - f->height, in->surface_w,
                 in->surface_h);
    else
        clip_box(t, BELOW_X_LOW, below + 1, BELOW_W_LOW, BELOW_END_LOW - f->top - f->height, in->surface_w,
                 in->surface_h);
    if (!tile_quarter(t, art, in, w, h, column0, row0)) return;
    const int32_t beside = f->left + f->width;
    if (in->high_res != 0)
        clip_box(t, beside + 2, RIGHT_Y_HIGH, RIGHT_W_HIGH, RIGHT_H_HIGH, in->surface_w, in->surface_h);
    else
        clip_box(t, beside + 1, RIGHT_Y_LOW, RIGHT_W_LOW, RIGHT_H_LOW, in->surface_w, in->surface_h);
    if (!tile_quarter(t, art, in, w, h, column0, row0)) return;
    if (in->high_res != 0)
        clip_screen(t, in->width, in->height, in->surface_w, in->surface_h);
    else
        clip_box(t, BELOW_X_LOW, RIGHT_Y_LOW, RIGHT_W_LOW, RIGHT_H_LOW, in->surface_w, in->surface_h);
    const uint8_t *top_piece = find(art, "b00t"), *left_piece = find(art, "b00l");
    if (top_piece == NULL || left_piece == NULL) return;
    const int32_t step_x = s16(top_piece + SHAPE_WIDTH), step_y = s16(left_piece + SHAPE_HEIGHT);
    const int32_t right = beside - 1, bottom = below - 1;
    const int32_t x0 = beside - BOTTOM_PIECES * step_x, y0 = below - SIDE_PIECES * step_y - 1;
    if ((step_x <= 0 && x0 < right) || (step_y <= 0 && y0 <= bottom - step_y)) return;  /* runs on */
    for (int32_t x = x0; x < right; x += step_x)
        if (!piece(t, art, "b00b", x, bottom)) return;
    for (int32_t y = y0; y <= bottom - step_y; y += step_y)
        if (!piece(t, art, "b00r", right, y + 1)) return;
    piece(t, art, "b002", right, bottom);
}
