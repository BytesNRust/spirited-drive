/* port: the page indicator of the port's multi-page screens (CUSTOMIZE GAME, Controls setup, Graphics): the page
 * count in the middle and the original's solid triangles on its sides, the ones the track select and Best Times
 * footers carry ("larl"/"lard", "rarl"/"rard"). No EA pixels live in the repository: the triangles are cut from the
 * CD at run time as a hue-independent coverage mask (item_art.h's, 0..15) -- the dim and the lit shape differ only in
 * their fill, cream and yellow, so the pair's difference is the fill alone -- and drawn in the port's own look, cream
 * when idle and yellow when the pointer is on them. Where the CD cannot give them the indicator is the text it was. */
#ifndef TNFS_PAGE_ARROWS_H
#define TNFS_PAGE_ARROWS_H

#include "game/frontend/config_bgnd.h"

#include <stdint.h>

enum {
    PAGE_ARROW_PAD = 3,   /* empty rows and columns round the fill in the mask: the outline and the shadow's room */
    PAGE_ARROW_MAX = 32,  /* the biggest shape cut */
    PAGE_ARROW_MASK = PAGE_ARROW_MAX + 2 * PAGE_ARROW_PAD
};

typedef enum { PAGE_ARROW_LEFT, PAGE_ARROW_RIGHT, PAGE_ARROWS } PageArrow;

typedef struct {
    uint8_t mask[PAGE_ARROWS][PAGE_ARROW_MASK * PAGE_ARROW_MASK];   /* w x h each, packed */
    int w[PAGE_ARROWS], h[PAGE_ARROWS];                              /* the masks' sizes, the padding in */
} PageArrowMasks;

/* A dim and a lit shape of one triangle (w x h palette indices into `palette`) as a coverage mask, the padding
 * round it: (w + 2 PAGE_ARROW_PAD) x (h + 2 PAGE_ARROW_PAD) into `mask`. 1 on success, 0 when the shape is bigger than
 * PAGE_ARROW_MAX. */
int page_arrows_mask(const uint8_t *dim, const uint8_t *lit, int w, int h, const uint8_t palette[768], uint8_t *mask);

/* Reads the track select art through `read` and cuts both triangles into `m`. 1 on success. */
int page_arrows_load(ConfigBgndRead read, PageArrowMasks *m);

/* The cut of the game's own reader, made once and kept; NULL when the CD cannot give it. */
const PageArrowMasks *page_arrows_shared(ConfigBgndRead read);

/* the indicator's box, which is where the pointer is "on" it: the page screens' PLUS_NAV_*, EXTRAS_NAV_* and
 * CONTROLS_NAV_* */
enum {
    PAGE_NAV_X0 = 270, PAGE_NAV_X1 = 370, PAGE_NAV_Y0 = 432, PAGE_NAV_Y1 = 476,
    PAGE_NAV_CENTRE = 320, PAGE_NAV_TEXT_Y = 444,
    PAGE_NAV_ARROW_Y = 442   /* the masks' top row: the 26 rows of the triangle stand on the text's middle */
};

/* the two triangles' left columns: the left one's at the box's left edge, the right one's flush with its right */
static inline int page_nav_arrow_x(PageArrow which, int mask_w)
{
    return which == PAGE_ARROW_LEFT ? PAGE_NAV_X0 + 2 : PAGE_NAV_X1 - 2 - mask_w;
}

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
