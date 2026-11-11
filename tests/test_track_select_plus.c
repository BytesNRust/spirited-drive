/* The track screen's variant entries (track_select.c's port additions): the order Prev and Next step through. */
#include "game/frontend/track_select.h"
#include "game/race/track_fam.h"
#include "test.h"

static void step(int track, int variant, int direction, int lost, int m, int r, int want_track, int want_variant)
{
    int t = -1, v = -1;
    track_select_entry(track, variant, direction, lost, m, r, &t, &v);
    CHECK_EQ_INT(t, want_track);
    CHECK_EQ_INT(v, want_variant);
}

static void test_without_variants_it_is_the_originals_walk(void)
{
    step(0, 0, 1, 0, 0, 0, 1, 0);
    step(7, 0, 1, 0, 0, 0, 0, 0);   /* round, Lost Vegas not there */
    step(7, 0, 1, 1, 0, 0, 8, 0);
    step(8, 0, 1, 1, 0, 0, 0, 0);
    step(0, 0, -1, 0, 0, 0, 7, 0);
    step(0, 0, -1, 1, 0, 0, 8, 0);
    step(5, 0, -1, 0, 0, 0, 4, 0);
}

static void test_the_variants_follow_their_track(void)
{
    step(0, 0, 1, 0, 1, 0, 0, 1);   /* mirror only */
    step(0, 1, 1, 0, 1, 0, 1, 0);
    step(1, 0, -1, 0, 1, 0, 0, 1);
    step(0, 0, -1, 0, 1, 0, 7, 1);
    step(0, 0, 1, 0, 1, 1, 0, 1);   /* both */
    step(0, 1, 1, 0, 1, 1, 0, 2);
    step(0, 2, 1, 0, 1, 1, 1, 0);
    step(1, 0, -1, 0, 1, 1, 0, 2);
    step(0, 2, -1, 0, 1, 1, 0, 1);
    step(0, 1, -1, 0, 1, 1, 0, 0);
    step(0, 0, 1, 0, 0, 1, 0, 2);   /* rally only: the mirror is skipped */
}

static void test_lost_vegas_has_none(void)
{
    step(7, 1, 1, 1, 1, 1, 7, 2);
    step(7, 2, 1, 1, 1, 1, 8, 0);
    step(8, 0, 1, 1, 1, 1, 0, 0);
    step(8, 0, -1, 1, 1, 1, 7, 2);   /* back from Lost Vegas: the last of Tropic's */
    step(0, 0, -1, 1, 1, 1, 8, 0);
}

/* the time of day: the original hides it on Lost Vegas and with a variant; the port's variant_time offers it with
 * one, save Rusty Springs' rally (TR5, which has no evening art) */
static void test_time_of_day_with_variants(void)
{
    CHECK_EQ_INT(track_fam_time_offered(1, 0, 0, 0), 1);
    CHECK_EQ_INT(track_fam_time_offered(1, 1, 0, 0), 0);
    CHECK_EQ_INT(track_fam_time_offered(1, 0, 1, 0), 0);
    CHECK_EQ_INT(track_fam_time_offered(1, 1, 1, 1), 1);
    CHECK_EQ_INT(track_fam_time_offered(1, 1, 0, 1), 1);
    CHECK_EQ_INT(track_fam_time_offered(3, 1, 0, 1), 1);
    CHECK_EQ_INT(track_fam_time_offered(3, 0, 1, 1), 0);
    CHECK_EQ_INT(track_fam_time_offered(8, 0, 0, 1), 0);
}

/* the weather row and the console version time (light_time 0 day, 1 dawn, 2 dusk, 3 night; weather 0 clear, 1 fog, 2 rain, 3 storm) */
static void test_weather_and_time_cycles(void)
{
    CHECK_EQ_INT(track_select_weather_step(0, 1), 1);
    CHECK_EQ_INT(track_select_weather_step(1, 1), 2);
    CHECK_EQ_INT(track_select_weather_step(2, 1), 3);
    CHECK_EQ_INT(track_select_weather_step(3, 1), 0);
    CHECK_EQ_INT(track_select_weather_step(0, -1), 3);
    CHECK_EQ_INT(track_select_light_time_step(0, 1), 1);   /* day -> dawn -> dusk -> night -> day */
    CHECK_EQ_INT(track_select_light_time_step(1, 1), 2);
    CHECK_EQ_INT(track_select_light_time_step(2, 1), 3);
    CHECK_EQ_INT(track_select_light_time_step(3, 1), 0);
    CHECK_EQ_INT(track_select_light_time_step(0, -1), 3);
    CHECK_EQ_INT(track_select_light_time_art(1), 0);   /* dawn shows the morning art, day midday, dusk evening */
    CHECK_EQ_INT(track_select_light_time_art(0), 1);
    CHECK_EQ_INT(track_select_light_time_art(2), 2);
    CHECK_EQ_INT(track_select_light_time_art(3), 2);   /* night shows the evening art, darkened (track_select.c) */
}

