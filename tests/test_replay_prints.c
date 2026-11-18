/* dep59 records by print, step 4: a Replay.rpx made now ends with the pieces' prints (race_replay_image.h's TNFSPRN1, after the cars', the
 * looks' and the track's blocks): the prints of the added cars of its entries and of the added track, the SHA-256 the records are
 * kept under (game/sys/piece_print.h). A replay with it asks "changed" by the print; one without it (made before) by the CRC-32 as it
 * did; a stored print that is none (a piece with no print when it was made) falls back to the CRC-32 too. A trailer that is not one of ours
 * is no replay. */
#include "game/race/car_registry.h"
#include "game/race/race_replay_image.h"
#include "game/race/race_state.h"
#include "game/sys/track_registry.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

enum { CARS_END = REPLAY_CARS_AT + REPLAY_CARS_BLOCK, LOOKS_END = CARS_END + REPLAY_LOOKS_BLOCK, TRACK_END = LOOKS_END + REPLAY_TRACK_BLOCK,
       WHOLE = TRACK_END + REPLAY_PRINTS_BLOCK };

static void register_track(const char *id, const char *stem, uint32_t crc, int print_byte)
{
    TrackPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id);
    snprintf(p.name, sizeof p.name, "%s", id);
    snprintf(p.stem, sizeof p.stem, "%s", stem);
    p.spec.kind = TRACK_KIND_STAGE, p.spec.segments = 1, p.spec.times = TRACK_TIME_DAY;
    p.fingerprint = crc;
    memset(p.print, print_byte, sizeof p.print);
    char why[160];
    CHECK_EQ_INT(track_registry_add(&p, why, sizeof why), 16);
}

static void register_car(const char *id, const char *stem, uint32_t crc, int print_byte)
{
    CarPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id);
    snprintf(p.stem, sizeof p.stem, "%s", stem);
    p.sound_model = 5, p.ai = (CarAiTuning){ CAR_AI_PACE_MIN, CAR_AI_COAST_DEFAULT }, p.dash_model = -1, p.fingerprint = crc;
    memset(p.print, print_byte, sizeof p.print);
    CHECK_EQ_INT(car_registry_add(&p, NULL, 0), 16);
}

/* a Replay.rpx of a race on `stem` whose first car is `car0`; with `track` the track's block, and the prints' block at its end. Returns its size;
 * `prints` 0: none (a file made before) */
static size_t make_file(uint8_t *file, const char *stem, int car0, const TrackPiece *track, int prints)
{
    static RaceState race;
    memset(&race, 0, sizeof race);
    snprintf(race.track_name, sizeof race.track_name, "%s", stem);
    race.race_type = 0, race.racers = 2, race.humans = 1, race.car_count = 2, race.laps_setting = 1, race.player = 0;
    race.entries[0].car = car0, race.entries[1].car = 5;
    race.track = track != NULL ? 16 : 0;
    memset(file, 0, WHOLE);
    race_replay_image_setup(&race, 1, file);
    RaceField f;
    memset(&f, 0, sizeof f);
    f.racers = 2, f.entries[0].car = car0, f.entries[1].car = 5;
    race_replay_cars_write(file + REPLAY_CARS_AT, &f);
    ReplayLook looks[RACE_PLAYERS];
    memset(looks, 0, sizeof looks);
    race_replay_looks_put(looks, file + CARS_END);
    size_t size = LOOKS_END;
    if (track != NULL) race_replay_track_write(file + size, track), size += REPLAY_TRACK_BLOCK;
    if (prints) {
        race_replay_prints_put_cars(file + size, &f);
        race_replay_prints_put_track(file + size, track);
        size += REPLAY_PRINTS_BLOCK;
    }
    return size;
}

static void test_the_block(void)
{
    uint8_t block[REPLAY_PRINTS_BLOCK];
    car_registry_clear();
    track_registry_clear();
    register_car("f40", "F40", 0x1234, 3);
    register_track("alps-run", "PEAKS", 0x5678, 7);
    RaceField f;
    memset(&f, 0, sizeof f);
    f.racers = 2, f.entries[0].car = 16, f.entries[1].car = 5;
    memset(block, 0xAA, sizeof block);
    race_replay_prints_put_cars(block, &f);
    race_replay_prints_put_track(block, track_registry_piece(16));
    CHECK(memcmp(block, "TNFSPRN1", 8) == 0);
    CHECK_EQ_INT(REPLAY_PRINTS_BLOCK, 8 + (RACE_PLAYERS + 1) * 32);
    uint8_t three[32], seven[32], none[32];
    memset(three, 3, 32), memset(seven, 7, 32), memset(none, 0, 32);
    CHECK(memcmp(block + 8, three, 32) == 0);                      /* slot 0: the added car's print */
    CHECK(memcmp(block + 8 + 32, none, 32) == 0);                  /* slot 1: an original: none */
    CHECK(memcmp(block + 8 + 2 * 32, none, 32) == 0);              /* an entry past the racers: none */
    CHECK(memcmp(block + 8 + RACE_PLAYERS * 32, seven, 32) == 0);  /* the last: the track's */
    race_replay_prints_put_track(block, NULL);                     /* no added track: none, the cars' kept */
    CHECK(memcmp(block + 8 + RACE_PLAYERS * 32, none, 32) == 0 && memcmp(block + 8, three, 32) == 0);
}

