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

int main(void)
{
    test_fog_colour();
    test_night_fog_stays_dark();
    test_colour_is_softened_not_lost();
    test_style();
    return TEST_REPORT();
}
