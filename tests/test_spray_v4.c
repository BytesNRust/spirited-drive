/* The v4 spray's model (race/spray_v4.h) and its sprites (viewer/spray_v4_art.h): a particle is born where and when its wheel was, its look depends on its age alone; the drops and stones fall and land, the mist thins and spreads, the
 * earth floats, the snow settles; each material has its own sprites. */
#include "game/race/spray_v4.h"
#include "game/viewer/spray_v4_art.h"
#include "tests/test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static SprayV4Emit car_at(int car, uint32_t tick, float x, float z, float y, SprayV4Surface surface)
{
    SprayV4Emit e = { car, tick, 7, 30.0f, 1, surface, { { x - 0.7f, y, z }, { x + 0.7f, y, z } }, { 0, 1 }, { 1, 0 } };
    return e;
}

static int count_kind(const SprayV4 *s, int kind)
{
    int n = 0;
    for (int i = 0; i < SPRAY_V4_MAX; i++) n += s->p[i].live && s->p[i].kind == kind;
    return n;
}

static void test_a_particle_is_born_where_and_when_its_wheel_was(void)
{
    static SprayV4 s;
    spray_v4_clear(&s);
    const SprayV4Emit e = car_at(0, 100, 10.0f, 50.0f, 3.25f, SPRAY_V4_WATER);   /* a hill: the wheels' own height */
    int made = spray_v4_emit(&s, &e);
    CHECK_EQ_INT(spray_v4_emit(&s, &e), 0);   /* the same tick again: nothing */
    CHECK(made >= 4);
    int found = 0;
    for (int i = 0; i < SPRAY_V4_MAX; i++)
        if (s.p[i].live) {
            found++;
            CHECK(s.p[i].y == 3.25f);   /* the ground at its own place, not the wheel's now */
            CHECK(s.p[i].born == 100u);
            CHECK(s.p[i].life <= 124);   /* about two seconds at most (a tick is a sixtieth) */
        }
    CHECK_EQ_INT(found, made);
}

static void test_the_emitter_runs_each_tick_with_that_ticks_place(void)
{
    static SprayV4 s;
    spray_v4_clear(&s);
    for (uint32_t t = 100; t < 108; t += 2) {   /* four steps: the wheels move on a metre each */
        const SprayV4Emit e = car_at(0, t, 10.0f, 50.0f + (float)(t - 100) / 2.0f, 0.0f, SPRAY_V4_WATER);
        spray_v4_emit(&s, &e);
    }
    float zmin = 1e9f, zmax = -1e9f;
    for (int i = 0; i < SPRAY_V4_MAX; i++)
        if (s.p[i].live) zmin = s.p[i].z < zmin ? s.p[i].z : zmin, zmax = s.p[i].z > zmax ? s.p[i].z : zmax;
    CHECK(zmax - zmin > 1.5f);   /* laid along the way the car went, with no extrapolation from the last place */
    SprayV4Emit back = car_at(0, 50, 0, 0, 0, SPRAY_V4_WATER);
    spray_v4_emit(&s, &back);   /* the clock went back: the old trail is of another time */
    int n = 0;
    for (int i = 0; i < SPRAY_V4_MAX; i++) n += s.p[i].live;
    CHECK(n <= 16);
    CHECK(count_kind(&s, SPRAY_V4_MIST) <= 2);
}