static void test_a_track_by_its_print(void)
{
    static uint8_t file[WHOLE];
    char id[REPLAY_CARS_ID];
    ReplayRace r;
    car_registry_clear();
    track_registry_clear();
    register_track("alps-run", "PEAKS", 0x5678, 7);
    const size_t size = make_file(file, "peaks1", 3, track_registry_piece(16), 1);
    CHECK_EQ_INT(size, TRACK_END + REPLAY_PRINTS_BLOCK);
    CHECK_EQ_INT(race_replay_file_cars(file, size, &r, id), REPLAY_CARS_OK);
    CHECK_EQ_INT(r.added_track, 16);
    track_registry_clear();   /* the same CRC-32, another print: changed */
    register_track("alps-run", "PEAKS", 0x5678, 8);
    CHECK_EQ_INT(race_replay_file_cars(file, size, &r, id), REPLAY_CARS_CHANGED);
    CHECK(strcmp(id, "alps-run") == 0 && race_replay_file_track_changed(file, size));
    track_registry_clear();   /* another CRC-32, the same print: the print decides */
    register_track("alps-run", "PEAKS", 0x1111, 7);
    CHECK_EQ_INT(race_replay_file_cars(file, size, &r, id), REPLAY_CARS_OK);
    CHECK(!race_replay_file_track_changed(file, size));
    track_registry_clear();   /* gone */
    CHECK_EQ_INT(race_replay_file_cars(file, size, &r, id), REPLAY_CARS_MISSING);
}

static void test_a_replay_before_the_prints_goes_by_the_crc(void)
{
    static uint8_t file[WHOLE];
    char id[REPLAY_CARS_ID];
    ReplayRace r;
    car_registry_clear();
    track_registry_clear();
    register_track("alps-run", "PEAKS", 0x5678, 7);
    const size_t size = make_file(file, "peaks1", 3, track_registry_piece(16), 0);
    CHECK_EQ_INT(size, TRACK_END);
    track_registry_clear();   /* another print, the same CRC-32: as it was */
    register_track("alps-run", "PEAKS", 0x5678, 8);
    CHECK_EQ_INT(race_replay_file_cars(file, size, &r, id), REPLAY_CARS_OK);
    track_registry_clear();
    register_track("alps-run", "PEAKS", 0x9999, 7);
    CHECK_EQ_INT(race_replay_file_cars(file, size, &r, id), REPLAY_CARS_CHANGED);
    CHECK(race_replay_file_track_changed(file, size));
}

static void test_no_stored_print_falls_back_to_the_crc(void)
{
    static uint8_t file[WHOLE];
    char id[REPLAY_CARS_ID];
    ReplayRace r;
    car_registry_clear();
    track_registry_clear();
    register_track("alps-run", "PEAKS", 0x5678, 0);   /* no print when it was made */
    const size_t size = make_file(file, "peaks1", 3, track_registry_piece(16), 1);
    track_registry_clear();
    register_track("alps-run", "PEAKS", 0x5678, 9);
    CHECK_EQ_INT(race_replay_file_cars(file, size, &r, id), REPLAY_CARS_OK);
    track_registry_clear();
    register_track("alps-run", "PEAKS", 0x4444, 9);
    CHECK_EQ_INT(race_replay_file_cars(file, size, &r, id), REPLAY_CARS_CHANGED);
}

static void test_a_car_by_its_print(void)
{
    static uint8_t file[WHOLE];
    char id[REPLAY_CARS_ID];
    ReplayRace r;
    track_registry_clear();
    car_registry_clear();
    register_car("f40", "F40", 0x1234, 3);
    const size_t size = make_file(file, "tr1", 16, NULL, 1);
    CHECK_EQ_INT(size, LOOKS_END + REPLAY_PRINTS_BLOCK);
    CHECK_EQ_INT(race_replay_file_cars(file, size, &r, id), REPLAY_CARS_OK);
    CHECK_EQ_INT(r.field.entries[0].car, 16);
    car_registry_clear();
    register_car("f40", "F40", 0x1234, 4);   /* the same CRC-32, another print */
    CHECK_EQ_INT(race_replay_file_cars(file, size, &r, id), REPLAY_CARS_CHANGED);
    CHECK(strcmp(id, "f40") == 0 && !race_replay_file_track_changed(file, size));
    car_registry_clear();
    register_car("f40", "F40", 0x7777, 3);   /* another CRC-32, the same print */
    CHECK_EQ_INT(race_replay_file_cars(file, size, &r, id), REPLAY_CARS_OK);
}

static void test_a_trailer_that_is_not_ours(void)
{
    static uint8_t file[WHOLE];
    char id[REPLAY_CARS_ID];
    ReplayRace r;
    car_registry_clear();
    track_registry_clear();
    register_track("alps-run", "PEAKS", 0x5678, 7);
    const size_t size = make_file(file, "peaks1", 3, track_registry_piece(16), 1);
    CHECK_EQ_INT(race_replay_file_cars(file, size, &r, id), REPLAY_CARS_OK);
    file[TRACK_END + 7] = '2';   /* its magic */
    CHECK_EQ_INT(race_replay_file_cars(file, size, &r, id), REPLAY_CARS_BAD);
    CHECK(!race_replay_file_track_changed(file, size));
    file[TRACK_END + 7] = '1';
    CHECK_EQ_INT(race_replay_file_cars(file, size - 1, &r, id), REPLAY_CARS_BAD);   /* any other size */
    CHECK_EQ_INT(race_replay_file_cars(file, size + 1, &r, id), REPLAY_CARS_BAD);
    CHECK_EQ_INT(race_replay_file_cars(file, LOOKS_END + REPLAY_PRINTS_BLOCK - 1, &r, id), REPLAY_CARS_BAD);
}

int main(void)
{
    test_the_block();
    test_a_track_by_its_print();
    test_a_replay_before_the_prints_goes_by_the_crc();
    test_no_stored_print_falls_back_to_the_crc();
    test_a_car_by_its_print();
    test_a_trailer_that_is_not_ours();
    car_registry_clear();
    track_registry_clear();
    return TEST_REPORT();
}
