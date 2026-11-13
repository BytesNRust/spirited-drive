/* The nose under the wipers (dash_nose.h, dep49), on dashes drawn for the test: a hood with a vertical gradient (one grey a row) under a windscreen, and a blade across it.
 * A red pixel on the glass stays the glass's; a red pixel on the nose is filled from the nose round it, top, bottom and sides, so it carries on the hood's gradient whatever
 * the blade's slope (diagonal, nearly flat); the dash's art next to the blade is never read; a blade along the hood's edge is split at the edge; with no cyan nothing is
 * made. The cache makes the nose again when the art, the mask, the palette (the body colour), the settings' generation or the wipers change (the change of mode with no
 * restart, through the settings themselves, is test_live_mode_switch's). */
#include "game/race/dash_mask.h"
#include "game/race/dash_nose.h"
#include "game/platform/platform.h"
#include "test.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* dash_nose_key reads the settings: no files here */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, *size = 0; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size) { (void)name, (void)data, (void)size; return false; }
void platform_free_file(uint8_t *data) { free(data); }

enum { W = 48, H = 32, EDGE = 14, KEY = 0xff, RAMP = 20, HOOD = 10, INTERIOR = 50, BLADE_ART = 60 };   /* rows < EDGE are the glass, rows >= EDGE the hood */

static uint8_t palette[768];

static void palette_make(int body_blue)
{
    memset(palette, 0, sizeof palette);
    for (int j = 0; j < RAMP; j++)   /* the hood's ramp: a grey per row, (body_blue: a bluish body) */
        palette[3 * (HOOD + j)] = (uint8_t)(j * 12), palette[3 * (HOOD + j) + 1] = (uint8_t)(j * 12), palette[3 * (HOOD + j) + 2] = (uint8_t)(j * 12 + (body_blue ? 15 : 0));
    palette[3 * INTERIOR] = 255, palette[3 * INTERIOR + 2] = 255;   /* the dash's art: magenta, far from any grey */
    palette[3 * BLADE_ART] = 5, palette[3 * BLADE_ART + 1] = 5, palette[3 * BLADE_ART + 2] = 5;   /* the drawn blade: near black */
}

static uint8_t hood_colour(int y) { return (uint8_t)(HOOD + (y - EDGE)); }

typedef struct {
    uint8_t art[W * H], mask[W * H];
} Dash;

/* the glass above EDGE, the hood (cyan) below it, the dash's art (blue) in the first `interior` columns of the hood */
static void dash_make(Dash *d, int interior)
{
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int i = y * W + x;
            if (y < EDGE) d->art[i] = KEY, d->mask[i] = DASH_MASK_AUTO;
            else if (x < interior) d->art[i] = INTERIOR, d->mask[i] = DASH_MASK_DASH;
            else d->art[i] = hood_colour(y), d->mask[i] = DASH_MASK_NOSE;
        }
}

/* a blade `half` thick either side of the line from (x0, y0) to (x1, y1): drawn near black, red in the mask */
static void blade(Dash *d, float x0, float y0, float x1, float y1, float half)
{
    const float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float t = ((x - x0) * dx + (y - y0) * dy) / len2;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            const float px = x0 + t * dx - x, py = y0 + t * dy - y;
            if (px * px + py * py <= half * half) d->art[y * W + x] = BLADE_ART, d->mask[y * W + x] = DASH_MASK_BLADE;
        }
}

/* every red pixel: on the glass rows KEY, on the hood rows its row's grey (within `slack` steps of the ramp) and never the blade's or the dash's colour; every other pixel
 * the nose's own colour or KEY; the count is the nose's pixels */
