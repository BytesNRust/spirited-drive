/* What a free race guards (free_laps.h): the splits' counters, the scheduler's frame, the AI zones. A free race
 * (port_free_laps = 1) differs from the exe's only past the exe's own limits; below them both are the same. */
#include "game/race/race_schedule.h"
#include "game/race/race_split_times.h"
#include "game/race/race_state.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static RaceState *new_race(void)
{
    RaceState *race = calloc(1, sizeof *race);
    for (int i = 0; i < RACE_SPLITS; i++) race->splits.checkpoints[i] = 10 * (i + 1);
    race->race_type = 0;
    return race;
}

static void test_splits_stop_at_85(void)
{
    for (int on = 0; on <= 1; on++) {
        RaceState *race = new_race();
        race->port_free_laps = on;
        race->splits.passed = RACE_SPLITS - 1;
        race->tick = 7;
        race_splits_pass(race, 1 << 30, 0);   /* the 85th split */
        CHECK_EQ_INT(race->splits.passed, RACE_SPLITS);
        CHECK_EQ_INT(race->splits.passed_ticks[RACE_SPLITS - 1], 7);
        race->tick = 9;
        race_splits_pass(race, 1 << 30, 0);   /* one past the arrays */
        if (on) {
            CHECK_EQ_INT(race->splits.passed, RACE_SPLITS);   /* nothing: the counter stops */
            CHECK_EQ_INT(race->splits.unknown_2c4[0], 0);
        } else {
            CHECK_EQ_INT(race->splits.passed, RACE_SPLITS + 1);   /* the exe's: it went on, into the next words */
            CHECK_EQ_INT(race->splits.unknown_2c4[0], 9);
        }
        free(race);
    }
}

static void test_splits_below_the_limit_are_the_same(void)
{
    RaceState *a = new_race(), *b = new_race();
    a->port_free_laps = 0, b->port_free_laps = 1;
    for (int t = 1; t < 60; t++) {
        a->tick = b->tick = t;
        race_splits_pass(a, t * 7, 0);
        race_splits_pass(b, t * 7, 0);
    }
    CHECK(memcmp(&a->splits, &b->splits, sizeof a->splits) == 0);
    free(a), free(b);
}

static int called;
static void count_call(void *ctx, int which) { (void)ctx, (void)which; called++; }

static void test_scheduler_frame_wraps(void)
{
    const RaceScheduleOps ops = { NULL, count_call };
    RaceScheduled sched[RACE_SCHEDULE_SLOTS] = { { 32760, 0 }, { 0, 0 }, { 0, 0 } };
    const int16_t later = (int16_t)(32770);   /* 10 ticks on: the 16-bit frame went negative */
    called = 0;
    race_schedule_port_wrap = 0;
    race_schedule_run(&ops, sched, later);
    CHECK_EQ_INT(called, 0);   /* the exe: the signed compare says it is not time, for the rest of the race */
    race_schedule_port_wrap = 1;
    race_schedule_run(&ops, sched, later);
    CHECK_EQ_INT(called, 1);
    CHECK_EQ_INT(sched[0].frame, 0);
    sched[0].frame = 100;
    called = 0;
    race_schedule_run(&ops, sched, 99);   /* not yet, with the wrap too */
    CHECK_EQ_INT(called, 0);
    race_schedule_run(&ops, sched, 100);
    CHECK_EQ_INT(called, 1);
    race_schedule_port_wrap = 0;
}

int main(void)
{
    test_splits_stop_at_85();
    test_splits_below_the_limit_are_the_same();
    test_scheduler_frame_wraps();
    return TEST_REPORT();
}
