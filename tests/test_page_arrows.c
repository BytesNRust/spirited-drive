/* The page indicator's triangles (page_arrows.h): the cut from a dim and a lit shape, what the indicator draws on a
 * first, a middle and a last page, which triangle is lit, the text when the CD gives none, and the CD's own. */
#include "game/frontend/page_arrows.h"
#include "formats/qfs.h"
#include "test.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CREAM = 0, YELLOW = 1 };

/* the shapes' palette: 0 the background, 1 black outline, 2 cream fill, 3 yellow fill */
static void palette(uint8_t pal[768])
{
    memset(pal, 0, 768);
    pal[3 * 0 + 0] = 48, pal[3 * 0 + 1] = 64, pal[3 * 0 + 2] = 80;
    pal[3 * 2 + 0] = 236, pal[3 * 2 + 1] = 212, pal[3 * 2 + 2] = 180;
    pal[3 * 3 + 0] = 252, pal[3 * 3 + 1] = 204, pal[3 * 3 + 2] = 0;
}

/* a 6 x 5 shape: a background, an outline (1) round a 2 x 3 fill */
static void shape(uint8_t *out, int fill)
{
    static const uint8_t ROWS[5][6] = { { 0, 0, 1, 1, 0, 0 }, { 0, 1, 9, 9, 1, 0 }, { 0, 1, 9, 9, 1, 0 },
                                         { 0, 1, 9, 9, 1, 0 }, { 0, 0, 1, 1, 0, 0 } };
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 6; x++) out[y * 6 + x] = ROWS[y][x] == 9 ? (uint8_t)fill : ROWS[y][x];
}

static void test_the_mask_is_the_fill_alone(void)
{
    uint8_t pal[768], dim[30], lit[30];
    palette(pal), shape(dim, 2), shape(lit, 3);
    uint8_t mask[PAGE_ARROW_MASK * PAGE_ARROW_MASK];
    memset(mask, 0xee, sizeof mask);
    CHECK_EQ_INT(page_arrows_mask(dim, lit, 6, 5, pal, mask), 1);
    const int w = 6 + 2 * PAGE_ARROW_PAD, h = 5 + 2 * PAGE_ARROW_PAD;
    int fill = 0, other = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const int inside = x >= PAGE_ARROW_PAD + 2 && x <= PAGE_ARROW_PAD + 3 && y >= PAGE_ARROW_PAD + 1 &&
                               y <= PAGE_ARROW_PAD + 3;
            const uint8_t v = mask[y * w + x];
            if (inside) fill += v == 15;
            else other += v != 0;   /* the outline, the background and the padding all cancel */
        }
    CHECK_EQ_INT(fill, 6);
    CHECK_EQ_INT(other, 0);
}

static void test_a_shape_too_big_is_refused(void)
{
    uint8_t pal[768], mask[PAGE_ARROW_MASK * PAGE_ARROW_MASK];
    static uint8_t big[(PAGE_ARROW_MAX + 1) * (PAGE_ARROW_MAX + 1)];
    palette(pal);
    CHECK_EQ_INT(page_arrows_mask(big, big, PAGE_ARROW_MAX + 1, PAGE_ARROW_MAX + 1, pal, mask), 0);
}

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
    *w = right ? 26 : 29, *h = 32;
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
    PageNavOps o = { f, f_text, f_width, f_in_rect, f_size, f_arrow };
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
    CHECK_EQ_INT(f.arrow_x[0], PAGE_NAV_X1 - 2 - 26);   /* flush with the box's right edge */
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
    CHECK_EQ_INT(f.arrow_x[0], PAGE_NAV_X0 + 2);
    CHECK_EQ_INT(f.arrow_right[1], 1);
    f.arrows = 0;
    page_nav_draw(&o, 2, 3, 0);
    CHECK_EQ_INT(f.arrows, 1);
    CHECK_EQ_INT(f.arrow_right[0], 0);
    CHECK(strcmp(f.text[f.texts - 1], "3/3") == 0);
}

static void test_the_text_is_centred_between_the_triangles(void)
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

static uint8_t *read_cd(const char *name, size_t *size)
{
    char path[512];
    int n = snprintf(path, sizeof path, "%s/original/FRONTEND/ART/", TNFS_SOURCE_DIR);
    for (const char *c = name; *c && n < (int)sizeof path - 1; c++) path[n++] = (char)toupper((unsigned char)*c);
    path[n] = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    static uint8_t raw[1 << 20];
    const size_t len = fread(raw, 1, sizeof raw, f);
    fclose(f);
    uint8_t *out = NULL;
    if (qfs_decompressed_size(raw, len, size) == QFS_OK && (out = malloc(*size)) != NULL &&
        qfs_decompress(raw, len, out, *size) != QFS_OK)
        free(out), out = NULL;
    return out;
}

/* with the CD at hand: the two triangles as the track select draws them, solid fills of 23 x 26 and 20 x 25 in their
 * shapes, each fill 17 rows tall; the masks keep the fill's rows and the padding only, so both stand at the same height */
static void test_the_cd_gives_the_triangles(void)
{
    static PageArrowMasks m;
    if (!page_arrows_load(read_cd, &m)) {
        fprintf(stderr, "test_page_arrows: no CD data, skipping the CD check\n");
        return;
    }
    CHECK_EQ_INT(m.w[PAGE_ARROW_LEFT], 23 + 2 * PAGE_ARROW_PAD);
    CHECK_EQ_INT(m.h[PAGE_ARROW_LEFT], 17 + 2 * PAGE_ARROW_PAD);
    CHECK_EQ_INT(m.w[PAGE_ARROW_RIGHT], 20 + 2 * PAGE_ARROW_PAD);
    CHECK_EQ_INT(m.h[PAGE_ARROW_RIGHT], 17 + 2 * PAGE_ARROW_PAD);
    for (int a = 0; a < PAGE_ARROWS; a++) {
        int solid = 0, edge = 0;
        const int w = m.w[a], h = m.h[a];
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                solid += m.mask[a][y * w + x] >= 14;
                edge += (x < PAGE_ARROW_PAD || x >= w - PAGE_ARROW_PAD || y < PAGE_ARROW_PAD || y >= h - PAGE_ARROW_PAD) &&
                        m.mask[a][y * w + x] != 0;
            }
        CHECK(solid > 80);        /* a triangle's fill, not an empty box */
        CHECK(solid < w * h / 2);
        CHECK_EQ_INT(edge, 0);    /* the padding stays empty: the outline is drawn by the text's own look */
        int first = -1;
        for (int y = 0; y < h && first < 0; y++)
            for (int x = 0; x < w; x++)
                if (m.mask[a][y * w + x] != 0) first = y;
        CHECK_EQ_INT(first, PAGE_ARROW_PAD);   /* the fill starts right under the padding */
    }
}

int main(void)
{
    test_the_mask_is_the_fill_alone();
    test_a_shape_too_big_is_refused();
    test_the_first_page_has_the_right_triangle_only();
    test_a_middle_page_has_both_the_last_the_left_only();
    test_the_text_is_centred_between_the_triangles();
    test_the_lit_triangle_is_the_one_asked();
    test_without_the_cd_it_is_the_text_it_was();
    test_the_pointer_lights_the_triangle_it_is_on();
    test_the_cd_gives_the_triangles();
    return TEST_REPORT();
}
