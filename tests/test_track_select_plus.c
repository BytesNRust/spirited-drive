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

int main(void)
{
    test_without_variants_it_is_the_originals_walk();
    test_the_variants_follow_their_track();
    test_lost_vegas_has_none();
    test_time_of_day_with_variants();
    return TEST_REPORT();
}