static void check_fill(const Dash *d, const uint8_t *out, int count, int slack, int edge_rows)
{
    int nose = 0, wrong = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int i = y * W + x;
            if (d->mask[i] == DASH_MASK_BLADE) {
                if (y < EDGE - edge_rows) wrong += out[i] != KEY;
                else if (y >= EDGE + edge_rows) {
                    const int got = out[i], want = hood_colour(y);
                    wrong += got == KEY || got < want - slack || got > want + slack;
                    if (got == KEY || got < want - slack || got > want + slack) fprintf(stderr, "  (%d, %d): %d, want %d\n", x, y, got, want);
                }
            } else if (d->mask[i] == DASH_MASK_NOSE) wrong += out[i] != d->art[i];
            else wrong += out[i] != KEY;
            nose += out[i] != KEY;
        }
    CHECK_EQ_INT(wrong, 0);
    CHECK_EQ_INT(count, nose);
}

static void test_diagonal(void)
{
    static Dash d;
    static uint8_t out[W * H];
    palette_make(0);
    dash_make(&d, 0);
    blade(&d, 4.0f, 3.0f, 40.0f, 29.0f, 1.6f);   /* from the glass down across the hood's edge into the hood */
    const int count = dash_nose_fill(d.art, d.mask, W, H, palette, KEY, out);
    check_fill(&d, out, count, 0, 2);   /* a straight gradient: Laplace's solution is the gradient itself, exact; rows by the edge are the split's (below) */
}

static void test_nearly_flat(void)
{
    static Dash d;
    static uint8_t out[W * H];
    palette_make(0);
    dash_make(&d, 0);
    blade(&d, 2.0f, 20.0f, 46.0f, 25.0f, 1.8f);   /* nearly flat: a row's neighbours either side are the blade, the fill must come from above and below */
    const int count = dash_nose_fill(d.art, d.mask, W, H, palette, KEY, out);
    check_fill(&d, out, count, 0, 0);
    /* a filled pixel deep in the blade, under its middle, keeps its own row's grey: not the row above's carried along (a fill from the same row would be) */
    CHECK_EQ_INT(out[22 * W + 24], hood_colour(22));
}

static void test_touching_the_edge(void)
{
    static Dash d;
    static uint8_t out[W * H];
    palette_make(0);
    dash_make(&d, 0);
    for (int y = EDGE - 2; y < EDGE + 2; y++)   /* a blade along the hood's edge: two rows on the glass, two on the hood */
        for (int x = 6; x < 40; x++) d.art[y * W + x] = BLADE_ART, d.mask[y * W + x] = DASH_MASK_BLADE;
    const int count = dash_nose_fill(d.art, d.mask, W, H, palette, KEY, out);
    check_fill(&d, out, count, 0, 0);   /* the split is at the edge: the glass's rows KEY, the hood's rows their own grey */
    CHECK_EQ_INT(out[(EDGE - 1) * W + 20], KEY);
    CHECK_EQ_INT(out[EDGE * W + 20], hood_colour(EDGE));
}

static void test_dash_art_not_read(void)
{
    static Dash d;
    static uint8_t out[W * H];
    palette_make(0);
    dash_make(&d, 6);   /* the dash's art (magenta) in the hood's first six columns */
    for (int y = EDGE + 2; y < H; y++)   /* a blade down the hood against the dash's art */
        for (int x = 6; x < 9; x++) d.art[y * W + x] = BLADE_ART, d.mask[y * W + x] = DASH_MASK_BLADE;
    const int count = dash_nose_fill(d.art, d.mask, W, H, palette, KEY, out);
    check_fill(&d, out, count, 0, 0);   /* the dash's art claims nothing and is never averaged in: the column next to it is the hood's grey */
    CHECK_EQ_INT(out[20 * W + 6], hood_colour(20));
}

static void test_body_colour(void)
{
    static Dash d;
    static uint8_t grey[W * H], blue[W * H];
    dash_make(&d, 0);
    blade(&d, 4.0f, 3.0f, 40.0f, 29.0f, 1.6f);
    palette_make(0);
    (void)dash_nose_fill(d.art, d.mask, W, H, palette, KEY, grey);
    palette_make(1);   /* another body colour: the same entries, other colours */
    (void)dash_nose_fill(d.art, d.mask, W, H, palette, KEY, blue);
    CHECK(memcmp(grey, blue, sizeof grey) == 0);   /* the ramp's order is the same, so the fill picks the same entries */
}

