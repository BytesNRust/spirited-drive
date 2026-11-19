/* The page indicator (page_arrows.h) against a fake screen: what it draws on a first, a middle and a last page, which triangle is
 * lit, the text when the screen has no triangles, and where the pointer lights them. The kit's own measures are test_ui_kit's. */
#include "game/frontend/page_arrows.h"
#include "game/frontend/ui_kit.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

enum { CREAM = 0, YELLOW = 1 };

/* the indicator against a fake screen: what it drew, in order */
typedef struct {
    int texts, arrows;
    char text[4][16];
    int text_x[4], text_y[4], text_style[4];
    int arrow_right[4], arrow_lit[4], arrow_x[4], arrow_y[4];
    int has_arrows;           /* arrow_size answers */
    int pointer_x, pointer_y; /* where the pointer is */
} Fake;

static void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    Fake *f = ctx;
    snprintf(f->text[f->texts], sizeof f->text[0], "%s", text);
    f->text_x[f->texts] = x, f->text_y[f->texts] = y, f->text_style[f->texts++] = style;
}
static int32_t f_width(void *ctx, const char *text) { (void)ctx; return (int32_t)strlen(text) * 10; }
static int32_t f_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    const Fake *f = ctx;
    return f->pointer_x >= x0 && f->pointer_x < x1 && f->pointer_y >= y0 && f->pointer_y < y1;
}
static int32_t f_size(void *ctx, int32_t right, int32_t *w, int32_t *h)
{
    const Fake *f = ctx;
    if (!f->has_arrows) return 0;
    (void)right, *w = 16, *h = 32;
    return 1;
}
static void f_arrow(void *ctx, int32_t right, int32_t lit, int32_t x, int32_t y)
{
    Fake *f = ctx;
    f->arrow_right[f->arrows] = right, f->arrow_lit[f->arrows] = lit, f->arrow_x[f->arrows] = x;
    f->arrow_y[f->arrows++] = y;
}

static PageNavOps ops(Fake *f)
{
    PageNavOps o = { f, f_text, f_width, f_in_rect, f_size, f_arrow, { NULL, NULL, NULL }, 0 };
    return o;
}

static void test_the_first_page_has_the_right_triangle_only(void)
{
    Fake f = { .has_arrows = 1 };
    const PageNavOps o = ops(&f);
    page_nav_draw(&o, 0, 3, 0);
    CHECK_EQ_INT(f.arrows, 1);
    CHECK_EQ_INT(f.arrow_right[0], 1);
    CHECK_EQ_INT(f.arrow_lit[0], 0);
    CHECK_EQ_INT(f.texts, 1);
    CHECK(strcmp(f.text[0], "1/3") == 0);   /* the count, no chevron in it */
    CHECK_EQ_INT(f.text_style[0], CREAM);
    CHECK_EQ_INT(f.arrow_x[0], ui_pager_layout(30, 16).right_x);   /* the kit's gap from the "1/3" (3 x 10 wide) */
    CHECK_EQ_INT(f.arrow_y[0], PAGE_NAV_ARROW_MID - 32 / 2);   /* the mask's middle on the count's */
    CHECK_EQ_INT(f.text_y[0], PAGE_NAV_TEXT_Y);
    CHECK_EQ_INT(PAGE_NAV_TEXT_Y, 440);   /* the count's last row on DONE's and CANCEL's (456) */
}

static void test_a_middle_page_has_both_the_last_the_left_only(void)
{
    Fake f = { .has_arrows = 1 };
    const PageNavOps o = ops(&f);
    page_nav_draw(&o, 1, 3, 0);
    CHECK_EQ_INT(f.arrows, 2);
    CHECK_EQ_INT(f.arrow_right[0], 0);
    CHECK_EQ_INT(f.arrow_x[0], ui_pager_layout(30, 16).left_x);   /* the kit's gap from the "2/3" (3 x 10 wide) */
    CHECK_EQ_INT(f.arrow_right[1], 1);
    f.arrows = 0;
    page_nav_draw(&o, 2, 3, 0);
    CHECK_EQ_INT(f.arrows, 1);
    CHECK_EQ_INT(f.arrow_right[0], 0);
    CHECK(strcmp(f.text[f.texts - 1], "3/3") == 0);
}

static void test_a_count_too_wide_for_the_box_is_the_text(void)
{
    Fake f = { .has_arrows = 1 };
    const PageNavOps o = ops(&f);
    page_nav_draw(&o, 99, 120, 0);   /* "100/120": 70 wide here, with the kit's gaps the triangles would leave the box */
    CHECK_EQ_INT(f.arrows, 0);
    CHECK(strcmp(f.text[f.texts - 1], "< 100/120 >") == 0);
}

/* a screen that can draw the count at the buttons' size: where it draws it, which of its two draws, and the width it lays out by */
static int scaled_plain, scaled_clean, scaled_num, scaled_den, scaled_y;
static void f_plain(void *ctx, int32_t style, const char *text, int32_t x, int32_t y, int32_t num, int32_t den)
{
    (void)style, (void)text, (void)x;
    Fake *f = ctx;
    f->texts++, scaled_plain++, scaled_num = num, scaled_den = den, scaled_y = y;
}
static void f_clean(void *ctx, int32_t style, const char *text, int32_t x, int32_t y, int32_t num, int32_t den)
{
    f_plain(ctx, style, text, x, y, num, den);
    scaled_plain--, scaled_clean++;
}
static int32_t f_scaled_width(void *ctx, const char *text, int32_t num, int32_t den)
{
    (void)ctx, (void)num, (void)den;
    return (int32_t)strlen(text) * 8;
}