/* the arrows with the weather row: DONE -> TIME_OF_DAY -> WEATHER -> DONE going up and down; a hidden time is skipped */
static void test_weather_in_the_arrow_chain(void)
{
    enum { DONE = 0, LAPS = 7, TIME = 9, WEATHER = 10 };
    CHECK_EQ_INT(track_select_arrow_plus(TRACK_SELECT_KEY_DOWN, TIME, DONE, 1), WEATHER);
    CHECK_EQ_INT(track_select_arrow_plus(TRACK_SELECT_KEY_DOWN, LAPS, DONE, 1), WEATHER);    /* time hidden: Laps' down was Done */
    CHECK_EQ_INT(track_select_arrow_plus(TRACK_SELECT_KEY_DOWN, WEATHER, 7, 1), DONE);
    CHECK_EQ_INT(track_select_arrow_plus(TRACK_SELECT_KEY_UP, DONE, TIME, 1), WEATHER);
    CHECK_EQ_INT(track_select_arrow_plus(TRACK_SELECT_KEY_UP, WEATHER, TIME, 1), TIME);
    CHECK_EQ_INT(track_select_arrow_plus(TRACK_SELECT_KEY_LEFT, WEATHER, 0, 1), 3);          /* Best, as the time row's */
    CHECK_EQ_INT(track_select_arrow_plus(TRACK_SELECT_KEY_RIGHT, WEATHER, 0, 1), 2);         /* Next */
    CHECK_EQ_INT(track_select_arrow_plus(TRACK_SELECT_KEY_DOWN, TIME, DONE, 0), DONE);       /* without the row: the original's */
    CHECK_EQ_INT(track_select_arrow_plus(TRACK_SELECT_KEY_DOWN, 4, 9, 1), 9);                /* other moves stay */
    /* the Reverse row (11): right of the weather, or alone */
    CHECK_EQ_INT(track_select_arrow_reverse(TRACK_SELECT_KEY_RIGHT, WEATHER, 0, 2, 1, 1), 11);
    CHECK_EQ_INT(track_select_arrow_reverse(TRACK_SELECT_KEY_LEFT, 11, 0, 0, 1, 1), WEATHER);
    CHECK_EQ_INT(track_select_arrow_reverse(TRACK_SELECT_KEY_RIGHT, 11, 0, 0, 1, 1), 2);        /* Next */
    CHECK_EQ_INT(track_select_arrow_reverse(TRACK_SELECT_KEY_DOWN, 11, 0, WEATHER, 1, 1), DONE);
    CHECK_EQ_INT(track_select_arrow_reverse(TRACK_SELECT_KEY_UP, 11, TIME, TIME, 1, 1), TIME);
    CHECK_EQ_INT(track_select_arrow_reverse(TRACK_SELECT_KEY_LEFT, 11, 0, 0, 0, 1), 3);         /* no weather: Best */
    CHECK_EQ_INT(track_select_arrow_reverse(TRACK_SELECT_KEY_DOWN, TIME, DONE, DONE, 0, 1), 11);
    CHECK_EQ_INT(track_select_arrow_reverse(TRACK_SELECT_KEY_UP, DONE, TIME, TIME, 0, 1), 11);
    CHECK_EQ_INT(track_select_arrow_reverse(TRACK_SELECT_KEY_DOWN, TIME, DONE, WEATHER, 1, 1), WEATHER);   /* the weather first */
    CHECK_EQ_INT(track_select_arrow_reverse(TRACK_SELECT_KEY_DOWN, TIME, DONE, DONE, 0, 0), DONE);          /* the row off: the moves' */
}

int main(void)
{
    test_weather_and_time_cycles();
    test_weather_in_the_arrow_chain();
    test_without_variants_it_is_the_originals_walk();
    test_the_variants_follow_their_track();
    test_lost_vegas_has_none();
    test_time_of_day_with_variants();
    /* the race's time (a replay's or the menus'): the one asked without a variant, midday with one unless offered */
    CHECK_EQ_INT(track_fam_time_of(1, 0, 0, 0, 2), 2);
    CHECK_EQ_INT(track_fam_time_of(1, 1, 0, 0, 2), 1);
    CHECK_EQ_INT(track_fam_time_of(1, 1, 1, 1, 2), 2);
    CHECK_EQ_INT(track_fam_time_of(3, 0, 1, 1, 2), 1);
    CHECK_EQ_INT(track_fam_time_of(8, 0, 0, 1, 2), 2);
    return TEST_REPORT();
}
