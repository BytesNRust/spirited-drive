/* The flat sky's art extended above its horizon line until it reaches the view's top (sky_stretch.h), in its two modes. No game data. */
#include "game/race/sky_stretch.h"
#include "game/race/horizon.h"
#include "test.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { W = 4, H = 10, ABOVE = 6 };   /* a bitmap 4 wide, 10 rows, the horizon on row 6 */

/* each row a different byte, so a resampled row says which source row it came from */
static FshEntry make_art(uint8_t pixels[W * H])
{
    for (int y = 0; y < H; y++) memset(pixels + y * W, 10 + y, W);
    FshEntry e = { { 'h', 'o', 'r', 'z' }, 0, 0, W, H, { 0, ABOVE, 0, 0 }, pixels, W * H };
    return e;
}

static uint8_t grey[768];   /* palette entry i is the grey (i, i, i) */

static int32_t scaled(int32_t rows, int32_t scale) { return (int32_t)(((uint32_t)rows * (uint32_t)scale) >> 16); }

/* an art that already reaches the top is the same entry, whatever the flag-on caller asks */
static void test_unchanged_when_it_reaches(void)
{
    uint8_t px[W * H];
    const FshEntry art = make_art(px);
    SkyStretch s = { 0 };
    CHECK(sky_stretch(&s, &art, 0x20000, 100, 100 - 12, SKY_FILL_COLUMN, grey) == &art);   /* 6 rows x 2 = 12 reach exactly */
    CHECK(sky_stretch(&s, &art, 0x20000, 100, 100 - 5, SKY_FILL_COLUMN, grey) == &art);    /* more than needed */
    CHECK(sky_stretch(&s, &art, 0x20000, 100, 100, SKY_FILL_COLUMN, grey) == &art);         /* the top on the line */
    CHECK(sky_stretch(&s, &art, 0x20000, 100, 130, SKY_FILL_COLUMN, grey) == &art);         /* the top below the line */
    sky_stretch_free(&s);
}

static void test_degenerate_inputs(void)
{
    uint8_t px[W * H];
    FshEntry art = make_art(px);
    SkyStretch s = { 0 };
    CHECK(sky_stretch(&s, &art, 0, 100, 0, SKY_FILL_COLUMN, grey) == &art);        /* no scale */
    CHECK(sky_stretch(&s, &art, -5, 100, 0, SKY_FILL_COLUMN, grey) == &art);
    art.misc[1] = 0;
    CHECK(sky_stretch(&s, &art, 0x20000, 100, 0, SKY_FILL_COLUMN, grey) == &art);  /* nothing above the line to stretch */
    art = make_art(px);
    art.payload = NULL;
    CHECK(sky_stretch(&s, &art, 0x20000, 100, 0, SKY_FILL_COLUMN, grey) == &art);
    CHECK(sky_stretch(&s, NULL, 0x20000, 100, 0, SKY_FILL_COLUMN, grey) == NULL);
    art = make_art(px);
    CHECK(sky_stretch(&s, &art, 0x20000, INT32_MIN, 0, SKY_FILL_COLUMN, grey) == &art);   /* the horizon behind the eye */
    sky_stretch_free(&s);
}

/* the rows above the line: the smallest count whose scaled height covers what the view needs */
static void test_rows_needed(void)
{
    uint8_t px[W * H];
    const FshEntry art = make_art(px);
    SkyStretch s = { 0 };
    const int32_t scale = 0x20000;
    for (int32_t needed = 13; needed < 60; needed++) {
        const FshEntry *out = sky_stretch(&s, &art, scale, 100, 100 - needed, SKY_FILL_COLUMN, grey);
        CHECK(out != &art);
        const int32_t above = out->misc[1];
        CHECK(scaled(above, scale) >= needed);
        CHECK(scaled(above - 1, scale) < needed);
    }
    sky_stretch_free(&s);
}

static void test_rows_below_the_line_untouched(void)
{
    uint8_t px[W * H];
    const FshEntry art = make_art(px);
    SkyStretch s = { 0 };
    const FshEntry *out = sky_stretch(&s, &art, 0x20000, 100, 100 - 40, SKY_FILL_COLUMN, grey);
    const int32_t above = out->misc[1];
    CHECK(above > ABOVE);
    CHECK_EQ_INT(out->width, W);
    CHECK_EQ_INT(out->height, H - ABOVE + above);
    CHECK_EQ_INT(out->misc[0], 0);
    CHECK_EQ_INT(out->misc[2], 0);
    CHECK_EQ_INT(out->misc[3], 0);
    CHECK(memcmp(out->payload + above * W, art.payload + ABOVE * W, (size_t)(H - ABOVE) * W) == 0);
    CHECK_EQ_INT(out->payload[out->width * out->height - 1], art.payload[W * H - 1]);   /* the bottom fill's colour */
    CHECK_EQ_INT((int)out->payload_size, W * out->height);
    sky_stretch_free(&s);
}