static void test_no_cyan(void)
{
    static Dash d;
    static uint8_t out[W * H];
    palette_make(0);
    dash_make(&d, 0);
    for (int i = 0; i < W * H; i++)
        if (d.mask[i] == DASH_MASK_NOSE) d.mask[i] = DASH_MASK_AUTO;   /* the masks as they were: no cyan */
    blade(&d, 4.0f, 3.0f, 40.0f, 29.0f, 1.6f);
    CHECK_EQ_INT(dash_nose_fill(d.art, d.mask, W, H, palette, KEY, out), 0);
    int drawn = 0;
    for (int i = 0; i < W * H; i++) drawn += out[i] != KEY;
    CHECK_EQ_INT(drawn, 0);
    CHECK_EQ_INT(dash_nose_fill(NULL, d.mask, W, H, palette, KEY, out), -1);
}

static DashNoseKey key_for(const Dash *d, uint32_t generation, int wipers)
{
    DashNoseKey k;
    memset(&k, 0, sizeof k);
    k.art_id = d->art, k.mask_id = d->mask, k.generation = generation, k.wipers = wipers;
    memcpy(k.palette, palette, sizeof k.palette);
    return k;
}

static void test_cache(void)
{
    static Dash d, other;
    DashNoseCache c;
    memset(&c, 0, sizeof c);
    palette_make(0);
    dash_make(&d, 0);
    blade(&d, 4.0f, 3.0f, 40.0f, 29.0f, 1.6f);
    other = d;
    DashNoseKey k = key_for(&d, 1, 1);
    CHECK(dash_nose_cached(&c, &k, d.art, d.mask, W, H, KEY) != NULL);
    CHECK_EQ_INT(c.builds, 1);
    CHECK(dash_nose_cached(&c, &k, d.art, d.mask, W, H, KEY) != NULL);
    CHECK_EQ_INT(c.builds, 1);   /* the same: kept */
    palette_make(1);   /* the body colour */
    k = key_for(&d, 1, 1);
    (void)dash_nose_cached(&c, &k, d.art, d.mask, W, H, KEY);
    CHECK_EQ_INT(c.builds, 2);
    k = key_for(&other, 1, 1);   /* another car's dash */
    (void)dash_nose_cached(&c, &k, other.art, other.mask, W, H, KEY);
    CHECK_EQ_INT(c.builds, 3);
    k = key_for(&other, 2, 1);   /* the settings' generation moved on (a change of the settings with no restart): made again */
    (void)dash_nose_cached(&c, &k, other.art, other.mask, W, H, KEY);
    CHECK_EQ_INT(c.builds, 4);
    k = key_for(&other, 2, 0);   /* the wipers off: no nose, nothing kept */
    CHECK(dash_nose_cached(&c, &k, other.art, other.mask, W, H, KEY) == NULL);
    CHECK(c.pixels == NULL);
    k = key_for(&other, 2, 1);   /* and on again: made afresh */
    CHECK(dash_nose_cached(&c, &k, other.art, other.mask, W, H, KEY) != NULL);
    CHECK_EQ_INT(c.builds, 5);
    for (int i = 0; i < W * H; i++)
        if (other.mask[i] == DASH_MASK_NOSE) other.mask[i] = DASH_MASK_AUTO;
    k = key_for(&other, 2, 1);
    k.mask_id = NULL;   /* a mask without cyan */
    CHECK(dash_nose_cached(&c, &k, other.art, other.mask, W, H, KEY) == NULL);
    dash_nose_cache_free(&c);
}

int main(void)
{
    test_diagonal();
    test_nearly_flat();
    test_touching_the_edge();
    test_dash_art_not_read();
    test_body_colour();
    test_no_cyan();
    test_cache();
    return TEST_REPORT();
}
