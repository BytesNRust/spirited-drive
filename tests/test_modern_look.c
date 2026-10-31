/* port: EXTRAS' Texture Filter and Fog made into Modern 3D's style for a track (modern_look.h): the fog's colour
 * from the horizon's foot, its depths from the track drawn, its tables remade when the palette changes. */
#include "viewer/modern_look.h"
#include "test.h"

#include <string.h>

enum { HW = 8, HH = 6 };

static uint8_t horizon_px[HW * HH];
static TrackArt art;

/* a horizon whose top rows are colour 1 (blue) and its last FOOT_ROWS rows colour 2 and 3 alternately (greys 100 and
 * 120); colour 0xff (see-through) in a corner of the foot, which the average skips */
static void make_art(void)
{
    memset(&art, 0, sizeof art);
    for (int i = 0; i < 256; i++) art.palette[3 * i] = art.palette[3 * i + 1] = art.palette[3 * i + 2] = (uint8_t)i;
    art.palette[3 * 1] = 0, art.palette[3 * 1 + 1] = 0, art.palette[3 * 1 + 2] = 255;
    art.palette[3 * 2] = art.palette[3 * 2 + 1] = art.palette[3 * 2 + 2] = 100;
    art.palette[3 * 3] = art.palette[3 * 3 + 1] = art.palette[3 * 3 + 2] = 120;
    for (int y = 0; y < HH; y++)
        for (int x = 0; x < HW; x++) horizon_px[y * HW + x] = (uint8_t)(y < HH - MODERN_LOOK_FOOT_ROWS ? 1 : 2 + (x & 1));
    horizon_px[(HH - 1) * HW] = 0xff;
    art.horizon = (FshEntry){ { 0 }, FSH_TYPE_BITMAP, 0, HW, HH, { 0 }, horizon_px, sizeof horizon_px };
    art.has_horizon = 1;
    art.has_palette = 1;
}

static void test_fog_colour(void)
{
    make_art();
    uint8_t rgb[3];
    modern_look_fog_colour(&art, rgb);
    /* the foot's greys (110.7 on average, not the blue) lifted MODERN_LOOK_LIFT_PERCENT of the way to white: a haze is
     * lighter than what it stands in front of */
    const int want = (int)(110.7 + (255 - 110.7) * MODERN_LOOK_LIFT_PERCENT / 100.0);
    CHECK(rgb[0] >= want - 1 && rgb[0] <= want + 1 && rgb[1] == rgb[0] && rgb[2] == rgb[0]);
    art.has_horizon = 0;
    modern_look_fog_colour(&art, rgb);
    CHECK_EQ_INT(rgb[0], MODERN_LOOK_NO_HORIZON);
}

/* the foot all one colour */
static void foot_of(uint8_t r, uint8_t g, uint8_t b)
{
    make_art();
    art.palette[3 * 2] = art.palette[3 * 3] = r, art.palette[3 * 2 + 1] = art.palette[3 * 3 + 1] = g;
    art.palette[3 * 2 + 2] = art.palette[3 * 3 + 2] = b;
}

/* a night horizon (Lost Vegas' black foot) keeps a dark fog: no lift to white */
static void test_night_fog_stays_dark(void)
{
    uint8_t rgb[3];
    foot_of(0, 0, 0);
    modern_look_fog_colour(&art, rgb);
    CHECK(rgb[0] < 16 && rgb[1] < 16 && rgb[2] < 16);
}

/* a strong blue (Alpine's foot, 16 44 148) loses MODERN_LOOK_GREY_PERCENT of its colour to its own grey: still
 * blue, but less than the lift alone left it */
static void test_colour_is_softened_not_lost(void)
{
    uint8_t rgb[3];
    foot_of(16, 44, 148);
    modern_look_fog_colour(&art, rgb);
    const int lifted_spread = (148 + (255 - 148) * MODERN_LOOK_LIFT_PERCENT / 100) - (16 + (255 - 16) * MODERN_LOOK_LIFT_PERCENT / 100);
    CHECK(rgb[2] > rgb[0] + 20);                       /* blue still */
    CHECK(rgb[2] - rgb[0] < lifted_spread * 3 / 4);    /* greyer than the lift alone */
}

