/* The PC's alternative sky as the console version sky's draw takes it (sky_art_draw.h): a made-up 4 x 2 'horz' picture, its own palette and a race palette. */
#include "viewer/sky_art_draw.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { W = 4, H = 2 };

static void test_indexed_and_in_palette(void)
{
    uint8_t texels[W * H] = { 0, 1, 1, 0, 1, 1, 0, 0 };
    uint8_t decode[768] = { 0 };
    decode[3 * 1] = 200, decode[3 * 1 + 1] = 40, decode[3 * 1 + 2] = 40;   /* texel colour 1: red */
    FshEntry horizon = { { 0 }, FSH_TYPE_BITMAP, 0, W, H, { 0, 1, 0, 0 }, texels, sizeof texels };
    SkyArtDraw d;
    memset(&d, 0, sizeof d);
    CHECK(sky_art_draw_make_indexed(&d, &horizon, decode));
    CHECK(d.ready && d.horz.width == W && d.horz.height == H && d.horz.misc[1] == 1);   /* the horizon row is the picture's own */
    CHECK(d.horz.payload != texels && memcmp(d.horz.payload, texels, sizeof texels) == 0);   /* a copy */
    CHECK(sky_art_draw_in_palette(&d) == NULL);   /* no target yet */
    /* the race's palette as the world is shown: entry 7 a red, entry 9 a blue; red's nearest is 7 */
    uint8_t target[768] = { 0 };
    target[3 * 7] = 190, target[3 * 7 + 1] = 50, target[3 * 7 + 2] = 50;
    target[3 * 9] = 30, target[3 * 9 + 1] = 30, target[3 * 9 + 2] = 200;
    sky_art_draw_target(&d, target);
    const FshEntry *shown = sky_art_draw_in_palette(&d);
    CHECK(shown != NULL && shown->payload != NULL && shown->width == W);
    CHECK(shown->payload[1] == 7 && shown->payload[0] == horizon_smooth_nearest(&d.smooth, 0, 0, 0));
    /* the target changes: the picture follows it */
    target[3 * 7] = 20, target[3 * 7 + 1] = 20, target[3 * 7 + 2] = 220;
    target[3 * 9] = 200, target[3 * 9 + 1] = 45, target[3 * 9 + 2] = 45;
    sky_art_draw_target(&d, target);
    shown = sky_art_draw_in_palette(&d);
    CHECK(shown->payload[1] == 9);
    sky_art_draw_free(&d);
    CHECK(!d.ready && d.mapped == NULL);
}

static void test_rejected(void)
{
    SkyArtDraw d;
    memset(&d, 0, sizeof d);
    uint8_t texels[3] = { 0 };
    uint8_t decode[768] = { 0 };
    const FshEntry short_payload = { { 0 }, FSH_TYPE_BITMAP, 0, W, H, { 0 }, texels, sizeof texels };
    CHECK(!sky_art_draw_make_indexed(&d, &short_payload, decode));
    CHECK(!sky_art_draw_make_indexed(&d, NULL, decode));
    CHECK(!d.ready);
}

/* the night: the sky's own colours scaled per channel before they are matched to the shown palette, so a bright orange sky matches the dark entries' darkness */
static void test_scale_darkens_the_sky(void)
{
    uint8_t texels[W * H] = { 0, 1, 1, 0, 1, 1, 0, 0 };
    uint8_t decode[768] = { 0 };
    decode[3 * 1] = 200, decode[3 * 1 + 1] = 100, decode[3 * 1 + 2] = 40;
    FshEntry horizon = { { 0 }, FSH_TYPE_BITMAP, 0, W, H, { 0, 1, 0, 0 }, texels, sizeof texels };
    SkyArtDraw d;
    memset(&d, 0, sizeof d);
    CHECK(sky_art_draw_make_indexed(&d, &horizon, decode));
    const float mul[3] = { 0.5f, 0.25f, 2.0f };
    sky_art_draw_scale(&d, mul);
    CHECK_EQ_INT(d.bitmap.palette[3], 100);
    CHECK_EQ_INT(d.bitmap.palette[4], 25);
    CHECK_EQ_INT(d.bitmap.palette[5], 80);
    CHECK_EQ_INT(d.bitmap.palette[0], 0);
    const float big[3] = { 4, 4, 4 };
    sky_art_draw_scale(&d, big);
    CHECK_EQ_INT(d.bitmap.palette[3], 255);   /* kept in a byte */
    sky_art_draw_free(&d);
    sky_art_draw_scale(&d, mul);   /* nothing made: nothing happens */
}

