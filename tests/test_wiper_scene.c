/* The wiper scene's measure (wiper_scene.h) on art made for the test: a flat-bottomed windscreen of LAYER_KEY in an opaque dash, with a hole (the wheel) and an island of glass of its own; the glass on the frame
 * at the screen's width (the dash's middle as it is, its sides stretched) and the pair placed on the glass the frame shows. */
#include "game/viewer/wiper_scene.h"
#include "game/viewer/layer_compose.h"
#include "game/viewer/screen_layout.h"
#include "game/viewer/track_load.h"
#include "tests/test.h"

#include <math.h>
#include <string.h>

enum { GL = 80, GR = SCREEN_W - 1, GTOP = 100, GBOT = 299, WIDE = 1120 };   /* the glass: columns GL..GR (on to the right edge, as a dash view's), rows GTOP..GBOT */
static uint8_t art[SCREEN_W * SCREEN_H], flags[SCREEN_W * SCREEN_H], map[WIDE * SCREEN_H];
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

static void test_the_frame_glass_is_the_dash_as_the_screen_places_it(void)
{
    make_dash();
    wiper_scene_frame_glass(flags, SCREEN_W, map);   /* 640: the layer itself */
    CHECK(memcmp(map, flags, sizeof flags) == 0);
    wiper_scene_frame_glass(flags, WIDE, map);
    for (int y = 0; y < SCREEN_H; y += 7)
        for (int x = 0; x < WIDE; x += 3)
            CHECK_EQ_INT(map[y * WIDE + x], flags[screen_dash_y(y) * SCREEN_W + screen_dash_x(WIDE, x)]);
    CHECK_EQ_INT(map[(GBOT + DASH_DROP) * WIDE + WIDE - 1], 1);   /* the glass on to the wide screen's right edge, dropped with the dash */
    CHECK_EQ_INT(map[(GBOT + DASH_DROP + 1) * WIDE + WIDE - 1], 0);
}

static void test_the_edges_are_read_from_the_map(void)
{
    make_dash();
    CHECK(wiper_scene_windscreen_bottom(flags, SCREEN_W, 300) == (float)GBOT - 2.0f);
    static uint8_t none[SCREEN_W * SCREEN_H];
    CHECK(wiper_scene_windscreen_bottom(none, SCREEN_W, 20) < 0.0f);   /* no glass in that column */
    int l = -1, r = -1;
    wiper_scene_glass_extent(flags, SCREEN_W, 200, &l, &r);
    CHECK_EQ_INT(l, GL);
    CHECK_EQ_INT(r, GR);
    CHECK_EQ_INT(wiper_scene_glass_row(flags, SCREEN_W), GBOT - 10);
    wiper_scene_frame_glass(flags, WIDE, map);
    CHECK_EQ_INT(wiper_scene_glass_row(map, WIDE), GBOT + DASH_DROP - 10);
}

/* the pair on the glass the frame shows (`width` wide, its left edge `gl`): the pivots and the length are shares of the glass's width, so the blades sit at the same places of the windscreen at any width */
static void check_pair(const uint8_t *m, int width, int gl)
{
    WiperBlade b[WIPER_BLADES];
    float span = 0.0f;
    wiper_scene_measure(m, width, -1.0f, b, &span);
    const float seen = (float)(width - 1 - gl);
    CHECK(fabsf(b[0].x - ((float)gl + WIPER_LEFT_SHARE * seen)) < 1.0f);
    CHECK(fabsf(b[1].x - ((float)gl + WIPER_RIGHT_SHARE * seen)) < 1.0f);
    CHECK(fabsf(b[0].len - WIPER_LEN_SHARE * (float)SCREEN_H) < 0.5f && b[1].len == b[0].len);   /* the dash keeps its size at any width (its middle is drawn 1:1), so do the blades */
    float ux, uy;
    wiper_offset(&b[0], b[0].len, b[0].park + 3.14159265f / 2.0f, &ux, &uy);
    CHECK(-uy < b[0].len && -uy > 0.75f * b[0].len);   /* straight up the glass it is foreshortened, a little shorter on the screen than at rest */
    CHECK(b[0].park == 0.0f && b[1].park == 0.0f);   /* a flat bottom edge: parked flat along it */
    CHECK(fabsf(b[0].y - b[1].y) < 1.0f && b[0].y >= (float)(GBOT + (width > SCREEN_W ? DASH_DROP : 0)) - 3.0f);   /* both on the glass's bottom edge */
    CHECK(b[0].xmax == 0.0f && b[1].xmax == 0.0f);   /* the glass decides where a blade shows, not the 640 layer's edge */
    for (int i = 0; i < WIPER_BLADES; i++) {   /* at rest each blade lies on the glass (where nothing nearer hides it): its tip's pixel, or the last one in the frame, is glass */
        float dx, dy;
        const float r = fminf(b[i].len * 0.95f, (float)width - 2.0f - b[i].x);
        wiper_offset(&b[i], r, b[i].park, &dx, &dy);
        CHECK_EQ_INT(m[(int)(b[i].y + dy) * width + (int)(b[i].x + dx)], 1);
    }
    CHECK(span >= 50.0f && span <= 120.5f);
    float again = 0.0f;
    WiperBlade copy[WIPER_BLADES];
    wiper_scene_measure(m, width, -1.0f, copy, &again);   /* the same glass, the same measure */
    CHECK(again == span && memcmp(copy, b, sizeof copy) == 0);
}

