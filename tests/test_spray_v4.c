/* The v4 spray's model (race/spray_v4.h) and its puffs' sprites (viewer/spray_v4_draw.h): a particle is born where and when its wheel was, its look depends on its age alone, the gravel falls and lands, the water thins and
 * spreads, a puff's sprite has no hard rim. TNFS_SPRAY_V4_SHEET=<file.ppm> writes the sprites as a contact sheet (dev). */
#include "game/race/spray_v4.h"
#include "game/viewer/spray_v4_draw.h"
#include "tests/test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SprayV4Emit car_at(int car, uint32_t tick, float x, float z, float y, int wet, int gravel)
{
    SprayV4Emit e = { car, tick, 7, 30.0f, 1, wet, gravel, { { x - 0.7f, y, z }, { x + 0.7f, y, z } }, { 0, 1 }, { 1, 0 } };
    return e;
}

static void test_a_particle_is_born_where_and_when_its_wheel_was(void)
{
    static SprayV4 s;
    spray_v4_clear(&s);
    const SprayV4Emit e = car_at(0, 100, 10.0f, 50.0f, 3.25f, 1, 0);   /* a hill: the wheels' own height */
    int made = spray_v4_emit(&s, &e);
    made += spray_v4_emit(&s, &e);   /* the same tick again: nothing */
    CHECK(made >= 1 && made <= 2);
    int found = 0;
    for (int i = 0; i < SPRAY_V4_MAX; i++)
        if (s.p[i].live) {
            found++;
            CHECK(s.p[i].y == 3.25f);   /* the ground at its own place, not the wheel's now */
            CHECK(s.p[i].born == 100u);
            CHECK(s.p[i].life <= 120);   /* under two seconds (a tick is a sixtieth) */
        }
    CHECK_EQ_INT(found, made);
}

static void test_the_emitter_runs_each_tick_with_that_ticks_place(void)
{
    static SprayV4 s;
    spray_v4_clear(&s);
    for (uint32_t t = 100; t < 108; t += 2) {   /* four steps: the wheels move on a metre each */
        const SprayV4Emit e = car_at(0, t, 10.0f, 50.0f + (float)(t - 100) / 2.0f, 0.0f, 1, 0);
        spray_v4_emit(&s, &e);
    }
    float zmin = 1e9f, zmax = -1e9f;
    int n = 0;
    for (int i = 0; i < SPRAY_V4_MAX; i++)
        if (s.p[i].live) n++, zmin = s.p[i].z < zmin ? s.p[i].z : zmin, zmax = s.p[i].z > zmax ? s.p[i].z : zmax;
    CHECK(n >= 4);
    CHECK(zmax - zmin > 1.5f);   /* laid along the way the car went, with no extrapolation from the last place */
    SprayV4Emit back = car_at(0, 50, 0, 0, 0, 1, 0);
    spray_v4_emit(&s, &back);   /* the clock went back: the old trail is of another time */
    n = 0;
    for (int i = 0; i < SPRAY_V4_MAX; i++) n += s.p[i].live;
    CHECK(n <= 2);
}

static void test_water_thins_and_opens_gravel_falls_and_lands(void)
{
    static SprayV4 s;
    spray_v4_clear(&s);
    const SprayV4Emit w = car_at(1, 200, 0, 0, 0, 1, 0);
    spray_v4_emit(&s, &w);
    const SprayV4Particle *p = NULL;
    for (int i = 0; i < SPRAY_V4_MAX && p == NULL; i++)
        if (s.p[i].live) p = &s.p[i];
    CHECK(p != NULL);
    SprayV4Look a, b;
    CHECK(spray_v4_look(p, 202, 16, &a));
    CHECK(spray_v4_look(p, 202 + 50, 16, &b));
    CHECK(b.radius > a.radius);   /* it widens */
    CHECK(b.alpha < a.alpha + 0.001f || a.alpha < 0.2f);   /* and thins (the first ticks fade in) */
    CHECK(!spray_v4_look(p, 200 + p->life, 16, &b));   /* gone at the end of its life */
    CHECK(!spray_v4_look(p, 202, 0, &b));              /* a closed zone is dry */
    spray_v4_clear(&s);
    const SprayV4Emit g = car_at(2, 300, 0, 0, 5.0f, 0, 1);   /* dry gravel */
    CHECK_EQ_INT(spray_v4_emit(&s, &g), 12);   /* five stones and a puff of dust a wheel */
    float top = 5.0f;
    for (int i = 0; i < SPRAY_V4_MAX; i++)
        if (s.p[i].live && s.p[i].kind == SPRAY_V4_GRAVEL)
            for (uint32_t t = 300; t < 300u + s.p[i].life; t++) {
                SprayV4Look l;
                if (!spray_v4_look(&s.p[i], t, 0, &l)) continue;   /* open 0: gravel is thrown anywhere */
                CHECK(l.y >= 5.0f);
                if (l.y > top) top = l.y;
            }
    CHECK(top > 5.04f);   /* it was thrown up */
    SprayV4Look end;
    for (int i = 0; i < SPRAY_V4_MAX; i++)
        if (s.p[i].live && s.p[i].kind == SPRAY_V4_GRAVEL && spray_v4_look(&s.p[i], 300u + (uint32_t)s.p[i].life - 1u, 0, &end)) CHECK(end.y < 5.1f);   /* and came down */
}