/* the night's hue: every colour of the sky keeps its luminance and takes the ramp's hue, an orange dusk sky and an olive one alike go blue-black */
static void test_rehue_keeps_luminance_and_drops_the_dusk_hue(void)
{
    uint8_t texels[W * H] = { 0, 1, 2, 0, 1, 2, 0, 0 };
    uint8_t decode[768] = { 0 };
    decode[3 * 1] = 60, decode[3 * 1 + 1] = 30, decode[3 * 1 + 2] = 20;    /* a dusk's red-brown */
    decode[3 * 2] = 25, decode[3 * 2 + 1] = 27, decode[3 * 2 + 2] = 0;     /* an olive */
    FshEntry horizon = { { 0 }, FSH_TYPE_BITMAP, 0, W, H, { 0, 1, 0, 0 }, texels, sizeof texels };
    SkyArtDraw d;
    memset(&d, 0, sizeof d);
    CHECK(sky_art_draw_make_indexed(&d, &horizon, decode));
    const float ramp[3] = { 0.6f, 0.8f, 1.0f };
    sky_art_draw_rehue(&d, ramp);
    for (int i = 1; i <= 2; i++) {
        const uint8_t *const c = d.bitmap.palette + 3 * i;
        const int was = i == 1 ? (30 * 60 + 59 * 30 + 11 * 20 + 50) / 100 : (30 * 25 + 59 * 27 + 50) / 100;
        const int now = (30 * c[0] + 59 * c[1] + 11 * c[2] + 50) / 100;
        CHECK(abs(now - was) <= 2);                 /* the luminance stays */
        CHECK(c[2] > c[1] && c[1] > c[0]);          /* the ramp's order: blue over green over red */
    }
    CHECK_EQ_INT(d.bitmap.palette[0], 0);           /* black stays black */
    sky_art_draw_free(&d);
    sky_art_draw_rehue(&d, ramp);                    /* nothing made: nothing happens */
}

/* a made-up night sky: 256 x 32, the horizon at row 24, row y of palette entry y (a dark ramp), entry 31 a bright cloud */
enum { SW = 256, SH = 32, SHORIZON = 24, CLOUD = 31 };

static void night_sky(SkyArtDraw *d, uint8_t pixels[SW * SH])
{
    memset(d, 0, sizeof *d);
    uint8_t palette[768] = { 0 };
    for (int i = 0; i < SH; i++) palette[3 * i] = (uint8_t)(i + 5), palette[3 * i + 1] = (uint8_t)(i + 8), palette[3 * i + 2] = (uint8_t)(i * 2 + 20);
    palette[3 * CLOUD] = palette[3 * CLOUD + 1] = palette[3 * CLOUD + 2] = 220;
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++) pixels[y * SW + x] = (uint8_t)(x == 5 && y < SHORIZON ? CLOUD : y);
    FshEntry horizon = { { 0 }, FSH_TYPE_BITMAP, 0, SW, SH, { 0, SHORIZON, 0, 0 }, pixels, SW * SH };
    sky_art_draw_make_indexed(d, &horizon, palette);
}

static int is_star(const SkyArtDraw *d, uint8_t index)
{
    return index >= 32 && index < 35 && d->bitmap.palette[3 * index] == d->bitmap.palette[3 * index + 1];
}

static void test_stars_in_the_dark_sky(void)
{
    uint8_t pixels[SW * SH];
    SkyArtDraw d;
    night_sky(&d, pixels);
    const int n = sky_art_draw_stars(&d, 7);
    CHECK(n > 10 && n < SW * SHORIZON / 8);          /* a sprinkle, not a snow */
    int seen = 0;
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++) {
            const uint8_t v = d.horz.payload[y * SW + x];
            if (is_star(&d, v)) {
                seen++;
                CHECK(y < SHORIZON - 2);                 /* none on the horizon or below */
                CHECK(x != 5);                           /* none over the cloud's column (a bright texel stays) */
            } else {
                CHECK_EQ_INT(v, pixels[y * SW + x]);     /* the rest of the sky is as it was */
            }
        }
    CHECK_EQ_INT(seen, n);
    CHECK(d.bitmap.palette[3 * 32] < d.bitmap.palette[3 * 33] && d.bitmap.palette[3 * 33] < d.bitmap.palette[3 * 34]);   /* three greys, dim to bright */
    sky_art_draw_free(&d);
}

static void test_stars_are_the_seeds_and_thinner_low(void)
{
    uint8_t pa[SW * SH], pb[SW * SH], pc[SW * SH];
    SkyArtDraw a, b, c;
    night_sky(&a, pa), night_sky(&b, pb), night_sky(&c, pc);
    CHECK_EQ_INT(sky_art_draw_stars(&a, 3), sky_art_draw_stars(&b, 3));
    CHECK(memcmp(a.horz.payload, b.horz.payload, SW * SH) == 0);   /* the same seed, the same stars */
    sky_art_draw_stars(&c, 4);
    CHECK(memcmp(a.horz.payload, c.horz.payload, SW * SH) != 0);
    int top = 0, low = 0;
    for (int y = 0; y < SHORIZON; y++)
        for (int x = 0; x < SW; x++)
            if (is_star(&a, a.horz.payload[y * SW + x])) {
                if (y < SHORIZON / 2) top++;
                else low++;
            }
    CHECK(top > low);   /* thicker overhead, thinning to the horizon */
    sky_art_draw_free(&a), sky_art_draw_free(&b), sky_art_draw_free(&c);
    CHECK_EQ_INT(sky_art_draw_stars(&a, 1), 0);   /* nothing made: nothing happens */
}

