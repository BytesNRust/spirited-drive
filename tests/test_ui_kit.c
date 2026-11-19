/* The UI kit's pager (ui_kit.h) against the measures taken on the CD's screens (ui_kit_metrics.h): the triangles' cream ink, the
 * gaps between them and the text, and the box they stay in. */
#include "game/frontend/ui_kit.h"
#include "game/sys/text_soft.h"
#include "test.h"

#include <string.h>

/* the cream a mask leaves once the text's look has darkened its bottom and right rim: the box of what is solid and not rim */
static void cream_box(const uint8_t *mask, int *x0, int *y0, int *x1, int *y1)
{
    *x0 = *y0 = 1 << 20, *x1 = *y1 = -1;
    for (int y = 0; y < UI_ARROW_MASK_H; y++)
        for (int x = 0; x < UI_ARROW_MASK_W; x++)
            if (mask[y * UI_ARROW_MASK_W + x] != 0 &&
                !text_soft_edge(mask, UI_ARROW_MASK_W, UI_ARROW_MASK_H, x, y)) {
                if (x < *x0) *x0 = x;
                if (x > *x1) *x1 = x;
                if (y < *y0) *y0 = y;
                if (y > *y1) *y1 = y;
            }
}

static void test_each_triangle_shows_the_cds_ink(void)
{
    for (int right = 0; right < 2; right++) {
        uint8_t mask[UI_ARROW_MASK_W * UI_ARROW_MASK_H];
        ui_arrow_mask(right, mask);
        int x0, y0, x1, y1;
        cream_box(mask, &x0, &y0, &x1, &y1);
        CHECK_EQ_INT(x1 - x0 + 1, UI_ARROW_INK_W);
        CHECK_EQ_INT(y1 - y0 + 1, UI_ARROW_INK_H);
        CHECK_EQ_INT(x0, UI_ARROW_PAD);   /* the layout counts the ink from the padding's end */
        CHECK_EQ_INT(y0, UI_ARROW_PAD + 1);   /* both start on the same row: the first under the triangle's own */
    }
}

static void test_the_triangles_point_outwards(void)
{
    uint8_t l[UI_ARROW_MASK_W * UI_ARROW_MASK_H], r[UI_ARROW_MASK_W * UI_ARROW_MASK_H];
    ui_arrow_mask(0, l), ui_arrow_mask(1, r);
    const int tip = UI_ARROW_TRI_H / 2, last = UI_ARROW_PAD + UI_ARROW_CORE_W - 1;
    CHECK(l[(UI_ARROW_PAD + tip) * UI_ARROW_MASK_W + UI_ARROW_PAD] != 0);   /* the left one's tip is in its first column, mid-height */
    CHECK_EQ_INT(l[UI_ARROW_PAD * UI_ARROW_MASK_W + UI_ARROW_PAD], 0);      /* and the first row only has its base column */
    CHECK(l[UI_ARROW_PAD * UI_ARROW_MASK_W + last] != 0);
    CHECK(r[(UI_ARROW_PAD + 1 + tip) * UI_ARROW_MASK_W + last] != 0);       /* the right one's, in its last, a row lower */
    CHECK(r[(UI_ARROW_PAD + 1) * UI_ARROW_MASK_W + UI_ARROW_PAD] != 0);
    CHECK_EQ_INT(r[(UI_ARROW_PAD + 1) * UI_ARROW_MASK_W + last], 0);
}

static void test_the_gaps_are_the_cds(void)
{
    const int text_w = 30;
    const UiPagerLayout l = ui_pager_layout(text_w, UI_ARROW_MASK_W);
    CHECK_EQ_INT(l.text_x, PAGE_NAV_CENTRE - text_w / 2);
    CHECK_EQ_INT(l.text_x - (l.left_x + UI_ARROW_PAD + UI_ARROW_INK_W), UI_PAGER_GAP_LEFT - UI_PAGER_TEXT_BEARING);
    /* ... which is the gap from the text's ink */
    CHECK_EQ_INT((l.right_x + UI_ARROW_PAD) - (l.text_x + text_w), UI_PAGER_GAP_RIGHT - UI_PAGER_TEXT_BEARING);
}

static void test_a_wide_label_is_not_given_triangles_on_its_text(void)
{
    CHECK(ui_pager_layout(30, UI_ARROW_MASK_W).fits);
    const UiPagerLayout l = ui_pager_layout(90, UI_ARROW_MASK_W);
    CHECK(!l.fits);   /* the box has no room for the triangles at the kit's gaps: the text form is drawn */
    const UiPagerLayout edge = ui_pager_layout(44, UI_ARROW_MASK_W);
    CHECK(edge.fits ? edge.left_x + UI_ARROW_PAD + UI_ARROW_INK_W <= edge.text_x : 1);
}

int main(void)
{
    test_each_triangle_shows_the_cds_ink();
    test_the_triangles_point_outwards();
    test_the_gaps_are_the_cds();
    test_a_wide_label_is_not_given_triangles_on_its_text();
    return TEST_REPORT();
}
