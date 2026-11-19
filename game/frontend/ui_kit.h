/* port: the menu UI kit (docs/ui-kit.md). A screen of the front end draws a menu element only through these functions; the measures
 * they draw with are ui_kit_metrics.h's. Here so far: the pager of E4. */
#ifndef TNFS_UI_KIT_H
#define TNFS_UI_KIT_H

#include "game/frontend/page_arrows.h"
#include "game/frontend/ui_kit_metrics.h"

#include <stdint.h>

/* a triangle's coverage mask (0..15) of UI_ARROW_MASK_W x UI_ARROW_MASK_H, the one pointing right when `right` */
void ui_arrow_mask(int right, uint8_t mask[UI_ARROW_MASK_W * UI_ARROW_MASK_H]);

/* where a pager's parts go: the text's left column and each triangle mask's left column, for a text `text_w` wide centred on
 * PAGE_NAV_CENTRE, the right triangle mask `right_w` wide. The triangles keep the kit's gap from the text; `fits` is 0 when that puts one
 * outside the pointer's box (a label too wide for it), and ui_pager then draws "< label >" as text. */
typedef struct {
    int text_x, left_x, right_x, fits;
} UiPagerLayout;
UiPagerLayout ui_pager_layout(int text_w, int right_w);

/* the pager "< label >" of E4: a triangle on each side that has somewhere to go, the `lit` one yellow (page_nav_lit's answer: 1 the
 * left, 2 the right). Without triangles from the screen, the text "< label >". */
void ui_pager(const PageNavOps *o, const char *label, int has_left, int has_right, int lit);

/* the pager of a screen's pages: ui_pager's "n/m" for page `page` (0-based) of `pages` */
void ui_page_count(const PageNavOps *o, int page, int pages, int lit);

#endif