static void test_the_pair_is_placed_on_the_glass_the_frame_shows(void)
{
    make_dash();
    check_pair(flags, SCREEN_W, GL);
    wiper_scene_frame_glass(flags, WIDE, map);
    int gl = 0;
    while (map[(GBOT + DASH_DROP - 10) * WIDE + gl] == 0) gl++;
    check_pair(map, WIDE, gl);
    WiperBlade narrow[WIPER_BLADES], wide[WIPER_BLADES];
    float s0, s1;
    wiper_scene_measure(flags, SCREEN_W, -1.0f, narrow, &s0);
    wiper_scene_measure(map, WIDE, -1.0f, wide, &s1);
    CHECK(wide[1].x + wide[1].len > (float)(WIDE + SCREEN_W) / 2.0f);   /* 21:9: the right blade at rest reaches past where the 640 layer's right edge is, into the stretched side */
    CHECK(wide[0].len == narrow[0].len);   /* the same blades at any width */
}

/* the fit stops at the windscreen's outline (the left pillar, the roof), not at what hangs in front of the glass (the mirror): a blade passes behind it */
static void test_the_sweep_passes_behind_the_mirror(void)
{
    make_dash();
    WiperBlade b[WIPER_BLADES];
    float open = 0.0f, hung = 0.0f;
    wiper_scene_measure(flags, SCREEN_W, -1.0f, b, &open);
    for (int y = GTOP + 5; y < GTOP + 60; y++)   /* a mirror hung inside the glass, over the right blade's way up */
        for (int x = (int)b[1].x - 120; x < (int)b[1].x + 40; x++) flags[y * SCREEN_W + x] = 0;
    wiper_scene_measure(flags, SCREEN_W, -1.0f, b, &hung);
    CHECK(hung == open);
    const float before = wiper_scene_blade_fit(flags, SCREEN_W, &b[0], 120.0f);
    CHECK(before > 24.0f && before < 120.0f);   /* the roof stops it short of the most it may turn */
    for (int y = GTOP; y < GBOT - 90; y++)   /* the glass's left edge pushed in over the left blade's way up (the outline, not a hole): its sweep is cut there */
        for (int x = GL; x < (int)b[0].x + 200; x++) flags[y * SCREEN_W + x] = 0;
    const float cut = wiper_scene_blade_fit(flags, SCREEN_W, &b[0], 120.0f);
    CHECK(cut < before);
}

/* the wheel's middle is the middle of the columns its frames reach in any pose (wheel_front_zone): -1 with no wheel */
static void test_the_wheel_is_found_in_its_zone(void)
{
    static uint8_t zone[SCREEN_W * SCREEN_H];
    memset(zone, 0, sizeof zone);
    CHECK(wiper_scene_wheel_x(zone) < 0.0f);
    for (int y = 330; y < 470; y++)
        for (int x = 150; x <= 500; x++) zone[y * SCREEN_W + x] = 1;
    CHECK(fabsf(wiper_scene_wheel_x(zone) - 325.0f) < 0.01f);
}

/* the dash's column on the frame: as it is at 640, moved by half the extra width in the dash's middle on a wide frame */
static void test_a_dash_column_on_the_frame(void)
{
    CHECK(wiper_scene_frame_x(SCREEN_W, 326.5f) == 326.5f);
    const float wide = wiper_scene_frame_x(WIDE, 326.5f);
    CHECK(fabsf(wide - (326.5f + (float)(WIDE - SCREEN_W) / 2.0f)) < 0.01f);
    CHECK_EQ_INT(screen_dash_x(WIDE, (int)wide), 326);
}

/* the owner's rule: the pivots either side of the steering wheel, the same distance each way (a share of the dash's height: the dash is drawn 1:1 at any width), on the glass's bottom edge,
 * wherever the wheel is and at any width */
static void test_the_pivots_flank_the_wheel(void)
{
    make_dash();
    wiper_scene_frame_glass(flags, WIDE, map);
    const struct { const uint8_t *m; int width; float wheel; } cases[] = {
        { flags, SCREEN_W, 320.0f }, { flags, SCREEN_W, 300.0f }, { map, WIDE, 560.0f }, { map, WIDE, 590.0f }
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        WiperBlade b[WIPER_BLADES];
        float span = 0.0f;
        wiper_scene_measure(cases[i].m, cases[i].width, cases[i].wheel, b, &span);
        const float spread = WIPER_PIVOT_SHARE * (float)SCREEN_H;
        CHECK(fabsf(b[0].x - (cases[i].wheel - spread)) < 1.0f);
        CHECK(fabsf(b[1].x - (cases[i].wheel + spread)) < 1.0f);
        CHECK(fabsf((b[0].x + b[1].x) / 2.0f - cases[i].wheel) < 1.0f);   /* symmetric about the wheel */
        CHECK(fabsf(b[0].y - b[1].y) < 1.0f && b[0].y >= (float)(GBOT + (cases[i].width > SCREEN_W ? DASH_DROP : 0)) - 3.0f);   /* on the glass's bottom edge */
        CHECK(span >= 50.0f);
    }
}

int main(void)
{
    test_the_wheel_is_found_in_its_zone();
    test_a_dash_column_on_the_frame();
    test_the_pivots_flank_the_wheel();
    test_the_sweep_passes_behind_the_mirror();
    test_the_flags_are_the_windscreen_alone();
    test_the_frame_glass_is_the_dash_as_the_screen_places_it();
    test_the_edges_are_read_from_the_map();
    test_the_pair_is_placed_on_the_glass_the_frame_shows();
    return TEST_REPORT();
}