static void test_a_wheel_in_the_air_or_dry_throws_no_water(void)
{
    static SprayV4 s;
    spray_v4_clear(&s);
    SprayV4Emit e = car_at(0, 10, 0, 0, 0, 0, 0);   /* dry road */
    CHECK_EQ_INT(spray_v4_emit(&s, &e), 0);
    e = car_at(0, 12, 0, 0, 0, 1, 0), e.on_ground = 0;
    CHECK_EQ_INT(spray_v4_emit(&s, &e), 0);
    e = car_at(0, 14, 0, 0, 0, 1, 0), e.speed = 3.0f;
    CHECK_EQ_INT(spray_v4_emit(&s, &e), 0);
}

static void test_the_clouds_are_ragged_and_each_variant_is_its_own(void)
{
    for (int c = 0; c <= SPRAY_V4_CLASSES; c++)
        for (int v = 0; v < (c == SPRAY_V4_CLASSES ? 1 : SPRAY_V4_VARIANTS); v++) {
            const uint8_t *sp = spray_v4_sprite(c, v);
            CHECK(sp != NULL);
            int peak = 0, holes = 0, mid = 0;
            for (int i = 0; i < SPRAY_V4_SPRITE * SPRAY_V4_SPRITE; i++) peak = sp[i] > peak ? sp[i] : peak, holes += sp[i] == 0, mid += sp[i] > 40 && sp[i] < 215;
            CHECK(peak >= 250);
            CHECK(holes > SPRAY_V4_SPRITE * SPRAY_V4_SPRITE / 5);   /* the thin tail is cut: ragged, not a disc */
            CHECK(mid > 200);                                         /* and there are soft parts */
            for (int i = 0; i < SPRAY_V4_SPRITE; i++) {              /* nothing touches the border */
                CHECK(sp[i] == 0 && sp[(SPRAY_V4_SPRITE - 1) * SPRAY_V4_SPRITE + i] == 0);
                CHECK(sp[i * SPRAY_V4_SPRITE] == 0 && sp[i * SPRAY_V4_SPRITE + SPRAY_V4_SPRITE - 1] == 0);
            }
        }
    for (int c = 0; c < SPRAY_V4_CLASSES; c++)   /* the variants differ */
        CHECK(memcmp(spray_v4_sprite(c, 0), spray_v4_sprite(c, 1), (size_t)SPRAY_V4_SPRITE * SPRAY_V4_SPRITE) != 0);
    CHECK(spray_v4_sprite(SPRAY_V4_CLASSES + 1, 0) == NULL && spray_v4_sprite(0, SPRAY_V4_VARIANTS) == NULL);
    const char *sheet = getenv("TNFS_SPRAY_V4_SHEET");
    FILE *f = sheet != NULL ? fopen(sheet, "wb") : NULL;
    if (f != NULL) {   /* a row a variant, a column a class (and the stone's dot last), white on dark */
        enum { CELL = SPRAY_V4_SPRITE + 4, W = (SPRAY_V4_CLASSES + 1) * CELL, H = SPRAY_V4_VARIANTS * CELL };
        fprintf(f, "P6\n%d %d\n255\n", W, H);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const int c = x / CELL, v = y / CELL, px_x = x % CELL, px_y = y % CELL;
                const int a = px_x < SPRAY_V4_SPRITE && px_y < SPRAY_V4_SPRITE && (c < SPRAY_V4_CLASSES || v == 0) ? spray_v4_sprite(c, v)[px_y * SPRAY_V4_SPRITE + px_x] : 0;
                const unsigned char px[3] = { (unsigned char)(24 + a * 220 / 255), (unsigned char)(26 + a * 222 / 255), (unsigned char)(30 + a * 225 / 255) };
                fwrite(px, 1, 3, f);
            }
        fclose(f);
    }
}

int main(void)
{
    test_a_particle_is_born_where_and_when_its_wheel_was();
    test_the_emitter_runs_each_tick_with_that_ticks_place();
    test_water_thins_and_opens_gravel_falls_and_lands();
    test_a_wheel_in_the_air_or_dry_throws_no_water();
    test_the_clouds_are_ragged_and_each_variant_is_its_own();
    return TEST_REPORT();
}
