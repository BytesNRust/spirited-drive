/* port (extras.cfg's rally_gravel, rally_gravel.h, dust_trail.h): the kinds of the rally's road, the dust a rolling car throws, the trail it leaves and
 * the road noise lower; the exe's own numbers when it is off. */#include "game/race/dust_trail.h"
#include "game/race/rally_gravel.h"
#include "game/race/race_state.h"
#include "test.h"

#include <string.h>

enum { TEST_CAMERA = 15, TEST_SCREEN = 27 };   /* skid_marks.c's word offsets of a mark's camera and screen points */

static void test_kinds(void)
{
    CHECK_EQ_INT(rally_gravel_kind(0, 1, 0, 1, 0), RALLY_GRAVEL_COMPACT);    /* City */
    CHECK_EQ_INT(rally_gravel_kind(1, 1, 0, 1, 0), RALLY_GRAVEL_COMPACT);    /* Coastal */
    CHECK_EQ_INT(rally_gravel_kind(2, 1, 0, 1, 0), RALLY_GRAVEL_COMPACT);    /* Alpine */
    CHECK_EQ_INT(rally_gravel_kind(3, 1, 0, 1, 0), RALLY_GRAVEL_DUST);       /* Rusty Springs' desert */
    CHECK_EQ_INT(rally_gravel_kind(3, 1, 1, 1, 0), RALLY_GRAVEL_REGOLITH);   /* with lunar_springs, the moon */
    CHECK_EQ_INT(rally_gravel_kind(3, 0, 1, 1, 0), RALLY_GRAVEL_OFF);        /* not the rally */
    CHECK_EQ_INT(rally_gravel_kind(3, 1, 1, 0, 0), RALLY_GRAVEL_OFF);        /* the switch off: the exe's */
}

static void test_level_and_dust(void)
{
    CHECK_EQ_INT(rally_gravel_level(RALLY_GRAVEL_OFF, 100), 100);
    for (int kind = RALLY_GRAVEL_COMPACT; kind <= RALLY_GRAVEL_REGOLITH; kind++) CHECK(rally_gravel_level(kind, 100) < 100);
    CHECK(rally_gravel_level(RALLY_GRAVEL_REGOLITH, 100) < rally_gravel_level(RALLY_GRAVEL_DUST, 100));
    const int32_t fast = 0x1e0000, slow = 0x10000;
    CHECK(rally_dust_skid(RALLY_GRAVEL_DUST, fast, 1) >= 0x3c0000);              /* a skid the marks take */
    CHECK_EQ_INT(rally_dust_skid(RALLY_GRAVEL_DUST, slow, 1), 0);                 /* standing: none */
    CHECK_EQ_INT(rally_dust_skid(RALLY_GRAVEL_DUST, fast, 0), 0);                 /* in the air: none */
    CHECK_EQ_INT(rally_dust_skid(RALLY_GRAVEL_COMPACT, fast, 1), 0);              /* compacted gravel: only the exe's slides */
    CHECK_EQ_INT(rally_dust_skid(RALLY_GRAVEL_OFF, fast, 1), 0);
    CHECK(rally_dust_skid(RALLY_GRAVEL_REGOLITH, fast, 1) < rally_dust_skid(RALLY_GRAVEL_DUST, fast, 1));   /* a few puffs, not a cloud */
    CHECK(rally_dust_skid(RALLY_GRAVEL_DUST, fast, 1) > rally_dust_skid(RALLY_GRAVEL_DUST, 0x80000, 1));    /* more with speed */
}

/* the rolling crunch: the exe's gravel sample (0x50) struck on the rolling car, oftener and louder with speed, none off or standing, quieter on the moon */
static int strikes(int kind, int32_t speed)
{
    int n = 0;
    for (uint32_t r = 0; r < 0x10000; r += 7) n += rally_gravel_crunch(kind, speed, r) > 0;
    return n;
}