/* column mode: the art is not stretched, the added rows are its top row, column by column, and its own rows follow */
static void test_column_rows_are_the_top_row(void)
{
    uint8_t px[W * H];
    FshEntry art = make_art(px);
    for (int x = 0; x < W; x++) px[x] = (uint8_t)(100 + x);   /* the top row: a different colour per column */
    SkyStretch s = { 0 };
    const FshEntry *out = sky_stretch(&s, &art, 0x20000, 100, 100 - 40, SKY_FILL_COLUMN, grey);
    const int32_t above = out->misc[1], added = above - ABOVE;
    CHECK(added > 0);
    for (int32_t r = 0; r < added; r++)
        for (int x = 0; x < W; x++) CHECK_EQ_INT(out->payload[r * W + x], 100 + x);
    CHECK(memcmp(out->payload + added * W, art.payload, (size_t)H * W) == 0);   /* the art as it is */
    sky_stretch_free(&s);
}

/* column mode: one odd pixel in the top row (as on AL1, where a column of row 0 differs from rows 1..7) does not become a band
 * above the art; each column takes the colour most of its top rows share */
static void test_column_ignores_odd_top_pixel(void)
{
    uint8_t px[W * H];
    FshEntry art = make_art(px);
    for (int y = 0; y < ABOVE; y++) memset(px + y * W, 60, W);
    px[2] = 90;   /* row 0, column 2: the odd one */
    SkyStretch s = { 0 };
    const FshEntry *out = sky_stretch(&s, &art, 0x20000, 100, 100 - 40, SKY_FILL_COLUMN, grey);
    const int32_t above = out->misc[1], added = above - ABOVE;
    CHECK(added > 0);
    for (int32_t r = 0; r < added; r++)
        for (int x = 0; x < W; x++) CHECK_EQ_INT(out->payload[r * W + x], 60);
    CHECK(memcmp(out->payload + added * W, art.payload, (size_t)H * W) == 0);   /* the art itself is not changed */
    sky_stretch_free(&s);
}

/* the same camera numbers, another mode, give the same added row count: the mode only changes the colours */
static void test_modes_agree_on_the_rows(void)
{
    uint8_t px[W * H];
    const FshEntry art = make_art(px);
    SkyStretch a = { 0 }, b = { 0 };
    const FshEntry *ca = sky_stretch(&a, &art, 0x20000, 100, 100 - 40, SKY_FILL_COLUMN, grey);
    const FshEntry *cb = sky_stretch(&b, &art, 0x20000, 100, 100 - 40, SKY_FILL_BLEND, grey);
    CHECK_EQ_INT(ca->misc[1], cb->misc[1]);
    CHECK_EQ_INT(ca->height, cb->height);
    CHECK(sky_stretch(&a, &art, 0x20000, 100, 100 - 40, 0, grey) == &art);      /* no such mode */
    CHECK(sky_stretch(&a, &art, 0x20000, 100, 100 - 40, 7, grey) == &art);
    CHECK(sky_stretch(&a, &art, 0x20000, 100, 100 - 40, SKY_FILL_BLEND, NULL) == &art);   /* blend needs a palette */
    sky_stretch_free(&a);
    sky_stretch_free(&b);
}

/* blend mode: far from the art one colour, the nearest palette entry to the mean of the top rows; near it, only that
 * colour or the column's own top pixel; the art's own rows are untouched */
static void test_blend_rows(void)
{
    uint8_t px[W * H];
    FshEntry art = make_art(px);
    for (int y = 0; y < SKY_FILL_MEAN_ROWS; y++)
        for (int x = 0; x < W; x++) px[y * W + x] = (uint8_t)(100 + 10 * y);   /* means 115 over the 4 rows */
    SkyStretch s = { 0 };
    const FshEntry *out = sky_stretch(&s, &art, 0x20000, 100, 100 - 80, SKY_FILL_BLEND, grey);
    const int32_t added = out->misc[1] - ABOVE;
    CHECK(added > SKY_FILL_BLEND_ROWS + 2);
    int seen_top = 0;
    for (int32_t r = 0; r < added; r++) {
        const int32_t d = added - r;
        for (int x = 0; x < W; x++) {
            const uint8_t v = out->payload[r * W + x];
            CHECK(v == 115 || v == 100);
            if (d >= SKY_FILL_BLEND_ROWS) CHECK_EQ_INT(v, 115);
            if (v == 100) seen_top++;
        }
    }
    CHECK(seen_top > 0);   /* the transition does take in the top row */
    int last_row_top = 0;
    for (int x = 0; x < W; x++) last_row_top += out->payload[(added - 1) * W + x] == 100;
    CHECK(last_row_top >= 2);   /* and mostly so right next to the art */
    CHECK(memcmp(out->payload + added * W, art.payload, (size_t)H * W) == 0);
    sky_stretch_free(&s);
}

