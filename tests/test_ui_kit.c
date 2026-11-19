/* The UI kit's pager (ui_kit.h) against the measures taken on the CD's screens (ui_kit_metrics.h): the triangles' cream ink, the
 * gaps between them and the text, and the box they stay in. */
#include "game/frontend/ui_kit.h"
#include "game/sys/text_soft.h"
#include "test.h"

#include <string.h>

/* the box of a mask's solid pixels */
static void ink_box(const uint8_t *mask, int *x0, int *y0, int *x1, int *y1)
{
    *x0 = *y0 = 1 << 20, *x1 = *y1 = -1;
    for (int y = 0; y < UI_ARROW_MASK_H; y++)
        for (int x = 0; x < UI_ARROW_MASK_W; x++)
            if (mask[y * UI_ARROW_MASK_W + x] >= TEXT_SOFT_SOLID_FROM) {
                if (x < *x0) *x0 = x;
                if (x > *x1) *x1 = x;
                if (y < *y0) *y0 = y;
                if (y > *y1) *y1 = y;
            }
}

static void test_each_triangle_is_the_cds_size_with_room_for_its_outline_and_shadow(void)
{
    for (int right = 0; right < 2; right++) {
        uint8_t mask[UI_ARROW_MASK_W * UI_ARROW_MASK_H];
        ui_arrow_mask(right, mask);
        int x0, y0, x1, y1;
        ink_box(mask, &x0, &y0, &x1, &y1);
        CHECK_EQ_INT(x1 - x0 + 1, UI_ARROW_INK_W);
        CHECK_EQ_INT(y1 - y0 + 1, UI_ARROW_INK_H);
        CHECK_EQ_INT(x0, UI_ARROW_PAD);   /* the layout counts the ink from the padding's end */
        CHECK_EQ_INT(y0, UI_ARROW_PAD);
        CHECK(UI_ARROW_PAD >= 1 + TEXT_SOFT_BADGE_SHADOW_DEPTH);   /* the outline and the shadow fit in the padding */
    }
}

/* the CD's triangles' outline (measured on its track select): a column down the base two rows past the fill at each end and one pixel
 * along each slope, so the rows 2 above the fill to 2 under it hold 1, 2, 2 ... 2, 1 outline pixels; the slopes are not two thick */
static void test_the_outline_is_one_pixel_on_the_slopes_as_the_cds_is(void)
{
    for (int right = 0; right < 2; right++) {
        uint8_t mask[UI_ARROW_MASK_W * UI_ARROW_MASK_H];
        ui_arrow_mask(right, mask);
        for (int y = 0; y < UI_ARROW_MASK_H; y++) {
            int outline = 0;
            for (int x = 0; x < UI_ARROW_MASK_W; x++) outline += mask[y * UI_ARROW_MASK_W + x] == UI_ARROW_OUTLINE;
            const int first = UI_ARROW_PAD - 2, last = UI_ARROW_PAD + UI_ARROW_INK_H + 1;
            const int want = y < first || y > last ? 0 : y == first || y == last ? 1 : 2;
            CHECK_EQ_INT(outline, want);
        }
    }
}

static void test_the_triangles_point_outwards(void)
{
    uint8_t l[UI_ARROW_MASK_W * UI_ARROW_MASK_H], r[UI_ARROW_MASK_W * UI_ARROW_MASK_H];
    ui_arrow_mask(0, l), ui_arrow_mask(1, r);
    const int tip = UI_ARROW_PAD + UI_ARROW_INK_H / 2, top = UI_ARROW_PAD;
    const int first = UI_ARROW_PAD, last = UI_ARROW_PAD + UI_ARROW_INK_W - 1;
    CHECK(l[tip * UI_ARROW_MASK_W + first] != 0);   /* the left one's tip is in its first column, mid-height */
    CHECK_EQ_INT(l[top * UI_ARROW_MASK_W + first], 0);
    CHECK(l[top * UI_ARROW_MASK_W + last] != 0);    /* and the first row only has its base column */
    CHECK(r[tip * UI_ARROW_MASK_W + last] != 0);    /* the right one's, in its last */
    CHECK(r[top * UI_ARROW_MASK_W + first] != 0);
    CHECK_EQ_INT(r[top * UI_ARROW_MASK_W + last], 0);
}

static void test_the_gaps_are_the_cds(void)
{
    const int text_w = 30;
    const UiPagerLayout l = ui_pager_layout(text_w, UI_ARROW_MASK_W);
    CHECK_EQ_INT(l.text_x, PAGE_NAV_CENTRE - text_w / 2);
    CHECK_EQ_INT(l.text_x - (l.left_x + UI_ARROW_PAD + UI_ARROW_INK_W), UI_PAGER_GAP_LEFT - UI_PAGER_BEARING_LEFT);
    /* ... which is the gap from the text's ink */
    CHECK_EQ_INT((l.right_x + UI_ARROW_PAD) - (l.text_x + text_w), UI_PAGER_GAP_RIGHT - UI_PAGER_BEARING_RIGHT);
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
    test_each_triangle_is_the_cds_size_with_room_for_its_outline_and_shadow();
    test_the_outline_is_one_pixel_on_the_slopes_as_the_cds_is();
    test_the_triangles_point_outwards();
    test_the_gaps_are_the_cds();
    test_a_wide_label_is_not_given_triangles_on_its_text();
    return TEST_REPORT();
}