static void test_crunch(void)
{
    const int32_t slow = 0x30000, mid = 0xf0000, fast = 0x1e0000;
    CHECK_EQ_INT(strikes(RALLY_GRAVEL_OFF, fast), 0);                                   /* the exe's: nothing of ours */
    CHECK_EQ_INT(strikes(RALLY_GRAVEL_COMPACT, 0x10000), 0);                            /* standing */
    CHECK_EQ_INT(strikes(RALLY_GRAVEL_COMPACT, -0x10000), 0);
    CHECK(strikes(RALLY_GRAVEL_COMPACT, slow) > 0);
    CHECK(strikes(RALLY_GRAVEL_COMPACT, mid) > strikes(RALLY_GRAVEL_COMPACT, slow));    /* oftener with speed */
    CHECK(strikes(RALLY_GRAVEL_COMPACT, fast) > strikes(RALLY_GRAVEL_COMPACT, mid));
    CHECK(strikes(RALLY_GRAVEL_COMPACT, fast) * 2 < 0x10000 / 7);                      /* never more than every other step */
    int32_t seen = 0;
    for (uint32_t r = 0; r < 0x10000; r++) if (rally_gravel_crunch(RALLY_GRAVEL_COMPACT, fast, r) > seen) seen = rally_gravel_crunch(RALLY_GRAVEL_COMPACT, fast, r);
    CHECK(seen > 0 && seen <= 0x7f);
    int32_t vol_slow = 0, vol_fast = 0, vol_moon = 0;
    for (uint32_t r = 0; r < 0x10000; r++) {
        if (rally_gravel_crunch(RALLY_GRAVEL_COMPACT, slow, r) > 0) vol_slow = rally_gravel_crunch(RALLY_GRAVEL_COMPACT, slow, r);
        if (rally_gravel_crunch(RALLY_GRAVEL_COMPACT, fast, r) > 0) vol_fast = rally_gravel_crunch(RALLY_GRAVEL_COMPACT, fast, r);
        if (rally_gravel_crunch(RALLY_GRAVEL_REGOLITH, fast, r) > 0) vol_moon = rally_gravel_crunch(RALLY_GRAVEL_REGOLITH, fast, r);
    }
    CHECK(vol_fast > vol_slow);                                                          /* louder with speed */
    CHECK(vol_moon < vol_fast);                                                          /* the moon's regolith is quieter */
}

/* the trail: puffs at the two rear wheels each tick the car throws dust, none when it does not, left where they were dropped, gone at the end of
 * their life; the ring clears on a new race; a long pause leaves a gap, not a burst */
static int live(const DustTrail *t, int32_t tick, int kind, int lunar)
{
    int n = 0;
    for (int i = 0; i < DUST_TRAIL_PUFFS; i++) {
        int32_t place[3], size;
        n += dust_puff_at(&t->puff[i], tick, kind, lunar, place, &size);
    }
    return n;
}

static void test_trail(void)
{
    static DustTrail t;
    const int32_t car[3] = { 0, 0x100000, 0x400000 };
    dust_trail_reset(&t);
    dust_trail_step(&t, 100, RALLY_GRAVEL_DUST, 1, car, 0, 0);   /* the first call only starts the clock */
    CHECK_EQ_INT(live(&t, 100, RALLY_GRAVEL_DUST, 0), 0);
    dust_trail_step(&t, 101, RALLY_GRAVEL_DUST, 1, car, 0, 0);
    CHECK_EQ_INT(live(&t, 101, RALLY_GRAVEL_DUST, 0), 2);    /* one at each rear wheel */
    dust_trail_step(&t, 101, RALLY_GRAVEL_DUST, 1, car, 0, 0);
    CHECK_EQ_INT(live(&t, 101, RALLY_GRAVEL_DUST, 0), 2);    /* the same tick drawn again: no more */
    dust_trail_step(&t, 102, RALLY_GRAVEL_DUST, 0, car, 0, 0);
    CHECK_EQ_INT(live(&t, 102, RALLY_GRAVEL_DUST, 0), 2);    /* not throwing: none dropped */
    dust_trail_step(&t, 120, RALLY_GRAVEL_DUST, 1, car, 0, 0);
    CHECK_EQ_INT(live(&t, 120, RALLY_GRAVEL_DUST, 0), 2 + 8);   /* a pause: 4 ticks of puffs, not 18 */
    CHECK_EQ_INT(live(&t, 101 + dust_life(RALLY_GRAVEL_DUST, 0), RALLY_GRAVEL_DUST, 0), 8);   /* the first pair has gone... */
    CHECK_EQ_INT(live(&t, 200, RALLY_GRAVEL_DUST, 0), 0);                                      /* ...and all, in time */
    dust_trail_step(&t, 5, RALLY_GRAVEL_DUST, 1, car, 0, 0);   /* the clock went back: a new race */
    CHECK_EQ_INT(live(&t, 5, RALLY_GRAVEL_DUST, 0), 0);
    dust_trail_step(&t, 6, RALLY_GRAVEL_COMPACT, 1, car, 0, 0);
    CHECK_EQ_INT(live(&t, 6, RALLY_GRAVEL_COMPACT, 0), 0);   /* compacted gravel: no trail */
}

