/* The wiper scene's measure (wiper_scene.h) on art made for the test: a flat-bottomed windscreen of LAYER_KEY in an opaque dash, with a hole (the wheel) and an island of glass of its own. */
#include "game/viewer/wiper_scene.h"
#include "game/viewer/layer_compose.h"
#include "game/viewer/track_load.h"
#include "tests/test.h"

#include <string.h>

enum { GL = 0, GR = SCREEN_W - 1, GTOP = 100, GBOT = 299 };   /* the glass: columns GL..GR, rows GTOP..GBOT */
static uint8_t art[SCREEN_W * SCREEN_H], flags[SCREEN_W * SCREEN_H];
static int stack[SCREEN_W * SCREEN_H];

static void make_dash(void)
{
    memset(art, 7, sizeof art);
    for (int y = GTOP; y <= GBOT; y++)
        for (int x = GL; x <= GR; x++) art[y * SCREEN_W + x] = LAYER_KEY;
    for (int y = 150; y < 190; y++)
        for (int x = 140; x < 180; x++) art[y * SCREEN_W + x] = 7;   /* a hole in the glass */
    for (int y = 360; y < 380; y++)
        for (int x = 300; x < 320; x++) art[y * SCREEN_W + x] = LAYER_KEY;   /* glass apart from the windscreen */
    wiper_scene_glass_flags(art, NULL, flags, stack);
}

static void test_the_flags_are_the_windscreen_alone(void)
{
    make_dash();
    CHECK_EQ_INT(flags[200 * SCREEN_W + 300], 1);
    CHECK_EQ_INT(flags[170 * SCREEN_W + 160], 0);   /* the hole */
    CHECK_EQ_INT(flags[370 * SCREEN_W + 310], 0);   /* the island, not connected */
    CHECK_EQ_INT(flags[50 * SCREEN_W + 300], 0);    /* the dash */
}

static void test_the_edges_are_read_from_the_flags(void)
{
    make_dash();
    CHECK(wiper_scene_windscreen_bottom(flags, 300) == (float)GBOT - 2.0f);
    static uint8_t none[SCREEN_W * SCREEN_H];
    CHECK(wiper_scene_windscreen_bottom(none, 20) < 0.0f);   /* no glass in that column */
    int l = -1, r = -1;
    wiper_scene_glass_extent(flags, 200, &l, &r);
    CHECK_EQ_INT(l, GL);
    CHECK_EQ_INT(r, GR);
    CHECK_EQ_INT(wiper_scene_glass_row(flags), GBOT - 10);
    CHECK(wiper_scene_wheel_left_rim(flags, (float)GBOT - 2.0f, 0.0f, GL, 333.0f) == 333.0f);   /* no bump: the fallback */
}

static void test_the_pair_is_measured_on_the_glass(void)
{
    make_dash();
    WiperBlade layer[WIPER_BLADES];
    float span = 0.0f;
    wiper_scene_measure(art, NULL, flags, layer, &span);
    CHECK(layer[0].len > 0.0f);
    CHECK(layer[0].len == layer[1].len);
    CHECK(layer[1].x > layer[0].x);   /* the second blade right of the driver's */
    CHECK(layer[0].park == 0.0f);     /* a flat bottom edge: parked flat */
    CHECK(span >= 50.0f && span <= 90.5f);
    float again = 0.0f;
    WiperBlade copy[WIPER_BLADES];
    wiper_scene_measure(art, NULL, flags, copy, &again);   /* the same art, the same measure */
    CHECK(again == span && memcmp(copy, layer, sizeof copy) == 0);
}

int main(void)
{
    test_the_flags_are_the_windscreen_alone();
    test_the_edges_are_read_from_the_flags();
    test_the_pair_is_measured_on_the_glass();
    return TEST_REPORT();
}
