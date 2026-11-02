/* port (extras.cfg's rally_gravel, rally_gravel.h, dust_trail.h): the kinds of the rally's road, the dust a rolling car throws, the trail it leaves and
 * the road noise lower; the exe's own numbers when it is off. */#include "game/race/dust_trail.h"
#include "game/race/rally_gravel.h"
#include "test.h"

#include <string.h>

enum { TEST_CAMERA = 15, TEST_SCREEN = 27 };   /* skid_marks.c's word offsets of a mark's camera and screen points */

static void test_kinds(void)
{
    CHECK_EQ_INT(rally_gravel_kind(0, 1, 0, 1), RALLY_GRAVEL_COMPACT);    /* City */
    CHECK_EQ_INT(rally_gravel_kind(1, 1, 0, 1), RALLY_GRAVEL_COMPACT);    /* Coastal */
    CHECK_EQ_INT(rally_gravel_kind(2, 1, 0, 1), RALLY_GRAVEL_COMPACT);    /* Alpine */
    CHECK_EQ_INT(rally_gravel_kind(3, 1, 0, 1), RALLY_GRAVEL_DUST);       /* Rusty Springs' desert */
    CHECK_EQ_INT(rally_gravel_kind(3, 1, 1, 1), RALLY_GRAVEL_REGOLITH);   /* with lunar_springs, the moon */
    CHECK_EQ_INT(rally_gravel_kind(3, 0, 1, 1), RALLY_GRAVEL_OFF);        /* not the rally */
    CHECK_EQ_INT(rally_gravel_kind(3, 1, 1, 0), RALLY_GRAVEL_OFF);        /* the switch off: the exe's */
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
    const int32_t crawl = 2 << 16, standing = 0, exe_crawl = 0x14 + 4, exe_standing = 0x14;
    CHECK_EQ_INT(rally_gravel_shoulder_level(RALLY_GRAVEL_OFF, exe_crawl, crawl), exe_crawl);
    for (int kind = RALLY_GRAVEL_COMPACT; kind <= RALLY_GRAVEL_REGOLITH; kind++) {
        CHECK(rally_gravel_shoulder_level(kind, exe_crawl, crawl) >= RALLY_SHOULDER_FLOOR);
        CHECK_EQ_INT(rally_gravel_shoulder_level(kind, exe_standing, standing), exe_standing);
        CHECK_EQ_INT(rally_gravel_shoulder_level(kind, 0x7f, 60 << 16), 0x7f);   /* fast: the exe's, already loud */
    }
}

int main(void)
{
    test_shoulder_scrape_is_heard_at_a_crawl();
    test_kinds();
    test_level_and_dust();
    test_trail();
    test_puff();
    test_catch_up_is_spread();
    return TEST_REPORT();
}