static void test_water_has_a_mist_and_drops_that_fall_and_vanish_on_the_road(void)
{
    static SprayV4 s;
    spray_v4_clear(&s);
    const SprayV4Emit w = car_at(1, 200, 0, 0, 0, SPRAY_V4_WATER);
    spray_v4_emit(&s, &w);
    CHECK_EQ_INT(count_kind(&s, SPRAY_V4_MIST), 2);   /* one a wheel */
    CHECK(count_kind(&s, SPRAY_V4_DROP) >= 4);
    SprayV4Look a, b;
    const SprayV4Particle *mist = NULL;
    for (int i = 0; i < SPRAY_V4_MAX && mist == NULL; i++)
        if (s.p[i].live && s.p[i].kind == SPRAY_V4_MIST) mist = &s.p[i];
    CHECK(mist != NULL && mist->life >= 40 && mist->life <= 60);   /* about 0.8 s */
    CHECK(spray_v4_look(mist, 204, 16, &a));
    CHECK(spray_v4_look(mist, 204 + 30, 16, &b));
    CHECK(b.radius > a.radius);   /* it widens */
    CHECK(b.y - a.y < 0.5f);      /* and rises little */
    CHECK(b.alpha < 0.95f);
    CHECK(!spray_v4_look(mist, 200 + mist->life, 16, &b));   /* gone at the end of its life */
    CHECK(!spray_v4_look(mist, 204, 0, &b));                 /* a closed zone is dry */
    int landed = 0, drops = 0;
    for (int i = 0; i < SPRAY_V4_MAX; i++) {
        if (!s.p[i].live || s.p[i].kind != SPRAY_V4_DROP) continue;
        drops++;
        float top = 0.0f;
        int gone_at = -1;
        for (uint32_t t = 200; t < 200u + s.p[i].life; t++) {
            SprayV4Look l;
            if (!spray_v4_look(&s.p[i], t, 16, &l)) {
                if (t > 204) { gone_at = (int)(t - 200); break; }
                continue;
            }
            CHECK(l.y >= -0.001f);   /* never under the road */
            if (l.y > top) top = l.y;
        }
        CHECK(top > 0.1f);   /* thrown up */
        if (gone_at > 0) {
            landed++;
            CHECK(gone_at <= 46 && gone_at >= 12);
        }   /* and down on the road within the drop's life (0.13 to 0.6 s) */
    }
    CHECK(landed * 2 >= drops);   /* most of them reach the ground before they fade */
    for (int i = 0; i < SPRAY_V4_MAX; i++)   /* a closed zone has no drops either */
        if (s.p[i].live && s.p[i].kind == SPRAY_V4_DROP) CHECK(!spray_v4_look(&s.p[i], 203, 0, &b));
}

static void test_gravel_falls_and_lands_with_dust(void)
{
    static SprayV4 s;
    spray_v4_clear(&s);
    const SprayV4Emit g = car_at(2, 300, 0, 0, 5.0f, SPRAY_V4_ON_GRAVEL);
    CHECK_EQ_INT(spray_v4_emit(&s, &g), 18);   /* eight stones and a puff of dust a wheel */
    CHECK_EQ_INT(count_kind(&s, SPRAY_V4_GRAVEL), 16);
    CHECK_EQ_INT(count_kind(&s, SPRAY_V4_DUST), 2);
    float top = 5.0f;
    for (int i = 0; i < SPRAY_V4_MAX; i++)
        if (s.p[i].live && s.p[i].kind == SPRAY_V4_GRAVEL) {
            CHECK(s.p[i].size < 0.2f);   /* small */
            for (uint32_t t = 300; t < 300u + s.p[i].life; t++) {
                SprayV4Look l;
                if (!spray_v4_look(&s.p[i], t, 0, &l)) continue;   /* open 0: gravel is thrown anywhere */
                CHECK(l.y >= 5.0f);
                if (l.y > top) top = l.y;
            }
        }
    CHECK(top > 5.04f);   /* it was thrown up */
    SprayV4Look end;
    for (int i = 0; i < SPRAY_V4_MAX; i++)
        if (s.p[i].live && s.p[i].kind == SPRAY_V4_GRAVEL) CHECK(!spray_v4_look(&s.p[i], 300u + s.p[i].life - 1u, 0, &end) || end.y < 5.1f);   /* and came down */
}

