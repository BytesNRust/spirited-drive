/* port: the page indicator's triangles of the Graphics, Controls and EXTRAS pages, moved out of central_glue.c */
#include "game/frontend/central_split.h"

#include "game/frontend/menu_glue.h"
#include "game/frontend/page_arrows.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"

/* port: the page indicator's triangles (page_arrows.h), cut from the CD the first time and kept; none when the CD
 * cannot give them, and the indicator is its text */
int32_t central_page_arrow_size(void *ctx, int32_t right, int32_t *w, int32_t *h)
{
    (void)ctx;
    const PageArrowMasks *const m = page_arrows_shared(resource_load_art);
    if (m == NULL) return 0;
    *w = m->w[right ? PAGE_ARROW_RIGHT : PAGE_ARROW_LEFT], *h = m->h[right ? PAGE_ARROW_RIGHT : PAGE_ARROW_LEFT];
    return 1;
}
void central_page_arrow(void *ctx, int32_t right, int32_t lit, int32_t x, int32_t y)
{
    (void)ctx;
    const PageArrowMasks *const m = page_arrows_shared(resource_load_art);
    const int a = right ? PAGE_ARROW_RIGHT : PAGE_ARROW_LEFT;
    if (m == NULL) return;
    text_select(lit ? TEXT_YELLOW : TEXT_CREAM);
    text_draw_item_mask(m->mask[a], m->w[a], m->h[a], x, y);
}
int32_t glue_plus_arrow_size(void *ctx, int32_t right, int32_t *w, int32_t *h) { return central_page_arrow_size(ctx, right, w, h); }
