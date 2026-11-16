/* dep59 step 1: the game's own files never take an added car. A replay's setup naming a car past the twelve is refused, whether or not
 * that number is registered (the original's format has no added car); car_registry_entries_added tells a race with one, of its racers
 * only; the race's mark (car_registry_race_set) is what the records and the replay's save look at. */
#include "game/race/car_registry.h"
#include "game/race/race_replay_image.h"
#include "game/race/race_state.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static RaceState race;
static uint8_t setup[REPLAY_SETUP_SIZE];

static void register_f40(void)
{
    CarPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "f40");
    snprintf(p.stem, sizeof p.stem, "F40");
    p.sound_model = 4, p.ai = (CarAiTuning){ CAR_AI_PACE_MIN, CAR_AI_COAST_DEFAULT }, p.dash_model = -1;
    char why[160];
    car_registry_clear();
    CHECK_EQ_INT(car_registry_add(&p, why, sizeof why), 16);
}

static void test_a_replay_names_no_added_car(void)
{
    memset(&race, 0, sizeof race);
    strcpy(race.track_name, "TR1");
    race.race_type = 0, race.racers = 2, race.humans = 1, race.car_count = 2, race.laps_setting = 2, race.player = 0;
    race.entries[0].car = 5, race.entries[1].car = 3;
    ReplayRace r;
    race_replay_image_setup(&race, 1, setup);
    CHECK(race_replay_image_race(setup, &r));   /* the originals: read */
    race.entries[0].car = 16;
    race_replay_image_setup(&race, 1, setup);
    car_registry_clear();
    CHECK(!race_replay_image_race(setup, &r));   /* no car 16: refused, as 12 is */
    register_f40();
    CHECK(!race_replay_image_race(setup, &r));   /* nor when 16 is registered */
}

static void test_a_race_with_an_added_car(void)
{
    RaceEntry e[RACE_PLAYERS];
    memset(e, 0, sizeof e);
    register_f40();
    for (int i = 0; i < RACE_PLAYERS; i++) e[i].car = i;
    CHECK_EQ_INT(car_registry_entries_added(e, RACE_PLAYERS), 0);
    e[0].car = 16;
    CHECK_EQ_INT(car_registry_entries_added(e, 1), 1);
    e[0].car = 0, e[3].car = 16;
    CHECK_EQ_INT(car_registry_entries_added(e, 3), 0);   /* past the racers: not one of them */
    CHECK_EQ_INT(car_registry_entries_added(e, 4), 1);
    e[3].car = 17;                                         /* not registered: no car, not an added one */
    CHECK_EQ_INT(car_registry_entries_added(e, 4), 0);
    CHECK_EQ_INT(car_registry_entries_added(NULL, 4), 0);
    car_registry_race_set(1);
    CHECK_EQ_INT(car_registry_race_has_added(), 1);
    car_registry_race_set(0);
    CHECK_EQ_INT(car_registry_race_has_added(), 0);
}

int main(void)
{
    test_a_replay_names_no_added_car();
    test_a_race_with_an_added_car();
    return TEST_REPORT();
}