/* ticks stepped at once are dropped along the way the car came, not in one heap: at 30 m/s the puffs of 3 ticks lie 1 m apart in z */
static void test_catch_up_is_spread(void)
{
    static DustTrail t;
    const int32_t car[3] = { 0, 0x100000, 0x400000 };
    dust_trail_reset(&t);
    dust_trail_step(&t, 10, RALLY_GRAVEL_DUST, 1, car, 0, 0);
    dust_trail_step(&t, 13, RALLY_GRAVEL_DUST, 1, car, 0, 30 * 0x10000);   /* heading +z, 30 m/s: 1 m a tick */
    int32_t zs[3] = { 0 };
    for (int i = 0; i < 3; i++) zs[i] = t.puff[2 * i].pos[2];   /* ticks 11, 12, 13: the left wheel's */
    CHECK(zs[0] < zs[1] - 0x8000 && zs[1] < zs[2] - 0x8000);   /* oldest farthest back, about a metre apart (the jitter is under 0.4 m) */
}

/* a puff stays where it was dropped (the car moves on), is low and rises slowly, spreads outward on its own side, and thins toward the end */
static void test_puff(void)
{
    static DustTrail t;
    const int32_t car[3] = { 0x200000, 0x100000, 0x400000 };
    dust_trail_reset(&t);
    dust_trail_step(&t, 10, RALLY_GRAVEL_DUST, 1, car, 0, 0);
    dust_trail_step(&t, 11, RALLY_GRAVEL_DUST, 1, car, 0, 0);   /* heading +z (yaw 0): the wheels are behind, at lower z */
    const DustPuff *left = &t.puff[0], *right = &t.puff[1];
    CHECK(left->pos[2] < car[2] - 0x10000 && right->pos[2] < car[2] - 0x10000);
    CHECK(left->pos[0] != right->pos[0]);   /* a track apart */
    int32_t a[3], b[3], size_a, size_b, size_end;
    CHECK(dust_puff_at(left, 11, RALLY_GRAVEL_DUST, 0, a, &size_a));
    CHECK(dust_puff_at(left, 25, RALLY_GRAVEL_DUST, 0, b, &size_b));
    CHECK(b[1] > a[1] && b[1] - a[1] < 0x10000);   /* risen, but less than a metre */
    CHECK_EQ_INT((b[0] - a[0] > 0) == (left->drift[0] > 0), 1);   /* outward on its own side */
    int32_t c[3];
    CHECK(dust_puff_at(left, 11 + dust_life(RALLY_GRAVEL_DUST, 0) - 1, RALLY_GRAVEL_DUST, 0, c, &size_end));
    CHECK(size_end < size_b);   /* thinned out near its end */
    CHECK(!dust_puff_at(left, 10, RALLY_GRAVEL_DUST, 0, c, &size_end));   /* not yet born */
    CHECK(dust_life(RALLY_GRAVEL_REGOLITH, 1) > dust_life(RALLY_GRAVEL_REGOLITH, 0));   /* the moon's pull: it hangs longer */
    CHECK(dust_life(RALLY_GRAVEL_REGOLITH, 0) > dust_life(RALLY_GRAVEL_DUST, 0));
}

/* the shoulder's scrape at a crawl: the exe's level at 2 m/s is 0x14 + 4 = 24 of 127, hardly there; with the switch a rolling car's scrape has a floor, a
 * standing one's is the exe's, and the exe's own level when off */
static void test_shoulder_scrape_is_heard_at_a_crawl(void)
{
    const int32_t crawl = 4 << 16, standing = 0, exe_crawl = 0x14 + 4, exe_standing = 0x14;
    CHECK_EQ_INT(rally_gravel_shoulder_level(RALLY_GRAVEL_OFF, exe_crawl, crawl), exe_crawl);
    for (int kind = RALLY_GRAVEL_COMPACT; kind <= RALLY_GRAVEL_REGOLITH; kind++) {
        CHECK(rally_gravel_shoulder_level(kind, exe_crawl, crawl) >= RALLY_SHOULDER_FLOOR);
        CHECK_EQ_INT(rally_gravel_shoulder_level(kind, exe_standing, standing), exe_standing);
        CHECK_EQ_INT(rally_gravel_shoulder_level(kind, 0x7f, 60 << 16), 0x7f);   /* fast: the exe's, already loud */
        CHECK_EQ_INT(rally_gravel_shoulder_level(kind, 0, 3 << 15), RALLY_SHOULDER_FLOOR / 2);   /* 1.5 m/s: half the floor */
    }
}

