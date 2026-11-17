/* dep59 Lunar L2: Lunar Springs as an entry (game/frontend/track_pick.h): the menus' track number never holds 10, the pick is (Rusty, rally,
 * the Lunar pick); 10 lives only in the track list and the arrows' step (notes/dep59-lunar-entry-l1l2l5-addendum.md). */
#include "game/frontend/track_pick.h"
#include "formats/stock_content.h"
#include "test.h"

enum { RUSTY = 3, AUTUMN = 4 };

static void test_both_ways(void)
{
    CHECK_EQ_INT(track_pick_of(RUSTY, 1, 1), STOCK_TRACK_LUNAR);
    CHECK_EQ_INT(track_pick_of(RUSTY, 1, 0), RUSTY);   /* Rusty's rally without the pick: the desert */
    CHECK_EQ_INT(track_pick_of(RUSTY, 0, 1), RUSTY);   /* the pick without the rally: no moon */
    CHECK_EQ_INT(track_pick_of(AUTUMN, 1, 1), AUTUMN);
    int track = 0, variant_r = 0, lunar = 0;
    track_pick_set(STOCK_TRACK_LUNAR, &track, &variant_r, &lunar);
    CHECK(track == RUSTY && variant_r == 1 && lunar == 1);
    track_pick_set(AUTUMN, &track, &variant_r, &lunar);   /* another: its number, the rally as it was, no pick */
    CHECK(track == AUTUMN && variant_r == 1 && lunar == 0);
    variant_r = 0;
    track_pick_set(RUSTY, &track, &variant_r, &lunar);
    CHECK(track == RUSTY && variant_r == 0 && lunar == 0);
}

typedef struct {
    int track, m, r, lunar;
} Entry;

static int at(const RaceSettings *s, int lunar, Entry want)
{
    return s->track == want.track && s->variant_m == want.m && s->variant_r == want.r && lunar == want.lunar;
}

/* with the variant switches off the entries are Rusty, Rusty Mirror, Rusty Rally (the desert), Lunar, Lunar Mirror, Autumn: no Lunar Rally,
 * the moon is one */
static void test_the_entries(void)
{
    static const Entry WALK[] = { { RUSTY, 0, 0, 0 }, { RUSTY, 1, 0, 0 }, { RUSTY, 0, 1, 0 }, { RUSTY, 0, 1, 1 }, { RUSTY, 1, 1, 1 },
                                  { AUTUMN, 0, 0, 0 } };
    enum { N = sizeof WALK / sizeof WALK[0] };
    const Unlocks u = { 1, 1, 1, 0, 1 };
    TrackList l;
    track_list_build(&l, NULL, 1, TRACK_LIST_LUNAR_ON);
    RaceSettings s = { 0 };
    int lunar = 0;
    s.track = RUSTY;
    for (int i = 1; i < N; i++) {
        track_pick_step(&s, &lunar, &u, &l, 1, 0);
        CHECK(at(&s, lunar, WALK[i]));
    }
    for (int i = N - 2; i >= 0; i--) {
        track_pick_step(&s, &lunar, &u, &l, -1, 0);
        CHECK(at(&s, lunar, WALK[i]));
    }
    const Unlocks no_mirror = { 0, 1, 1, 0, 1 };   /* an entry the level does not offer is skipped */
    s = (RaceSettings){ 0 }, s.track = RUSTY, s.variant_r = 1, lunar = 1;
    track_pick_step(&s, &lunar, &no_mirror, &l, 1, 0);
    CHECK(at(&s, lunar, (Entry){ AUTUMN, 0, 0, 0 }));
    track_pick_step(&s, &lunar, &no_mirror, &l, -1, 0);
    CHECK(at(&s, lunar, (Entry){ RUSTY, 0, 1, 1 }));
}

/* with the switches on (Restomod's track screen) the arrows walk tracks and the mirror is kept; Lunar is always the rally */
static void test_with_the_switches(void)
{
    const Unlocks u = { 1, 1, 1, 0, 1 };
    TrackList l;
    track_list_build(&l, NULL, 1, TRACK_LIST_LUNAR_ON);
    RaceSettings s = { 0 };
    int lunar = 0;
    s.track = RUSTY, s.variant_m = 1;
    track_pick_step(&s, &lunar, &u, &l, 1, 1);
    CHECK(at(&s, lunar, (Entry){ RUSTY, 1, 1, 1 }));
    track_pick_step(&s, &lunar, &u, &l, 1, 1);
    CHECK(s.track == AUTUMN && s.variant_m == 1 && lunar == 0);
    track_pick_step(&s, &lunar, &u, &l, -1, 1);
    CHECK(s.track == RUSTY && s.variant_r == 1 && lunar == 1);
    track_pick_step(&s, &lunar, &u, &l, -1, 1);
    CHECK(s.track == RUSTY && lunar == 0);
}

/* no Lunar in the list (the switch off, a tournament): Rusty's rally steps on to Autumn, a pick left over is dropped */
static void test_without_lunar(void)
{
    const Unlocks u = { 1, 1, 1, 0, 1 };
    TrackList l;
    track_list_build(&l, NULL, 1, TRACK_LIST_LUNAR_OFF);
    RaceSettings s = { 0 };
    int lunar = 0;
    s.track = RUSTY, s.variant_r = 1;
    track_pick_step(&s, &lunar, &u, &l, 1, 0);
    CHECK(at(&s, lunar, (Entry){ AUTUMN, 0, 0, 0 }));
    s.track = RUSTY, s.variant_r = 1, s.variant_m = 0, lunar = 1;
    track_pick_step(&s, &lunar, &u, &l, 1, 0);   /* 10 not listed: stepped as Rusty's rally */
    CHECK(at(&s, lunar, (Entry){ AUTUMN, 0, 0, 0 }));
    s.track = RUSTY, s.variant_r = 1, lunar = 0;
    track_pick_step(&s, &lunar, &u, NULL, 1, 0);   /* the original's own walk */
    CHECK(at(&s, lunar, (Entry){ AUTUMN, 0, 0, 0 }));
}

/* the pick the race runs: the moon only on Rusty's rally */
static void test_the_race_pick(void)
{
    track_pick_lunar_put(1);
    CHECK_EQ_INT(track_pick_lunar(), 1);
    track_pick_lunar_put(0);
    CHECK_EQ_INT(track_pick_lunar(), 0);
    track_pick_lunar_put(7);
    CHECK_EQ_INT(track_pick_lunar(), 1);
    track_pick_lunar_put(0);
}

/* dep59 Lunar L8: a guest in a link game holds no pick of its own (the host's rules decide); everyone else as before */
static void test_a_guest_holds_no_pick(void)
{
    CHECK_EQ_INT(track_pick_holds(1, RUSTY, 1, 0), 1);       /* solo, or the host: Lunar offered on Rusty's rally */
    CHECK_EQ_INT(track_pick_holds(1, RUSTY, 1, 1), 0);       /* a guest: none */
    CHECK_EQ_INT(track_pick_holds(0, RUSTY, 1, 0), 0);       /* not offered */
    CHECK_EQ_INT(track_pick_holds(1, AUTUMN, 1, 0), 0);      /* another track */
    CHECK_EQ_INT(track_pick_holds(1, RUSTY, 0, 0), 0);       /* no rally */
}

int main(void)
{
    test_both_ways();
    test_the_entries();
    test_with_the_switches();
    test_without_lunar();
    test_the_race_pick();
    test_a_guest_holds_no_pick();
    return TEST_REPORT();
}
