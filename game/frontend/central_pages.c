/* port: the page indicator's triangles of the Graphics, Controls and EXTRAS pages, moved out of central_glue.c */
#include "game/frontend/central_split.h"

#include "game/frontend/menu_glue.h"
#include "game/frontend/ui_kit.h"
#include "game/sys/text.h"

/* port: the page indicator's triangles, the kit's (ui_kit.h): the same mask on every screen, drawn in the look of the text */
int32_t central_page_arrow_size(void *ctx, int32_t right, int32_t *w, int32_t *h)
{
    (void)ctx, (void)right;
    *w = UI_ARROW_MASK_W, *h = UI_ARROW_MASK_H;
    return 1;
}
void central_page_arrow(void *ctx, int32_t right, int32_t lit, int32_t x, int32_t y)
{
    (void)ctx;
    static uint8_t mask[2][UI_ARROW_MASK_W * UI_ARROW_MASK_H];
    static int made;
    if (!made) ui_arrow_mask(0, mask[0]), ui_arrow_mask(1, mask[1]), made = 1;
    text_select(lit ? TEXT_YELLOW : TEXT_CREAM);
    text_draw_badge_mask(mask[right ? 1 : 0], UI_ARROW_MASK_W, UI_ARROW_MASK_H, x, y);
}
int32_t glue_plus_arrow_size(void *ctx, int32_t right, int32_t *w, int32_t *h) { return central_page_arrow_size(ctx, right, w, h); }