/* transparent pixels (0xff) in the top rows are not averaged and not copied up */
static void test_blend_skips_transparent(void)
{
    uint8_t px[W * H];
    FshEntry art = make_art(px);
    for (int y = 0; y < SKY_FILL_MEAN_ROWS; y++)
        for (int x = 0; x < W; x++) px[y * W + x] = x == 0 ? 0xff : 60;
    SkyStretch s = { 0 };
    const FshEntry *out = sky_stretch(&s, &art, 0x20000, 100, 100 - 80, SKY_FILL_BLEND, grey);
    for (int32_t r = 0; r < out->misc[1] - ABOVE; r++)
        for (int x = 0; x < W; x++) CHECK_EQ_INT(out->payload[r * W + x], 60);
    sky_stretch_free(&s);
}

typedef struct {
    int32_t band_y, fill_y, fill_h;
} Seen;

static void seen_fill(void *user, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t colour)
{
    Seen *s = user;
    (void)x, (void)w, (void)colour;
    if (s->fill_h == 0) s->fill_y = y, s->fill_h = h;
}

static void seen_band(void *user, const FshEntry *horz, int32_t x_offset, int32_t first_row, int32_t x, int32_t y)
{
    Seen *s = user;
    (void)horz, (void)x_offset, (void)first_row, (void)x;
    s->band_y = y;
}

/* with the stretched art the flat horizon starts at or above the box's top, so the flat fill over it has no rows left */
static void test_horizon_flat_reaches_the_top(void)
{
    uint8_t px[W * H];
    const FshEntry art = make_art(px);
    const int32_t scale = 0x20000, line = 199, top = 32;
    const HorizonBox box = { 0, top, 100, 400 };
    Seen before = { 0 }, after = { 0 };
    const HorizonFlatSink sink_b = { &before, seen_fill, seen_band }, sink_a = { &after, seen_fill, seen_band };
    horizon_flat(&art, 0, 0, 0, scale, &box, line, &sink_b);
    CHECK(before.band_y > top);   /* the exe: the art ends below the top */
    SkyStretch s = { 0 };
    const FshEntry *out = sky_stretch(&s, &art, scale, line, top, SKY_FILL_COLUMN, grey);
    horizon_flat(out, 0, 0, 0, scale, &box, line, &sink_a);
    CHECK(after.band_y <= top);
    CHECK(after.band_y > top - 2 * 2);   /* by less than a row's worth: no more than needed */
    sky_stretch_free(&s);
}

/* asked again with other numbers the entry is rebuilt, and an unchanged ask keeps the same pixels */
static void test_reuse(void)
{
    uint8_t px[W * H];
    const FshEntry art = make_art(px);
    SkyStretch s = { 0 };
    const FshEntry *a = sky_stretch(&s, &art, 0x20000, 100, 100 - 40, SKY_FILL_COLUMN, grey);
    const int32_t above_a = a->misc[1];
    const FshEntry *b = sky_stretch(&s, &art, 0x20000, 100, 100 - 80, SKY_FILL_COLUMN, grey);
    CHECK(b->misc[1] > above_a);
    const FshEntry *c = sky_stretch(&s, &art, 0x20000, 100, 100 - 40, SKY_FILL_COLUMN, grey);
    CHECK_EQ_INT(c->misc[1], above_a);
    CHECK_EQ_INT(c->payload[0], 10);
    CHECK(sky_stretch(&s, &art, 0x20000, 100, 100 - 12, SKY_FILL_COLUMN, grey) == &art);   /* back to the art as it is */
    sky_stretch_free(&s);
}

