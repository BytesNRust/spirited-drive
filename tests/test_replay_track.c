/* dep59 step 5 (T5): a replay's added track by id and fingerprint (race_replay_image.h's TNFSTRK1 block, after TNFSCAR1 and TNFSLOK1):
 * written from the piece, read back into today's number of the track; a missing track says "needs", a changed one "changed", as the cars
 * do; a block that is not exactly one of ours, or a setup whose stem is not the piece's, is no replay. */
#include "game/race/car_registry.h"
#include "game/race/race_replay_image.h"
#include "game/race/race_state.h"
#include "game/sys/track_registry.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static TrackPiece piece_of(const char *id, const char *stem, uint32_t print)
{
    TrackPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id);
    snprintf(p.name, sizeof p.name, "%s", id);
    snprintf(p.stem, sizeof p.stem, "%s", stem);
    p.spec.kind = TRACK_KIND_STAGE, p.spec.segments = 1, p.spec.times = TRACK_TIME_DAY;
    p.fingerprint = print;
    return p;
}

static void register_track(const char *id, const char *stem, uint32_t print, int want)
{
    const TrackPiece p = piece_of(id, stem, print);
    char why[160];
    CHECK_EQ_INT(track_registry_add(&p, why, sizeof why), want);
}

static void test_there_and_back(void)
{
    uint8_t block[REPLAY_TRACK_BLOCK];
    char id[REPLAY_TRACK_ID];
    int track = -1;
    track_registry_clear();
    register_track("alps-run", "PEAKS", 0x5678, 16);
    race_replay_track_write(block, track_registry_piece(16));
    CHECK(memcmp(block, "TNFSTRK1", 8) == 0);
    CHECK_EQ_INT(race_replay_track_read(block, &track, id), REPLAY_CARS_OK);
    CHECK_EQ_INT(track, 16);

    track_registry_clear();   /* another run: the same track by another number */
    register_track("turin", "TURINI", 7, 16);
    register_track("alps-run", "PEAKS", 0x5678, 17);
    CHECK_EQ_INT(race_replay_track_read(block, &track, id), REPLAY_CARS_OK);
    CHECK_EQ_INT(track, 17);

    track_registry_remove("alps-run");   /* gone: needs it */
    CHECK_EQ_INT(race_replay_track_read(block, &track, id), REPLAY_CARS_MISSING);
    CHECK(strcmp(id, "alps-run") == 0);
    register_track("alps-run", "PEAKS", 0x9999, 17);   /* back, but another design: changed */
    CHECK_EQ_INT(race_replay_track_read(block, &track, id), REPLAY_CARS_CHANGED);
    CHECK(strcmp(id, "alps-run") == 0);
}

static void test_bad_blocks(void)
{
    uint8_t good[REPLAY_TRACK_BLOCK], block[REPLAY_TRACK_BLOCK];
    char id[REPLAY_TRACK_ID];
    int track = -1;
    track_registry_clear();
    register_track("alps-run", "PEAKS", 0x5678, 16);
    race_replay_track_write(good, track_registry_piece(16));
    memcpy(block, good, sizeof block), block[7] = '2';                            /* the magic */
    CHECK_EQ_INT(race_replay_track_read(block, &track, id), REPLAY_CARS_BAD);
    memcpy(block, good, sizeof block), memset(block + 8, 'a', REPLAY_TRACK_ID);  /* no NUL in its 33 bytes */
    CHECK_EQ_INT(race_replay_track_read(block, &track, id), REPLAY_CARS_BAD);
    memcpy(block, good, sizeof block), block[8] = 'A';                            /* "Alps-run": not a mod's id */
    CHECK_EQ_INT(race_replay_track_read(block, &track, id), REPLAY_CARS_BAD);
    memcpy(block, good, sizeof block), memset(block + 8, 0, REPLAY_TRACK_ID);    /* no track at all */
    CHECK_EQ_INT(race_replay_track_read(block, &track, id), REPLAY_CARS_BAD);
    CHECK_EQ_INT(race_replay_track_read(good, &track, id), REPLAY_CARS_OK);
}