static void test_stars_take_the_least_used_entries(void)
{
    uint8_t pixels[SW * SH];
    SkyArtDraw d;
    night_sky(&d, pixels);
    for (int i = 32; i < 256; i++) {   /* every entry in use, none grey: the ground's rows hold entries 32.. once each */
        d.bitmap.palette[3 * i] = (uint8_t)i, d.bitmap.palette[3 * i + 1] = (uint8_t)(255 - i), d.bitmap.palette[3 * i + 2] = 9;
        d.bitmap.pixels[26 * SW + (i - 32)] = (uint8_t)i;
    }
    sky_art_draw_stars(&d, 1);
    int grey = 0;
    for (int i = 0; i < SW * 4; i++) {   /* the ground's texels are never stars; a used entry taken for them was given up for the nearest left */
        const uint8_t v = d.horz.payload[26 * SW + i];
        grey += d.bitmap.palette[3 * v] == d.bitmap.palette[3 * v + 1] && d.bitmap.palette[3 * v] == d.bitmap.palette[3 * v + 2];
    }
    CHECK_EQ_INT(grey, 0);
    sky_art_draw_free(&d);
}

/* the stars stay high and clear of the land: none above the horizon's row, none on a hill's or a
 * building's silhouette (a column whose texels fall to black from some row down), and the sky above one still has them */
static void test_stars_stay_high_and_off_the_land(void)
{
    uint8_t pixels[SW * SH];
    SkyArtDraw d;
    night_sky(&d, pixels);
    enum { HILL_FROM = 100, HILL_TO = 109, HILL_TOP = 8 };
    for (int y = HILL_TOP; y < SHORIZON; y++)
        for (int x = HILL_FROM; x <= HILL_TO; x++) d.bitmap.pixels[y * SW + x] = 0;   /* black land in the dark ramp */
    uint8_t before[SW * SH];
    memcpy(before, d.bitmap.pixels, sizeof before);
    CHECK(sky_art_draw_stars(&d, 11) > 10);
    int above_hill = 0;
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++)
            if (d.bitmap.pixels[y * SW + x] != before[y * SW + x]) {
                CHECK(y < SHORIZON);                                              /* above the horizon */
                CHECK(!(x >= HILL_FROM && x <= HILL_TO && y >= HILL_TOP - 1));      /* not on the land, nor the row just over it */
                above_hill += x >= HILL_FROM && x <= HILL_TO;
            }
    CHECK(above_hill >= 0);
    sky_art_draw_free(&d);
}

/* a sky that darkens smoothly to the horizon (the console version panoramas' gradient, 97 to 57 and under) is not land: its columns keep their stars, and the count stays
 * what the old sprinkle gave over the same area */
static void test_a_gradient_is_not_land(void)
{
    uint8_t pixels[SW * SH];
    SkyArtDraw d;
    memset(&d, 0, sizeof d);
    uint8_t palette[768] = { 0 };
    for (int i = 0; i < SH; i++) palette[3 * i] = palette[3 * i + 1] = palette[3 * i + 2] = (uint8_t)(60 - i);   /* darker as it goes down: 60 .. 29 */
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++) pixels[y * SW + x] = (uint8_t)(y + ((x * 7 + y * 13) % 5 == 0 ? 6 : 0));   /* dithered: a texel in five a few steps darker */
    FshEntry horizon = { { 0 }, FSH_TYPE_BITMAP, 0, SW, SH, { 0, SHORIZON, 0, 0 }, pixels, SW * SH };
    sky_art_draw_make_indexed(&d, &horizon, palette);
    const int n = sky_art_draw_stars(&d, 5);
    CHECK(n > SW * SHORIZON / 400);   /* about a hundredth of the band, not a handful */
    sky_art_draw_free(&d);
}

int main(void)
{
    test_indexed_and_in_palette();
    test_rejected();
    test_scale_darkens_the_sky();
    test_rehue_keeps_luminance_and_drops_the_dusk_hue();
    test_stars_in_the_dark_sky();
    test_stars_are_the_seeds_and_thinner_low();
    test_stars_take_the_least_used_entries();
    test_stars_stay_high_and_off_the_land();
    test_a_gradient_is_not_land();
    return TEST_REPORT();
}
