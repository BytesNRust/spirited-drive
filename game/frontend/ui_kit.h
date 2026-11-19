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

/* where a pager stands: its centre column, the count's line top (the digits' rows start 8 below it; the triangles' middle row is 13 below it, so
 * they are centred on the digits) and the box the triangles must stay inside. The screens' footers (320, UI_PAGER_TEXT_Y) are UI_PAGER_FOOT;
 * a list's pager (CONTENT's) gives its own. */
typedef struct {
    int centre, text_y, x0, x1;
} UiPagerPlace;
extern const UiPagerPlace UI_PAGER_FOOT;
UiPagerLayout ui_pager_layout_at(const UiPagerPlace *at, int text_w, int right_w);

/* a selector's triangles ("< name >" with a note under the name): for a block centred on `centre` whose widest line is `line_w` wide, each
 * triangle mask's left column. The ink is UI_PAGER_GAP_LEFT / RIGHT columns from the widest line's edge, as the pager's is from its text. */
typedef struct {
    int left_x, right_x;
} UiSelectorLayout;
UiSelectorLayout ui_selector_layout(int centre, int line_w);

/* E2c, text that must fit a fixed width. UI_FIT_CUT: the text cut with "..." (the showcase's names and values: added_card_name_fit's rule).
 * UI_FIT_SHRINK: the first of `sizes` (largest first) it fits at, else the smallest cut. `measure` is a run's width at num/den. 1 with the
 * text in `out` (`size` bytes) and the size in `used`; 0 when nothing is left to draw (an empty text, or not even "..." fits) */
typedef struct {
    int num, den;
} UiSize;
typedef enum { UI_FIT_CUT, UI_FIT_SHRINK } UiFitMode;
typedef int (*UiMeasure)(void *ctx, const char *text, int num, int den);
int ui_fit(const char *text, int max_w, UiFitMode mode, const UiSize *sizes, int count, UiMeasure measure, void *ctx, char *out, int size,
           UiSize *used);

/* the pager "< label >" of E4: a triangle on each side that has somewhere to go, the `lit` one yellow (page_nav_lit's answer: 1 the
 * left, 2 the right). Without triangles from the screen, the text "< label >". */
void ui_pager(const PageNavOps *o, const char *label, int has_left, int has_right, int lit);
void ui_pager_at(const PageNavOps *o, const UiPagerPlace *at, const char *label, int has_left, int has_right, int lit);

/* the pager of a screen's pages: ui_pager's "n/m" for page `page` (0-based) of `pages` */
void ui_page_count(const PageNavOps *o, int page, int pages, int lit);
void ui_page_count_at(const PageNavOps *o, const UiPagerPlace *at, int page, int pages, int lit);

/* the pager has no disabled state: a triangle with no page to go to is not drawn (the CD's pagers wrap, so every triangle they show works; ours
 * never show one that does not), at any size, on every screen */

#endif
