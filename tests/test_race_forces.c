/* The force-feedback jolts (race_forces.h race_jolt) against the start clock: the exe's timer (004bb8e0) runs in every
 * race, cheat or not, so a jolt more than the 25-tick window after the last goes through without the NITRO cheat. */
#include "game/race/race_forces.h"
#include "game/race/start_keys.h"
#include "test.h"

#include <string.h>

enum { GAP = 0x19, HIT = 0x1000, RACE_TICKS = 120, JOLT_TICK = 60 };

static int tones;

static void tone(void *user, int32_t strength, int32_t from, int32_t time)
{
    (void)user, (void)strength, (void)from, (void)time;
    tones++;
}

static RaceSounds sounds_with_tone(void)
{
    RaceSounds s;
    memset(&s, 0, sizeof s);
    s.tone = tone;
    return s;
}

/* the window: a jolt at a clock within the gap of the last one is dropped, one past it goes through */
static void test_a_jolt_past_the_window_goes_through(void)
{
    RaceForces f = { 0 };
    const RaceSounds s = sounds_with_tone();
    tones = 0;
    race_jolt(&f, 26, 0, 0, 0, HIT, &s);
    CHECK_EQ_INT(tones, 1);
    race_jolt(&f, 26 + GAP - 1, 0, 0, 0, HIT, &s);   /* inside the window: dropped */
    CHECK_EQ_INT(tones, 1);
    race_jolt(&f, 26 + GAP + 1, 0, 0, 0, HIT, &s);   /* past it: through */
    CHECK_EQ_INT(tones, 2);
}

/* The port's clock is the start clock fed each step (start_keys.h start_clock_fill) with no cheat on: a hit in a
 * race goes through. The clock held at 0 (the port before the fix) dropped every jolt. */
static void test_jolts_go_through_without_the_cheat(void)
{
    RaceState race;
    memset(&race, 0, sizeof race);
    race.start_input = 0;   /* no NITRO cheat */
    RaceForces f = { 0 };
    const RaceSounds s = sounds_with_tone();
    tones = 0;
    for (int32_t ticks = 0; ticks < RACE_TICKS * 2; ticks += 2) {   /* the input clock: two per step */
        start_clock_fill(&race.start_clock, ticks);
        if (ticks == JOLT_TICK * 2) race_jolt(&f, race.start_clock, 0, 0, 0, HIT, &s);
    }
    CHECK_EQ_INT(tones, 1);
    CHECK_EQ_INT(race.start_clock, (RACE_TICKS * 2) - 2);
}

static void test_a_clock_held_at_zero_drops_the_jolt(void)
{
    RaceForces f = { 0 };
    const RaceSounds s = sounds_with_tone();
    tones = 0;
    race_jolt(&f, 0, 0, 0, 0, HIT, &s);
    CHECK_EQ_INT(tones, 0);
}

/* port (replay_no_ffb): the stick stays still in a replay with the flag on, and only then */
static void test_a_replay_is_silent_only_with_the_flag(void)
{
    RaceState race;
    memset(&race, 0, sizeof race);
    CHECK_EQ_INT(race_forces_replay_silent(&race, 1), 0);   /* a live race shakes */
    CHECK_EQ_INT(race_forces_replay_silent(&race, 0), 0);
    race.replay = 1;
    CHECK_EQ_INT(race_forces_replay_silent(&race, 1), 1);
    CHECK_EQ_INT(race_forces_replay_silent(&race, 0), 0);   /* the exe's: a replay shakes as well */
    CHECK_EQ_INT(race_forces_replay_silent(NULL, 1), 0);
}

static void test_the_replay_clears_once_as_it_begins(void)
{
    int was = 0;
    CHECK_EQ_INT(race_forces_edge(&was, 0), 0);
    CHECK_EQ_INT(race_forces_edge(&was, 1), 1);   /* the replay begins */
    CHECK_EQ_INT(race_forces_edge(&was, 1), 0);   /* and not again while it runs */
    CHECK_EQ_INT(race_forces_edge(&was, 0), 0);
    CHECK_EQ_INT(race_forces_edge(&was, 1), 1);   /* a second replay clears again */
}

int main(void)
{
    test_a_jolt_past_the_window_goes_through();
    test_jolts_go_through_without_the_cheat();
    test_a_clock_held_at_zero_drops_the_jolt();
    test_a_replay_is_silent_only_with_the_flag();
    test_the_replay_clears_once_as_it_begins();
    return TEST_REPORT();
}
