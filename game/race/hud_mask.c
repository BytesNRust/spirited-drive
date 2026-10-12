/* The view's mask (hud_mask.h). */
#include "game/race/hud_mask.h"

#include "game/sys/shape_bank.h"

#include <stddef.h>

enum {
    KIND_SPLIT = 3,
    SPLIT_FIRST_COLUMN = 2, SPLIT_SECOND_COLUMN = 3,
    PARTS = DASH_PART_ROWS * DASH_PART_COLUMNS
};

static void select_screen(const HudMaskOps *ops, const HudMaskInputs *in)
{
    ops->select(ops->ctx, in->back_buffer != 0 ? HUD_SURFACE_SCREEN : HUD_SURFACE_DEFAULT);
}

/* 00513c14 / 00513c44 + 16 row + 4 column. */
static const uint8_t *run_list(const DashArt *art, const HudMaskInputs *in, int32_t row, int32_t column)
{
    const int32_t k = row * DASH_PART_COLUMNS + column;
    if (in->use_parts != 0) return art->parts[k / DASH_PART_COLUMNS][k % DASH_PART_COLUMNS];
    return art->mmb + art->mmb_words[k / DASH_PART_COLUMNS][k % DASH_PART_COLUMNS];
}

static void copy(const HudMaskOps *ops, const DashArt *art, const HudMaskInputs *in, int32_t row, int32_t column)
{
    const int32_t k = row * DASH_PART_COLUMNS + column;
    if (k < 0 || k >= PARTS) return;
    ops->runs(ops->ctx, run_list(art, in, row, column));
}

void hud_mask_draw(const HudMaskOps *ops, const DashArt *art, HudState *hud, uint8_t *clear_view,
                   const HudMaskInputs *in, int32_t first)
{
    if (first != 0) {
        ops->select(ops->ctx, HUD_SURFACE_VIEW);
        uint8_t *tach = shape_bank_find(art->bank, (const uint8_t *)"tach");
        if (tach == NULL) return;
        ops->draw(ops->ctx, tach);
        select_screen(ops, in);
        hud->unknown_4f9a20[2] = 0;
        ops->view_clip(ops->ctx);
        ops->present(ops->ctx);
        return;
    }
    int32_t row = in->view - 1;
    if (row < 0) row = 0;
    if (in->screen_kind != KIND_SPLIT) {
        select_screen(ops, in);
        copy(ops, art, in, row, in->window_size);
        return;
    }
    if (*clear_view != 0) {
        ops->select(ops->ctx, HUD_SURFACE_VIEW);
        ops->clip(ops->ctx, 0, in->width, 0, in->height);
        ops->fill(ops->ctx, in->strip_colour);
        select_screen(ops, in);
        copy(ops, art, in, row, SPLIT_FIRST_COLUMN);
        *clear_view = 0;
    } else {
        select_screen(ops, in);
    }
    copy(ops, art, in, row, SPLIT_SECOND_COLUMN);
}