static void test_the_count_is_drawn_at_the_buttons_size_where_the_screen_can(void)
{
    Fake f = { .has_arrows = 1 };
    PageNavOps o = ops(&f);
    o.scaled.plain = f_plain, o.scaled.clean = f_clean, o.scaled.width = f_scaled_width;
    page_nav_draw(&o, 1, 3, 0);
    CHECK_EQ_INT(scaled_plain, 1);
    CHECK_EQ_INT(scaled_clean, 0);
    CHECK_EQ_INT(scaled_num, UI_PAGER_TEXT_NUM);
    CHECK_EQ_INT(scaled_den, UI_PAGER_TEXT_DEN);
    CHECK_EQ_INT(scaled_y, UI_PAGER_TEXT_Y);
    CHECK_EQ_INT(f.arrow_x[0], ui_pager_layout(24, 16).left_x);   /* laid out by the width at that size */
    o.clean = 1;
    page_nav_draw(&o, 1, 3, 0);
    CHECK_EQ_INT(scaled_clean, 1);   /* over a clean background: that screen's own draw */
    o.scaled.clean = NULL;
    f.texts = 0;
    page_nav_draw(&o, 1, 3, 0);
    CHECK_EQ_INT(f.texts, 1);   /* a screen with no draw for it: the 1/1 text slot */
    CHECK_EQ_INT(f.text_y[0], PAGE_NAV_TEXT_Y);
}

static void test_the_text_is_centred_on_the_box(void)
{
    Fake f = { .has_arrows = 1 };
    const PageNavOps o = ops(&f);
    page_nav_draw(&o, 0, 2, 0);
    CHECK_EQ_INT(f.text_x[0], PAGE_NAV_CENTRE - 30 / 2);
}

static void test_the_lit_triangle_is_the_one_asked(void)
{
    Fake f = { .has_arrows = 1 };
    const PageNavOps o = ops(&f);
    page_nav_draw(&o, 1, 3, 1);
    CHECK_EQ_INT(f.arrow_lit[0], 1);   /* the left */
    CHECK_EQ_INT(f.arrow_lit[1], 0);
    f.arrows = 0;
    page_nav_draw(&o, 1, 3, 2);
    CHECK_EQ_INT(f.arrow_lit[0], 0);
    CHECK_EQ_INT(f.arrow_lit[1], 1);   /* the right */
}

static void test_without_the_cd_it_is_the_text_it_was(void)
{
    Fake f = { .has_arrows = 0 };
    const PageNavOps o = ops(&f);
    page_nav_draw(&o, 0, 2, 0);
    CHECK_EQ_INT(f.arrows, 0);
    CHECK(strcmp(f.text[0], "1/2 >") == 0);
    page_nav_draw(&o, 1, 2, 0);
    CHECK(strcmp(f.text[1], "< 2/2") == 0);
    page_nav_draw(&o, 1, 3, 0);
    CHECK(strcmp(f.text[2], "< 2/3 >") == 0);
    CHECK_EQ_INT(f.text_style[2], CREAM);
    PageNavOps none = ops(&f);
    none.arrow = NULL, none.arrow_size = NULL;   /* a screen whose ops have no slot for them */
    f.texts = 0;
    page_nav_draw(&none, 0, 2, 0);
    CHECK(strcmp(f.text[0], "1/2 >") == 0);
}

static void test_the_pointer_lights_the_triangle_it_is_on(void)
{
    Fake f = { .has_arrows = 1 };
    const PageNavOps o = ops(&f);
    f.pointer_x = 100, f.pointer_y = 100;
    CHECK_EQ_INT(page_nav_lit(&o, 1, 3), 0);   /* away from the box */
    f.pointer_y = (PAGE_NAV_Y0 + PAGE_NAV_Y1) / 2;
    f.pointer_x = PAGE_NAV_X0 + 5;
    CHECK_EQ_INT(page_nav_lit(&o, 1, 3), 1);   /* the left half of a page with both */
    f.pointer_x = PAGE_NAV_X1 - 5;
    CHECK_EQ_INT(page_nav_lit(&o, 1, 3), 2);
    f.pointer_x = PAGE_NAV_X0 + 5;
    CHECK_EQ_INT(page_nav_lit(&o, 0, 2), 2);   /* a page with one: the box is its button, wherever in it */
    CHECK_EQ_INT(page_nav_lit(&o, 1, 2), 1);
    f.pointer_x = PAGE_NAV_X1 - 5;
    CHECK_EQ_INT(page_nav_lit(&o, 1, 2), 1);
}

int main(void)
{
    test_the_first_page_has_the_right_triangle_only();
    test_a_middle_page_has_both_the_last_the_left_only();
    test_the_text_is_centred_on_the_box();
    test_the_count_is_drawn_at_the_buttons_size_where_the_screen_can();
    test_a_count_too_wide_for_the_box_is_the_text();
    test_the_lit_triangle_is_the_one_asked();
    test_without_the_cd_it_is_the_text_it_was();
    test_the_pointer_lights_the_triangle_it_is_on();
    return TEST_REPORT();
}