/* the road's rolling gravel at a crawl: the lowered level at 2 m/s is 18 of 127 or less (8 on the moon), then the outside views' 6/10: not heard. A rolling
 * car gets RALLY_ROAD_FLOOR, under the shoulder's floor; standing, fast and switch-off keep the level the caller has */
static void test_road_gravel_is_heard_at_a_crawl(void)
{
    const int32_t crawl = 4 << 16;
    CHECK_EQ_INT(rally_gravel_road_level(RALLY_GRAVEL_OFF, 5, crawl), 5);
    for (int kind = RALLY_GRAVEL_COMPACT; kind <= RALLY_GRAVEL_REGOLITH; kind++) {
        const int32_t low = rally_gravel_level(kind, 0x14 + 4);
        CHECK(low < RALLY_ROAD_FLOOR);
        CHECK_EQ_INT(rally_gravel_road_level(kind, low, crawl), RALLY_ROAD_FLOOR);
        CHECK_EQ_INT(rally_gravel_road_level(kind, 0x14, 0), 0x14);
        CHECK_EQ_INT(rally_gravel_road_level(kind, 0x60, 50 << 16), 0x60);
        CHECK(RALLY_ROAD_FLOOR < RALLY_SHOULDER_FLOOR);
    }
}

/* the rolling tyre scrub: the shoulder's own tyre sound while rolling, its skid value raised with speed, never lowered, none off, standing or in the air */
static void test_tyre_skid(void)
{
    const int32_t fast = 30 << 16, mid = 10 << 16;
    CHECK_EQ_INT(rally_gravel_tyre_skid(RALLY_GRAVEL_OFF, fast, 0, 1), 0);               /* the exe's: its own skid */
    CHECK_EQ_INT(rally_gravel_tyre_skid(RALLY_GRAVEL_OFF, fast, 7, 1), 7);
    CHECK_EQ_INT(rally_gravel_tyre_skid(RALLY_GRAVEL_COMPACT, 0x10000, 0, 1), 0);        /* standing */
    CHECK_EQ_INT(rally_gravel_tyre_skid(RALLY_GRAVEL_COMPACT, fast, 0, 0), 0);           /* in the air */
    CHECK(rally_gravel_tyre_skid(RALLY_GRAVEL_COMPACT, mid, 0, 1) > 0);
    CHECK(rally_gravel_tyre_skid(RALLY_GRAVEL_COMPACT, fast, 0, 1) > rally_gravel_tyre_skid(RALLY_GRAVEL_COMPACT, mid, 0, 1));   /* louder with speed */
    CHECK(rally_gravel_tyre_skid(RALLY_GRAVEL_COMPACT, 90 << 16, 0, 1) <= RALLY_TYRE_TOP);
    CHECK_EQ_INT(rally_gravel_tyre_skid(RALLY_GRAVEL_COMPACT, fast, 0, 1), 45);   /* half the first version's 90: 50 % quieter */
    CHECK_EQ_INT(rally_gravel_tyre_skid(RALLY_GRAVEL_COMPACT, fast, 0x70, 1), 0x70);   /* a skid is the original volume */
    CHECK_EQ_INT(rally_gravel_tyre_skid(RALLY_GRAVEL_COMPACT, mid, 0x70000, 1), 0x70000);   /* a real slide keeps its own, larger skid */
}

