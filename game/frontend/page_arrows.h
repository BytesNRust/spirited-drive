/* port: the page indicator of the port's multi-page screens (CUSTOMIZE GAME, Controls setup, the records): the page count in
 * the middle and the original's solid triangles on its sides, cream when idle and yellow when the pointer is on them. What it
 * draws is the UI kit's pager (ui_kit.h); this is the box where the pointer is "on" it and the screens' slots it draws through. */
#ifndef TNFS_PAGE_ARROWS_H
#define TNFS_PAGE_ARROWS_H

#include <stdint.h>

/* the indicator's box, which is where the pointer is "on" it: the page screens' PLUS_NAV_*, EXTRAS_NAV_* and
 * CONTROLS_NAV_* */
enum {
    PAGE_NAV_X0 = 270, PAGE_NAV_X1 = 370, PAGE_NAV_Y0 = 432, PAGE_NAV_Y1 = 476,
    PAGE_NAV_CENTRE = 320,
    PAGE_NAV_TEXT_Y = 440,   /* the count's last row (456) on the background's DONE and CANCEL */
    PAGE_NAV_ARROW_MID = 452   /* the count's middle row (447..456): each triangle's mask is centred on it */
};

/* what the indicator draws and asks of its screen: the screens' own slots, which have these signatures */
typedef struct PageNavOps {
    void *ctx;
    void (*text)(void *ctx, int32_t style, const char *text, int32_t x, int32_t y);   /* style 0 cream, 1 yellow */
    int32_t (*text_width)(void *ctx, const char *text);
    int32_t (*in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);
    /* the triangle's mask size; 0 when there is none (NULL: none) */
    int32_t (*arrow_size)(void *ctx, int32_t right, int32_t *w, int32_t *h);
    void (*arrow)(void *ctx, int32_t right, int32_t lit, int32_t x, int32_t y);
} PageNavOps;

/* which triangle the pointer is on: 0 neither, 1 the left, 2 the right. On a page with both it is the half of the
 * box the pointer is in; on a page with one, that one, the whole box being its button. */
int page_nav_lit(const PageNavOps *o, int page, int pages);

/* The indicator of page `page` (0-based) of `pages`: "2/3" with a triangle on each side that has a page to go to,
 * the `lit` one yellow (page_nav_lit's answer). With no triangles to draw, the text "< 2/3 >". */
void page_nav_draw(const PageNavOps *o, int page, int pages, int lit);

#endif