/* the night's stars sit in the art's top rows: the column fill carries a star in row 0 up as a stripe to the view's top; the quiet one leaves the stars where they are */
static void test_quiet_column_does_not_carry_a_star_up(void)
{
    uint8_t px[W * H];
    FshEntry art = make_art(px);
    px[1] = 235;                 /* a bright star in the top row of column 1 */
    px[2 * W + 2] = 235;         /* and one two rows down in column 2: no stripe either way */
    SkyStretch s = { 0 };
    const FshEntry *loud = sky_stretch(&s, &art, 0x20000, 100, 100 - 40, SKY_FILL_COLUMN, grey);
    CHECK(loud != &art && loud->payload[1] == 235);          /* what the owner sees: the stripe */
    const int added = (int)(loud->height) - (H - ABOVE) - ABOVE;
    const FshEntry *quiet = sky_stretch(&s, &art, 0x20000, 100, 100 - 40, SKY_FILL_COLUMN_QUIET, grey);
    CHECK(quiet != &art && added > 0);
    for (int r = 0; r < added; r++) {
        CHECK(quiet->payload[r * W + 1] < 100);               /* the star's column takes the sky under it */
        CHECK_EQ_INT(quiet->payload[r * W + 0], 10);          /* a column with no star keeps its top pixel, as before */
        CHECK_EQ_INT(quiet->payload[r * W + 3], 10);
    }
    CHECK_EQ_INT(quiet->payload[added * W + 1], 235);         /* the art's own rows are copied as they are, the star kept */
    CHECK(sky_stretch(&s, &art, 0x20000, 100, 100 - 40, SKY_FILL_COLUMN_QUIET, NULL) == &art);   /* it needs the palette to see a star */
    sky_stretch_free(&s);
}

static void test_quiet_column_keeps_a_wide_highlight(void)
{
    enum { WW = 16, HH = 10 };
    uint8_t px[WW * HH];
    for (int y = 0; y < HH; y++) memset(px + y * WW, 10 + y, WW);
    for (int x = 4; x < 12; x++) px[x] = 200;   /* eight columns of light: a cloud's edge, not a star */
    px[1] = 235;                                /* a star at the left */
    FshEntry art = { { 'h', 'o', 'r', 'z' }, 0, 0, WW, HH, { 0, ABOVE, 0, 0 }, px, WW * HH };
    SkyStretch s = { 0 };
    const FshEntry *quiet = sky_stretch(&s, &art, 0x20000, 100, 100 - 40, SKY_FILL_COLUMN_QUIET, grey);
    CHECK(quiet != &art);
    CHECK_EQ_INT(quiet->payload[1], 10);        /* the star: the column beside it */
    for (int x = 4; x < 12; x++) CHECK_EQ_INT(quiet->payload[x], 200);   /* the highlight is carried up as it was */
    sky_stretch_free(&s);
}

static void test_quiet_column_catches_a_dim_star_and_spares_a_grey_bump(void)
{
    enum { WW = 16, HH = 10 };
    uint8_t px[WW * HH];
    for (int y = 0; y < HH; y++) memset(px + y * WW, 10 + y, WW);   /* the sky under the top row: lightness 11 */
    px[1] = 40;     /* a dim star: 29 lighter, 3.6 times as light */
    px[8] = 51;       /* a grey bump on a lit sky: 21 lighter than the 30 under it, under 3 times as light */
    px[WW + 8] = 30;
    FshEntry art = { { 'h', 'o', 'r', 'z' }, 0, 0, WW, HH, { 0, ABOVE, 0, 0 }, px, WW * HH };
    SkyStretch s = { 0 };
    const FshEntry *quiet = sky_stretch(&s, &art, 0x20000, 100, 100 - 40, SKY_FILL_COLUMN_QUIET, grey);
    CHECK(quiet != &art);
    CHECK_EQ_INT(quiet->payload[1], 10);    /* the dim star is not carried up: the column beside it */
    CHECK_EQ_INT(quiet->payload[8], 51);    /* the bump is the sky's own */
    sky_stretch_free(&s);
}

/* sky_fill_below: an art that already reaches the bottom is the same entry, whatever the flag-on caller asks */
static void test_below_unchanged_when_it_reaches(void)
{
    uint8_t px[W * H];
    const FshEntry art = make_art(px);
    SkyStretch s = { 0 };
    /* line 100, 6 rows above x 2: the art's top at 88, its last row at 88 + scaled(9) = 106 */
    CHECK(sky_stretch_below(&s, &art, 0x20000, 100, 106) == &art);
    CHECK(sky_stretch_below(&s, &art, 0x20000, 100, 50) == &art);
    CHECK(sky_stretch_below(&s, NULL, 0x20000, 100, 200) == NULL);
    CHECK(sky_stretch_below(&s, &art, 0, 100, 200) == &art);
    CHECK(sky_stretch_below(&s, &art, 0x20000, INT32_MIN, 200) == &art);
    sky_stretch_free(&s);
}