static void test_earth_floats_and_lasts_two_seconds_snow_settles_with_lumps(void)
{
    static SprayV4 s;
    spray_v4_clear(&s);
    const SprayV4Emit d = car_at(3, 400, 0, 0, 0, SPRAY_V4_ON_EARTH);
    CHECK_EQ_INT(spray_v4_emit(&s, &d), 4);
    CHECK_EQ_INT(count_kind(&s, SPRAY_V4_EARTH), 4);
    for (int i = 0; i < SPRAY_V4_MAX; i++)
        if (s.p[i].live) {
            CHECK(s.p[i].life >= 100 && s.p[i].life <= 124);   /* 1.7 to 2 s */
            SprayV4Look a, b;
            CHECK(spray_v4_look(&s.p[i], 404, 0, &a) && spray_v4_look(&s.p[i], 404 + 60, 0, &b));
            CHECK(b.y > a.y && b.y - a.y < 1.0f);   /* it rises slowly */
            CHECK(b.radius > 1.5f * a.radius);       /* and spreads a lot */
            CHECK(b.z < a.z);                        /* it drifts backward over the ground (the car goes toward +z) */
        }
    spray_v4_clear(&s);
    const SprayV4Emit n = car_at(4, 500, 0, 0, 0, SPRAY_V4_ON_SNOW);
    CHECK_EQ_INT(spray_v4_emit(&s, &n), 6);   /* a puff of powder and two lumps a wheel */
    CHECK_EQ_INT(count_kind(&s, SPRAY_V4_SNOW), 2);
    CHECK_EQ_INT(count_kind(&s, SPRAY_V4_CLUMP), 4);
    for (int i = 0; i < SPRAY_V4_MAX; i++)
        if (s.p[i].live && s.p[i].kind == SPRAY_V4_CLUMP) CHECK(s.p[i].life <= 30);   /* the lumps' parabola is short */
}

static void test_the_ground_slope_is_that_of_the_way_the_wheels_came(void)
{
    static SprayV4 s;
    spray_v4_clear(&s);
    SprayV4Emit e = car_at(5, 600, 0, 0, 0.0f, SPRAY_V4_ON_GRAVEL);
    spray_v4_emit(&s, &e);
    e = car_at(5, 602, 0, 1.0f, 0.3f, SPRAY_V4_ON_GRAVEL);   /* uphill: 0.3 m in a metre along z */
    spray_v4_emit(&s, &e);
    int seen = 0;
    for (int i = 0; i < SPRAY_V4_MAX; i++)
        if (s.p[i].live && s.p[i].born == 602u) {
            CHECK(fabsf(s.p[i].gz - 0.3f) < 0.01f);
            seen++;
        }
    CHECK(seen > 0);
}

static void test_no_spray_when_dry_in_the_air_or_slow(void)
{
    static SprayV4 s;
    spray_v4_clear(&s);
    SprayV4Emit e = car_at(0, 10, 0, 0, 0, SPRAY_V4_NONE);   /* nothing under the wheels that throws */
    CHECK_EQ_INT(spray_v4_emit(&s, &e), 0);
    e = car_at(0, 12, 0, 0, 0, SPRAY_V4_WATER), e.on_ground = 0;
    CHECK_EQ_INT(spray_v4_emit(&s, &e), 0);
    e = car_at(0, 14, 0, 0, 0, SPRAY_V4_WATER), e.speed = 3.0f;
    CHECK_EQ_INT(spray_v4_emit(&s, &e), 0);
}