/* port (snow_weather): the rally of a snowy track (Alpine 3) is on snow: its own kind, a looser grip than the compacted gravel's, a white powder puff */
static void test_snow_kind(void)
{
    CHECK_EQ_INT(rally_gravel_kind(2, 1, 0, 1, 1), RALLY_GRAVEL_SNOW);    /* Alpine, the rally, snow on */
    CHECK_EQ_INT(rally_gravel_kind(2, 1, 0, 0, 1), RALLY_GRAVEL_SNOW);    /* the road is snow whatever the gravel switch says */
    CHECK_EQ_INT(rally_gravel_kind(2, 0, 0, 1, 1), RALLY_GRAVEL_OFF);     /* not the rally: the road is wet asphalt, not snow */
    CHECK_EQ_INT(rally_gravel_kind(2, 1, 0, 1, 0), RALLY_GRAVEL_COMPACT); /* a track without `snow 1`: as it was */
    CHECK_EQ_INT(rally_gravel_kind(2, 0, 0, 1, 0), RALLY_GRAVEL_OFF);
    CHECK(rally_gravel_level(RALLY_GRAVEL_SNOW, 100) < 100);              /* the gravel's roll, reused */
    CHECK_EQ_INT(rally_gravel_level(RALLY_GRAVEL_SNOW, 100), rally_gravel_level(RALLY_GRAVEL_COMPACT, 100));
    CHECK(rally_gravel_crunch(RALLY_GRAVEL_SNOW, 20 << 16, 0) == rally_gravel_crunch(RALLY_GRAVEL_COMPACT, 20 << 16, 0));
    CHECK_EQ_INT(rally_gravel_tyre_skid(RALLY_GRAVEL_SNOW, 20 << 16, 0, 1), rally_gravel_tyre_skid(RALLY_GRAVEL_COMPACT, 20 << 16, 0, 1));
    const int32_t fast = 30 << 16;
    CHECK(rally_dust_skid(RALLY_GRAVEL_SNOW, fast, 1) > 0);               /* the powder: thrown while it rolls, as the desert's dust is */
    CHECK_EQ_INT(rally_dust_skid(RALLY_GRAVEL_SNOW, fast, 0), 0);         /* not in the air */
    CHECK_EQ_INT(rally_dust_skid(RALLY_GRAVEL_COMPACT, fast, 1), 0);      /* compacted gravel throws none: unchanged */
    DustTrail trail;
    dust_trail_reset(&trail);
    const int32_t at[3] = { 0, 0, 0 };
    dust_trail_step(&trail, 100, RALLY_GRAVEL_SNOW, 1, at, 0, fast);
    dust_trail_step(&trail, 110, RALLY_GRAVEL_SNOW, 1, at, 0, fast);
    int alive = 0;
    for (int i = 0; i < DUST_TRAIL_PUFFS; i++) alive += trail.puff[i].born > 0;
    CHECK(alive > 0);                                                       /* the trail drops puffs on snow */
}

static RaceState snow_race(int kind, int rain)
{
    RaceState race;
    memset(&race, 0, sizeof race);
    race.track = 2, race.track_variant = 2, race.arcade = 1, race.port_rally_gravel = kind, race.port_rain_grip = rain;
    race_surfaces_init(&race);
    return race;
}

/* snow packed on a road: its friction is about half gravel's (about 0.3 against 0.6), so the grip is half the rally's 0x180 and the rolling resistance, half as much again */
static void test_snow_grip(void)
{
    const RaceState gravel = snow_race(RALLY_GRAVEL_COMPACT, 0), snow = snow_race(RALLY_GRAVEL_SNOW, 0);
    for (int i = 0; i < RACE_SURFACES; i++) {
        CHECK_EQ_INT(gravel.surfaces[i].grip, 0x180);                       /* the rally as it was */
        CHECK_EQ_INT(snow.surfaces[i].grip, RALLY_SNOW_GRIP);
        CHECK(snow.surfaces[i].grip < gravel.surfaces[i].grip && snow.surfaces[i].grip * 2 >= gravel.surfaces[i].grip);   /* looser, not ice */
        CHECK(snow.surfaces[i].rolling > gravel.surfaces[i].rolling);
        CHECK_EQ_INT(snow.surfaces[i].off_road, 1);                         /* road and shoulder alike: all of it is loose */
        CHECK_EQ_INT(snow.port_grip_dry[i], RALLY_SNOW_GRIP);
    }
    const RaceState wet = snow_race(RALLY_GRAVEL_SNOW, 1);
    for (int i = 0; i < RACE_SURFACES; i++) CHECK_EQ_INT(wet.surfaces[i].grip, RALLY_SNOW_GRIP);   /* the snow's number is the wet snow's: the rain's factor is not on it twice */
    const RaceState wet_gravel = snow_race(RALLY_GRAVEL_COMPACT, 1);
    for (int i = 0; i < RACE_SURFACES; i++) CHECK(wet_gravel.surfaces[i].grip < 0x180);               /* gravel in rain still loses grip: unchanged */
}

int main(void)
{
    test_tyre_skid();
    test_road_gravel_is_heard_at_a_crawl();
    test_shoulder_scrape_is_heard_at_a_crawl();
    test_kinds();
    test_snow_kind();
    test_snow_grip();
    test_level_and_dust();
    test_crunch();
    test_trail();
    test_puff();
    test_catch_up_is_spread();
    return TEST_REPORT();
}