/* the rows added reach the bottom, the art's own rows and its horizon row are as they were */
static void test_below_adds_the_rows_that_reach(void)
{
    uint8_t px[W * H];
    const FshEntry art = make_art(px);
    SkyStretch s = { 0 };
    const FshEntry *const e = sky_stretch_below(&s, &art, 0x20000, 100, 110);
    CHECK(e != &art);
    CHECK_EQ_INT((int16_t)e->height, 12);           /* ceil((110 - 88) / 2) + 1 rows */
    CHECK_EQ_INT((int16_t)e->misc[1], ABOVE);       /* the horizon row is where it was */
    CHECK_EQ_INT((int16_t)e->width, W);
    CHECK(memcmp(e->payload, px, sizeof px) == 0);  /* the art's own rows */
    CHECK_EQ_INT(88 + scaled(11, 0x20000) >= 110, 1);   /* and they reach the bottom */
    for (int x = 0; x < W; x++) {
        CHECK_EQ_INT(e->payload[10 * W + x], 19);   /* every row of the last 8 differs: the lowest wins the tie */
        CHECK_EQ_INT(e->payload[11 * W + x], 19);
    }
    sky_stretch_free(&s);
}

/* a lone odd pixel in the last rows is outvoted; a transparent one is not a colour; only the most frequent counts */
static void test_below_colour_is_the_most_frequent(void)
{
    uint8_t px[W * H];
    for (int y = 0; y < H; y++) memset(px + y * W, 50, W);
    px[9 * W + 1] = 200;                    /* a star on the last row of column 1 */
    px[7 * W + 2] = 0xff;                   /* transparent in column 2 */
    px[6 * W + 3] = 77, px[7 * W + 3] = 77, px[8 * W + 3] = 77, px[9 * W + 3] = 90;   /* column 3: 77 three times, 90 once */
    CHECK_EQ_INT(sky_below_colour(px, W, H, 0, SKY_FILL_BELOW_ROWS), 50);
    CHECK_EQ_INT(sky_below_colour(px, W, H, 1, SKY_FILL_BELOW_ROWS), 50);   /* the star is outvoted */
    CHECK_EQ_INT(sky_below_colour(px, W, H, 2, SKY_FILL_BELOW_ROWS), 50);
    CHECK_EQ_INT(sky_below_colour(px, W, H, 3, SKY_FILL_BELOW_ROWS), 50);   /* 50 appears 4 times in 8 rows, 77 3 times */
    CHECK_EQ_INT(sky_below_colour(px, W, H, 3, 4), 77);                      /* of the last 4 rows 77 is the 3 */
    for (int y = 0; y < H; y++) memset(px + y * W, 0xff, W);
    CHECK_EQ_INT(sky_below_colour(px, W, H, 0, SKY_FILL_BELOW_ROWS), 0xff);   /* all transparent: the colour of the exe's fill */
    /* fewer rows than asked for */
    uint8_t two[W * 2];
    memset(two, 5, sizeof two);
    CHECK_EQ_INT(sky_below_colour(two, W, 2, 1, SKY_FILL_BELOW_ROWS), 5);
}

/* the horizon table maps HORIZON_ROWS rows: a bottom far away stops there, the rest keeps the exe's flat fill */
static void test_below_stops_at_the_row_table(void)
{
    uint8_t px[W * H];
    const FshEntry art = make_art(px);
    SkyStretch s = { 0 };
    const FshEntry *const e = sky_stretch_below(&s, &art, 0x10000, 0, 100000);
    CHECK(e != &art);
    CHECK_EQ_INT((int16_t)e->height, HORIZON_ROWS);   /* scale 1: one output row a bitmap row */
    sky_stretch_free(&s);
}

int main(void)
{
    for (int i = 0; i < 256; i++) grey[3 * i] = grey[3 * i + 1] = grey[3 * i + 2] = (uint8_t)i;
    test_unchanged_when_it_reaches();
    test_below_unchanged_when_it_reaches();
    test_below_adds_the_rows_that_reach();
    test_below_colour_is_the_most_frequent();
    test_below_stops_at_the_row_table();
    test_degenerate_inputs();
    test_rows_needed();
    test_rows_below_the_line_untouched();
    test_column_rows_are_the_top_row();
    test_column_ignores_odd_top_pixel();
    test_modes_agree_on_the_rows();
    test_quiet_column_does_not_carry_a_star_up();
    test_quiet_column_keeps_a_wide_highlight();
    test_quiet_column_catches_a_dim_star_and_spares_a_grey_bump();
    test_blend_rows();
    test_blend_skips_transparent();
    test_horizon_flat_reaches_the_top();
    test_reuse();
    return TEST_REPORT();
}
