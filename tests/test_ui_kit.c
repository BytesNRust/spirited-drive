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

/* a fake face: `px` pixels a character at 1/1, scaled by num/den */
static int px_per_char = 10;
static int fake_width(void *ctx, const char *text, int num, int den)
{
    (void)ctx;
    return (int)strlen(text) * px_per_char * num / den;
}

static const UiSize SIZES[] = { { 1, 1 }, { 5, 6 }, { 3, 4 } };

static void test_the_selector_triangles_keep_the_cds_gaps_from_the_widest_line(void)
{
    const UiSelectorLayout l = ui_selector_layout(400, 200);
    CHECK_EQ_INT(l.left_x + UI_ARROW_PAD + UI_ARROW_INK_W + UI_PAGER_GAP_LEFT, 400 - 100);    /* the left ink ends 14 columns before the line */
    CHECK_EQ_INT(l.right_x + UI_ARROW_PAD, 400 + 100 + UI_PAGER_GAP_RIGHT);                    /* the right ink starts 15 after it */
    const UiSelectorLayout wide = ui_selector_layout(400, 230);   /* the widest line the kit allows */
    CHECK(wide.left_x < l.left_x && wide.right_x > l.right_x);   /* a wider line pushes both outwards, the block stays centred */
    CHECK_EQ_INT(400 - (wide.left_x + UI_ARROW_PAD + UI_ARROW_INK_W), wide.right_x + UI_ARROW_PAD - 400 - 1);
}

static void test_a_name_that_fits_is_left_alone(void)
{
    char out[64];
    UiSize used = { 0, 0 };
    px_per_char = 10;
    CHECK(ui_fit("Restomod '26", UI_SELECTOR_TEXT_W, UI_FIT_SHRINK, SIZES, 3, fake_width, NULL, out, sizeof out, &used));
    CHECK(strcmp(out, "Restomod '26") == 0);
    CHECK(used.num == 1 && used.den == 1);
}

static void test_a_wide_name_shrinks_to_the_first_size_it_fits_at(void)
{
    char out[64];
    UiSize used = { 0, 0 };
    px_per_char = 20;   /* 12 characters: 240 at 1/1, 200 at 5/6 */
    CHECK(ui_fit("WWWWWWWWWWWW", UI_SELECTOR_TEXT_W, UI_FIT_SHRINK, SIZES, 3, fake_width, NULL, out, sizeof out, &used));
    CHECK(strcmp(out, "WWWWWWWWWWWW") == 0);
    CHECK(used.num == 5 && used.den == 6);
}

static void test_a_name_too_wide_even_at_the_smallest_size_is_cut(void)
{
    char out[64];
    UiSize used = { 0, 0 };
    px_per_char = 40;   /* 12 characters: 480 at 1/1, 360 at 3/4 */
    CHECK(ui_fit("WWWWWWWWWWWW", UI_SELECTOR_TEXT_W, UI_FIT_SHRINK, SIZES, 3, fake_width, NULL, out, sizeof out, &used));
    CHECK(used.num == 3 && used.den == 4);
    CHECK(strstr(out, "...") != NULL && fake_width(NULL, out, 3, 4) <= UI_SELECTOR_TEXT_W);
    CHECK(strlen(out) < 12 + 3);
}

static void test_the_cut_mode_never_shrinks(void)
{
    char out[64];
    UiSize used = { 0, 0 };
    px_per_char = 20;
    CHECK(ui_fit("Read only. Duplicate to edit.", UI_SELECTOR_TEXT_W, UI_FIT_CUT, SIZES, 3, fake_width, NULL, out, sizeof out, &used));
    CHECK(used.num == 1 && used.den == 1);   /* only the first size is tried */
    CHECK(strstr(out, "...") != NULL && fake_width(NULL, out, 1, 1) <= UI_SELECTOR_TEXT_W);
}

static void test_an_empty_text_draws_nothing(void)
{
    char out[64] = "x";
    UiSize used = { 0, 0 };
    CHECK(!ui_fit("", UI_SELECTOR_TEXT_W, UI_FIT_SHRINK, SIZES, 3, fake_width, NULL, out, sizeof out, &used));
    CHECK(out[0] == 0);
    CHECK(!ui_fit("   ", UI_SELECTOR_TEXT_W, UI_FIT_CUT, SIZES, 3, fake_width, NULL, out, sizeof out, &used));
}

/* the longest name an edition may have: EDITION_NAME_MAX, 12 (core/formats/edition_ini.h:21), is always drawn, cut or not */
static void test_the_longest_edition_name_is_drawn(void)
{
    char out[64];
    UiSize used = { 0, 0 };
    for (px_per_char = 8; px_per_char <= 60; px_per_char += 4)
        CHECK(ui_fit("MMMMMMMMMMMM", UI_SELECTOR_TEXT_W, UI_FIT_SHRINK, SIZES, 3, fake_width, NULL, out, sizeof out, &used) &&
              fake_width(NULL, out, used.num, used.den) <= UI_SELECTOR_TEXT_W);
}

/* the foot rule: the left ink at the margin, the right ink ending the same from the right edge, the group centred between them with equal gaps */
static void test_the_foot_has_its_margins_and_a_centred_group(void)
{
    const UiFootWord left = { 50, 2, 3 }, right = { 80, 1, 4 }, mid[3] = { { 30, 1, 2 }, { 60, 2, 1 }, { 40, 0, 3 } };
    const UiFootLayout l = ui_foot_layout(left, right, mid, 3, 18, 10, 640);
    CHECK_EQ_INT(l.left_x + left.lead, 10);                       /* the left ink starts at the margin */
    CHECK_EQ_INT(l.right_x + right.w - right.trail, 640 - 10);    /* the right ink ends the same from the right */
    int ink_l[3], ink_r[3];
    for (int i = 0; i < 3; i++) ink_l[i] = l.mid_x[i] + mid[i].lead, ink_r[i] = l.mid_x[i] + mid[i].w - mid[i].trail;
    CHECK_EQ_INT(ink_l[1] - ink_r[0], 18);
    CHECK_EQ_INT(ink_l[2] - ink_r[1], 18);
    const int before = ink_l[0] - (l.left_x + left.w - left.trail), after = (l.right_x + right.lead) - ink_r[2];
    CHECK(before - after >= -1 && before - after <= 1);          /* centred between the two inks */
    const UiFootLayout tight = ui_foot_layout(left, right, mid, 3, 400, 10, 640);   /* a group that does not fit starts at the left item's ink end */
    CHECK_EQ_INT(tight.mid_x[0] + mid[0].lead, 10 + left.w - left.lead - left.trail);
}

int main(void)
{
    test_the_foot_has_its_margins_and_a_centred_group();
    test_the_selector_triangles_keep_the_cds_gaps_from_the_widest_line();
    test_a_name_that_fits_is_left_alone();
    test_a_wide_name_shrinks_to_the_first_size_it_fits_at();
    test_a_name_too_wide_even_at_the_smallest_size_is_cut();
    test_the_cut_mode_never_shrinks();
    test_an_empty_text_draws_nothing();
    test_the_longest_edition_name_is_drawn();
    test_each_triangle_is_the_cds_size_with_room_for_its_outline_and_shadow();
    test_the_outline_is_one_pixel_on_the_slopes_as_the_cds_is();
    test_the_triangles_point_outwards();
    test_the_gaps_are_the_cds();
    test_a_wide_label_is_not_given_triangles_on_its_text();
    return TEST_REPORT();
}