enum { CARS_END = REPLAY_CARS_AT + REPLAY_CARS_BLOCK, LOOKS_END = CARS_END + REPLAY_LOOKS_BLOCK, TRACK_END = LOOKS_END + REPLAY_TRACK_BLOCK };

/* a whole Replay.rpx of a race on `stem` with an original car and, past the looks, the track's block of `piece` */
static void make_file(uint8_t *file, const char *stem, const TrackPiece *piece)
{
    static RaceState race;
    memset(&race, 0, sizeof race);
    snprintf(race.track_name, sizeof race.track_name, "%s", stem);
    race.race_type = 0, race.racers = 2, race.humans = 1, race.car_count = 2, race.laps_setting = 1, race.player = 0;
    race.entries[0].car = 3, race.entries[1].car = 5;
    race.track = 16;
    memset(file, 0, TRACK_END);
    race_replay_image_setup(&race, 1, file);
    RaceField f;
    memset(&f, 0, sizeof f);
    f.racers = 2, f.entries[0].car = 3, f.entries[1].car = 5;
    race_replay_cars_write(file + REPLAY_CARS_AT, &f);   /* no added car: all its entries zeros */
    ReplayLook looks[RACE_PLAYERS];
    memset(looks, 0, sizeof looks);
    race_replay_looks_put(looks, file + CARS_END);
    race_replay_track_write(file + LOOKS_END, piece);
}

static void test_the_whole_file(void)
{
    static uint8_t file[TRACK_END];
    char id[REPLAY_CARS_ID];
    ReplayRace r;
    car_registry_clear();
    track_registry_clear();
    register_track("alps-run", "PEAKS", 0x5678, 16);
    make_file(file, "peaks1", track_registry_piece(16));
    CHECK_EQ_INT(race_replay_file_cars(file, sizeof file, &r, id), REPLAY_CARS_OK);
    CHECK_EQ_INT(r.added_track, 16);
    CHECK(strcmp(r.stem, "PEAKS1") == 0);
    CHECK(r.looks_known && r.field.entries[0].car == 3 && r.field.entries[1].car == 5);
    CHECK(!race_replay_file_track_changed(file, sizeof file));   /* the track reads: whatever else changed, the message is not the track's */

    CHECK_EQ_INT(race_replay_file_cars(file, LOOKS_END, &r, id), REPLAY_CARS_OK);   /* step 3's: no track's block */
    CHECK_EQ_INT(r.added_track, 0);
    CHECK_EQ_INT(race_replay_file_cars(file, sizeof file - 1, &r, id), REPLAY_CARS_BAD);   /* any other size: no Replay.rpx */
    CHECK_EQ_INT(race_replay_file_cars(file, sizeof file + 1, &r, id), REPLAY_CARS_BAD);

    track_registry_clear();   /* the track gone: the list says it needs it, by its id */
    CHECK_EQ_INT(race_replay_file_cars(file, sizeof file, &r, id), REPLAY_CARS_MISSING);
    CHECK(strcmp(id, "alps-run") == 0);
    register_track("alps-run", "PEAKS", 0x1111, 16);
    CHECK_EQ_INT(race_replay_file_cars(file, sizeof file, &r, id), REPLAY_CARS_CHANGED);
    CHECK(strcmp(id, "alps-run") == 0);
    CHECK(race_replay_file_track_changed(file, sizeof file));   /* the message says a track, not cars */
    CHECK(!race_replay_file_track_changed(file, LOOKS_END));
    CHECK(!race_replay_file_track_changed(NULL, 0));

    track_registry_clear();
    register_track("alps-run", "PEAKS", 0x5678, 16);
    make_file(file, "tr1", track_registry_piece(16));   /* a stem that is not the piece's: no replay of it */
    CHECK_EQ_INT(race_replay_file_cars(file, sizeof file, &r, id), REPLAY_CARS_BAD);
    CHECK_EQ_INT(r.added_track, 0);
    make_file(file, "peaksx", track_registry_piece(16));   /* the piece's stem, then no stage */
    CHECK_EQ_INT(race_replay_file_cars(file, sizeof file, &r, id), REPLAY_CARS_BAD);
}

int main(void)
{
    test_there_and_back();
    test_bad_blocks();
    test_the_whole_file();
    track_registry_clear();
    car_registry_clear();
    return TEST_REPORT();
}