static void test_style(void)
{
    make_art();
    static ModernLook look;
    memset(&look, 0, sizeof look);
    CHECK(modern_look_style(&look, &art, 0, 0, 0x1c) == NULL);   /* neither switch: Modern 3D as it was */
    const ModernStyle *s = modern_look_style(&look, &art, 1, 0, 0x1c);
    CHECK(s != NULL && s->filter == 1 && s->fog == NULL);
    s = modern_look_style(&look, &art, 0, 1, 0x1c);
    CHECK(s != NULL && s->filter == 0 && s->fog != NULL);
    CHECK(s->fog_end > s->fog_start && s->fog_start > 0);
    const float normal_end = s->fog_end;
    CHECK(normal_end >= 20000 && normal_end <= 24000);   /* the farthest road is about 24000 deep at 0x1c segments */
    s = modern_look_style(&look, &art, 0, 1, 0x34);
    CHECK(s->fog_end > normal_end * 1.6f);               /* the ultra distance pushes it back */
    uint8_t rgb[3];
    modern_look_fog_colour(&art, rgb);
    CHECK_EQ_INT(s->fog[MODERN_FOG_LEVELS - 1][1], rgb[0]);   /* the blue at full fog: the grey of the fog's colour */
    CHECK(s->fog[1][50] > 50);                                /* the grey 50 a step towards the fog's lighter grey */
    /* the palette changes (the police screen, the next track): the tables follow it */
    art.palette[3 * 50] = 0, art.palette[3 * 50 + 1] = 255, art.palette[3 * 50 + 2] = 0;
    s = modern_look_style(&look, &art, 0, 1, 0x1c);
    CHECK_EQ_INT(s->fog[1][50], 50);   /* now a green, nearest itself a step in: remade */
}

/* the PS1 look's forced dark fog: a light foot (a hazy horizon) gives the dark colour, and the foot's own again when it is cleared */
static void test_dark_fog_forced(void)
{
    make_art();
    static ModernLook look;
    memset(&look, 0, sizeof look);
    modern_look_set_dark(&look, 1);
    const ModernStyle *s = modern_look_style(&look, &art, 0, 1, 0x1c);
    CHECK(s != NULL && s->fog != NULL);
    CHECK(s->fog[MODERN_FOG_LEVELS - 1][255] < 40);   /* the fog's whole is near black, not the foot's light grey */
    modern_look_set_dark(&look, 0);
    s = modern_look_style(&look, &art, 0, 1, 0x1c);
    CHECK(s->fog[MODERN_FOG_LEVELS - 1][255] > 40);
}

/* port (fog_sample): a 4 x 20 horizon whose line is row 10: the band (rows 2..15) forest green, the rows around it other
 * colours (0, 1 red; 16, 17 yellow), the foot (18, 19) a strong blue as Alpine's */
enum { SW = 4, SH = 20, LINE = 10 };
static uint8_t sample_px[SW * SH];

static void make_banded_art(int line)
{
    memset(&art, 0, sizeof art);
    art.palette[3 * 1] = 20, art.palette[3 * 1 + 1] = 70, art.palette[3 * 1 + 2] = 20;      /* the band */
    art.palette[3 * 2] = 16, art.palette[3 * 2 + 1] = 44, art.palette[3 * 2 + 2] = 150;     /* the foot */
    art.palette[3 * 3] = 255;                                                                /* red, outside it */
    art.palette[3 * 4] = art.palette[3 * 4 + 1] = 255;                                       /* yellow, outside it */
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++)
            sample_px[y * SW + x] = (uint8_t)(y >= SH - 2 ? 2 : y < 2 ? 3 : y < 16 ? 1 : 4);
    art.horizon = (FshEntry){ { 0 }, FSH_TYPE_BITMAP, 0, SW, SH, { 0, (int16_t)line, 0, 0 }, sample_px, sizeof sample_px };
    art.has_horizon = 1;
    art.has_palette = 1;
}

/* the band's average (20 70 20) kept as a green, softened toward its grey; not the foot's blue and not the rows around it */
static void test_sampled_band_without_lift(void)
{
    make_banded_art(LINE);
    uint8_t rgb[3];
    modern_look_fog_colour_sampled(&art, 0, rgb);
    CHECK_EQ_INT(rgb[0], 30);
    CHECK_EQ_INT(rgb[1], 63);
    CHECK_EQ_INT(rgb[2], 30);
    uint8_t foot[3];
    modern_look_fog_colour(&art, foot);   /* the foot's own: lifted blue, as it was */
    CHECK(foot[2] > foot[1] + 20);
}

