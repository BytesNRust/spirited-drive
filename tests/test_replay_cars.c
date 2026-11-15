/* dep59 1b: a replay's added cars by id and fingerprint (race_replay_image.h's TNFSCAR1 block): written from the race's field, read back
 * into today's car numbers; a missing car or one whose fingerprint changed is said with its id; a block that is not exactly one of ours
 * is refused. The setup naming a car past the twelve still needs the block: without it, it is no replay. */
#include "game/race/car_registry.h"
#include "game/race/race_replay_image.h"
#include "game/race/race_state.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void register_car(const char *id, const char *stem, uint32_t print, int want)
{
    CarPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id);
    snprintf(p.stem, sizeof p.stem, "%s", stem);
    p.sound_model = 5, p.ai_model = 5, p.dash_model = -1, p.fingerprint = print;
    CHECK_EQ_INT(car_registry_add(&p, NULL, 0), want);
}

static RaceField field_of(int car0, int car1)
{
    RaceField f;
    memset(&f, 0, sizeof f);
    f.racers = 2;
    f.entries[0].car = car0, f.entries[1].car = car1;
    return f;
}

static void test_there_and_back(void)
{
    uint8_t block[REPLAY_CARS_BLOCK];
    char id[REPLAY_CARS_ID];
    car_registry_clear();
    register_car("f40", "F40", 0x1234, 16);
    RaceField f = field_of(16, 5);
    race_replay_cars_write(block, &f);
    CHECK(memcmp(block, "TNFSCAR1", 8) == 0);
    f.entries[0].car = 99;
    CHECK_EQ_INT(race_replay_cars_read(block, &f, id), REPLAY_CARS_OK);
    CHECK_EQ_INT(f.entries[0].car, 16);
    CHECK_EQ_INT(f.entries[1].car, 5);
    car_registry_clear();   /* another order: f40 is 17 now */
    register_car("other", "OTHER", 0x5555, 16);
    register_car("f40", "F40", 0x1234, 17);
    f.entries[0].car = 99;
    CHECK_EQ_INT(race_replay_cars_read(block, &f, id), REPLAY_CARS_OK);
    CHECK_EQ_INT(f.entries[0].car, 17);
    car_registry_clear();   /* gone */
    CHECK_EQ_INT(race_replay_cars_read(block, &f, id), REPLAY_CARS_MISSING);
    CHECK(strcmp(id, "f40") == 0);
    register_car("f40", "F40", 0x9999, 16);   /* another piece of the same id */
    CHECK_EQ_INT(race_replay_cars_read(block, &f, id), REPLAY_CARS_CHANGED);
    CHECK(strcmp(id, "f40") == 0);
    RaceField originals = field_of(3, 5);   /* no added car: every entry zeros */
    race_replay_cars_write(block, &originals);
    for (int i = 8; i < REPLAY_CARS_BLOCK; i++) CHECK_EQ_INT(block[i], 0);
    CHECK_EQ_INT(race_replay_cars_read(block, &originals, id), REPLAY_CARS_OK);
    CHECK_EQ_INT(originals.entries[0].car, 3);
}

static void test_bad_blocks(void)
{
    uint8_t good[REPLAY_CARS_BLOCK], block[REPLAY_CARS_BLOCK];
    char id[REPLAY_CARS_ID];
    car_registry_clear();
    register_car("f40", "F40", 0x1234, 16);
    RaceField f = field_of(16, 5);
    race_replay_cars_write(good, &f);
    memcpy(block, good, sizeof block), block[7] = '2';                            /* the magic */
    CHECK_EQ_INT(race_replay_cars_read(block, &f, id), REPLAY_CARS_BAD);
    memcpy(block, good, sizeof block), memset(block + 8, 'a', REPLAY_CARS_ID);   /* no NUL in its 33 bytes */
    CHECK_EQ_INT(race_replay_cars_read(block, &f, id), REPLAY_CARS_BAD);
    memcpy(block, good, sizeof block), block[8] = 'F';                            /* "F40": not a mod's id */
    CHECK_EQ_INT(race_replay_cars_read(block, &f, id), REPLAY_CARS_BAD);
    memcpy(block, good, sizeof block), block[8 + REPLAY_CARS_ID + 4 + REPLAY_CARS_ID] = 1;   /* an original's entry, its print not 0 */
    CHECK_EQ_INT(race_replay_cars_read(block, &f, id), REPLAY_CARS_BAD);
    CHECK_EQ_INT(race_replay_cars_read(good, &f, id), REPLAY_CARS_OK);
}

static void test_the_setup_needs_the_block(void)
{
    static RaceState race;
    static uint8_t setup[REPLAY_SETUP_SIZE];
    uint8_t block[REPLAY_CARS_BLOCK];
    char id[REPLAY_CARS_ID];
    car_registry_clear();
    register_car("f40", "F40", 0x1234, 16);
    memset(&race, 0, sizeof race);
    strcpy(race.track_name, "TR1");
    race.race_type = 0, race.racers = 2, race.humans = 1, race.car_count = 2, race.laps_setting = 2, race.player = 0;
    race.entries[0].car = 16, race.entries[1].car = 5;
    race_replay_image_setup(&race, 1, setup);
    ReplayRace r;
    CHECK(!race_replay_image_race(setup, &r));   /* without the block: no replay, as before */
    RaceField f = field_of(16, 5);
    race_replay_cars_write(block, &f);
    CHECK_EQ_INT(race_replay_image_race_cars(setup, block, &r, id), REPLAY_CARS_OK);
    CHECK_EQ_INT(r.field.entries[0].car, 16);
    CHECK_EQ_INT(r.field.entries[1].car, 5);
    CHECK(strcmp(r.stem, "TR1") == 0);
    car_registry_clear();
    CHECK_EQ_INT(race_replay_image_race_cars(setup, block, &r, id), REPLAY_CARS_MISSING);
    RaceField originals = field_of(3, 5);   /* the block says slot 0 is an original: the setup's 16 is no car */
    race_replay_cars_write(block, &originals);
    CHECK_EQ_INT(race_replay_image_race_cars(setup, block, &r, id), REPLAY_CARS_BAD);
}

int main(void)
{
    test_there_and_back();
    test_bad_blocks();
    test_the_setup_needs_the_block();
    car_registry_clear();
    return TEST_REPORT();
}