static void test_each_material_has_its_own_sprites(void)
{
    for (int f = 0; f < SPRAY_V4_ART_FAMILIES; f++)
        for (int v = 0; v < SPRAY_V4_ART_VARIANTS; v++) {
            const uint8_t *sp = spray_v4_art(f, v);
            CHECK(sp != NULL);
            int peak = 0, solid = 0, holes = 0;
            for (int i = 0; i < SPRAY_V4_ART_SIZE * SPRAY_V4_ART_SIZE; i++) peak = sp[i] > peak ? sp[i] : peak, solid += sp[i] > 200, holes += sp[i] == 0;
            CHECK(peak >= 230);
            CHECK(holes > SPRAY_V4_ART_SIZE * SPRAY_V4_ART_SIZE / 7);   /* a shape, not a square */
            for (int i = 0; i < SPRAY_V4_ART_SIZE; i++) {              /* nothing touches the border */
                CHECK(sp[i] == 0 && sp[(SPRAY_V4_ART_SIZE - 1) * SPRAY_V4_ART_SIZE + i] == 0);
                CHECK(sp[i * SPRAY_V4_ART_SIZE] == 0 && sp[i * SPRAY_V4_ART_SIZE + SPRAY_V4_ART_SIZE - 1] == 0);
            }
            if (f == SPRAY_V4_ART_STONE) CHECK(solid >= 5 && solid < 150 && holes > SPRAY_V4_ART_SIZE * SPRAY_V4_ART_SIZE * 9 / 10);   /* loose gravel: a scatter of single specks, strong ones among them, mostly empty */
            if (f == SPRAY_V4_ART_MIST) CHECK(solid == 0 || solid < SPRAY_V4_ART_SIZE * 8);   /* the mist is soft: no solid core */
        }
    for (int f = 0; f < SPRAY_V4_ART_FAMILIES; f++)   /* the variants differ, and so do the families */
        for (int g = 0; g < SPRAY_V4_ART_FAMILIES; g++)
            for (int v = 1; v < SPRAY_V4_ART_VARIANTS; v++)
                CHECK(memcmp(spray_v4_art(f, 0), spray_v4_art(g, v), SPRAY_V4_ART_SIZE * SPRAY_V4_ART_SIZE) != 0);
    CHECK(spray_v4_art(SPRAY_V4_ART_FAMILIES, 0) == NULL && spray_v4_art(0, SPRAY_V4_ART_VARIANTS) == NULL && spray_v4_art(-1, 0) == NULL);
    for (int v = 0; v < SPRAY_V4_ART_VARIANTS; v++) {   /* the glint is the stone's: a few pixels, inside the stone */
        const uint8_t *gl = spray_v4_art_glint(SPRAY_V4_ART_STONE, v), *st = spray_v4_art(SPRAY_V4_ART_STONE, v);
        CHECK(gl != NULL);
        int lit = 0;
        for (int i = 0; i < SPRAY_V4_ART_SIZE * SPRAY_V4_ART_SIZE; i++) {
            lit += gl[i] != 0;
            CHECK(gl[i] == 0 || st[i] > 200);
        }
        CHECK(lit >= 1 && lit <= 16);
        CHECK(spray_v4_art_glint(SPRAY_V4_ART_SNOW, v) == NULL);
    }
}

static void test_the_snow_has_loose_sparks(void)
{
    const uint8_t *sp = spray_v4_art(SPRAY_V4_ART_SNOW, SPRAY_V4_ART_VARIANTS - 1);
    int alone = 0;   /* a bright pixel with no neighbour: a loose spark */
    for (int y = 1; y < SPRAY_V4_ART_SIZE - 1; y++)
        for (int x = 1; x < SPRAY_V4_ART_SIZE - 1; x++) {
            if (sp[y * SPRAY_V4_ART_SIZE + x] < 250) continue;
            int near = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) near += (dx || dy) && sp[(y + dy) * SPRAY_V4_ART_SIZE + x + dx] > 40;
            alone += near == 0;
        }
    CHECK(alone >= 3);
}

int main(void)
{
    test_a_particle_is_born_where_and_when_its_wheel_was();
    test_the_emitter_runs_each_tick_with_that_ticks_place();
    test_water_has_a_mist_and_drops_that_fall_and_vanish_on_the_road();
    test_gravel_falls_and_lands_with_dust();
    test_earth_floats_and_lasts_two_seconds_snow_settles_with_lumps();
    test_the_ground_slope_is_that_of_the_way_the_wheels_came();
    test_no_spray_when_dry_in_the_air_or_slow();
    test_each_material_has_its_own_sprites();
    test_the_snow_has_loose_sparks();
    return TEST_REPORT();
}