/* the lift is a parameter: more lift, lighter fog, never darker than the band itself */
static void test_sampled_lift(void)
{
    make_banded_art(LINE);
    uint8_t a[3], b[3], c[3];
    modern_look_fog_colour_sampled(&art, 0, a);
    modern_look_fog_colour_sampled(&art, 15, b);
    modern_look_fog_colour_sampled(&art, 40, c);
    for (int k = 0; k < 3; k++) CHECK(a[k] < b[k] && b[k] < c[k]);
    CHECK(b[1] > b[0]);   /* still green */
    uint8_t over[3], whole[3];
    modern_look_fog_colour_sampled(&art, 1000, over);   /* clamped to 100 */
    modern_look_fog_colour_sampled(&art, 100, whole);
    CHECK_MEM(over, whole, 3);
}

/* the band is the rows MODERN_LOOK_BAND_ABOVE above the line to MODERN_LOOK_BAND_BELOW from it down: its edges change nothing outside */
static void test_sampled_rows(void)
{
    make_banded_art(LINE);
    uint8_t exact[3], moved[3];
    modern_look_fog_colour_sampled(&art, 0, exact);
    for (int y = 0; y < 2; y++) memset(sample_px + y * SW, 4, SW);                /* rows above the band */
    for (int y = 16; y < 18; y++) memset(sample_px + y * SW, 3, SW);              /* rows below it */
    modern_look_fog_colour_sampled(&art, 0, moved);
    CHECK_MEM(exact, moved, 3);
    memset(sample_px + (LINE - MODERN_LOOK_BAND_ABOVE) * SW, 4, SW);              /* its first row */
    modern_look_fog_colour_sampled(&art, 0, moved);
    CHECK(memcmp(exact, moved, 3) != 0);
    make_banded_art(LINE);
    memset(sample_px + (LINE + MODERN_LOOK_BAND_BELOW - 1) * SW, 3, SW);          /* its last row */
    modern_look_fog_colour_sampled(&art, 0, moved);
    CHECK(memcmp(exact, moved, 3) != 0);
}

/* a line near the top or past the bottom clips the band to the art; with no horizon, the no-horizon grey */
static void test_sampled_edges(void)
{
    uint8_t rgb[3];
    make_banded_art(1);
    modern_look_fog_colour_sampled(&art, 0, rgb);   /* rows 0..6 */
    CHECK(rgb[0] > 60);                             /* the red rows 0 and 1 are in it (the green alone gives 30) */
    make_banded_art(SH + 30);
    modern_look_fog_colour_sampled(&art, 0, rgb);   /* rows 22..: none of the art */
    CHECK_EQ_INT(rgb[0], MODERN_LOOK_NO_HORIZON);
    make_banded_art(LINE);
    art.has_horizon = 0;
    modern_look_fog_colour_sampled(&art, 0, rgb);
    CHECK_EQ_INT(rgb[0], MODERN_LOOK_NO_HORIZON);
}

/* the style takes the sampled colour when asked, the foot's when not, and the dark fog wins over both */
static void test_style_follows_the_switch(void)
{
    make_banded_art(LINE);
    static ModernLook look;
    memset(&look, 0, sizeof look);
    modern_look_style(&look, &art, 0, 1, 0x1c);
    const uint8_t foot[3] = { look.rgb[0], look.rgb[1], look.rgb[2] };
    modern_look_set_sample(&look, 1, 15);
    modern_look_style(&look, &art, 0, 1, 0x1c);
    uint8_t want[3];
    modern_look_fog_colour_sampled(&art, 15, want);
    CHECK_MEM(look.rgb, want, 3);
    CHECK(memcmp(look.rgb, foot, 3) != 0);
    modern_look_set_dark(&look, 1);
    modern_look_style(&look, &art, 0, 1, 0x1c);
    CHECK_EQ_INT(look.rgb[0], MODERN_LOOK_DARK);
    modern_look_set_dark(&look, 0);
    modern_look_set_sample(&look, 0, 15);
    modern_look_style(&look, &art, 0, 1, 0x1c);
    CHECK_MEM(look.rgb, foot, 3);
}

int main(void)
{
    test_fog_colour();
    test_night_fog_stays_dark();
    test_colour_is_softened_not_lost();
    test_style();
    test_dark_fog_forced();
    test_sampled_band_without_lift();
    test_sampled_lift();
    test_sampled_rows();
    test_sampled_edges();
    test_style_follows_the_switch();
    return TEST_REPORT();
}
