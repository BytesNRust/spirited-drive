/* The replay file's blocks (race_replay_image.h): the setup's race read with its checks, the race's setup written
 * where 004fb7bc keeps each word, and the cuts / seek block / records out and back in unchanged. With the CD's
 * replays as arguments, each one's setup reads as a race and its blocks come back byte for byte where the port has
 * a word for them. */
#include "game/race/race_replay_image.h"
#include "game/race/race_sim.h"
#include "test.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t setup[REPLAY_SETUP_SIZE], cuts[REPLAY_CUTS_SIZE], seek[REPLAY_SEEK_SIZE],
    records[REPLAY_RECORDS * REPLAY_RECORD_SIZE];
static uint8_t cuts2[REPLAY_CUTS_SIZE], seek2[REPLAY_SEEK_SIZE], records2[REPLAY_RECORDS * REPLAY_RECORD_SIZE];
static RaceState race;
static RaceReplayDirector dir;

static int32_t word(const uint8_t *b, int at) { return (int32_t)(b[at] | b[at + 1] << 8 | b[at + 2] << 16 | (uint32_t)b[at + 3] << 24); }

static void fill(uint8_t *b, size_t n, unsigned seed)
{
    for (size_t i = 0; i < n; i++) b[i] = (uint8_t)(seed = seed * 1103515245u + 12345u) >> 3;
}

static void synthetic(void)
{
    memset(&race, 0, sizeof race);
    strcpy(race.track_name, "TR6");
    race.race_type = 2, race.racers = 8, race.humans = 1, race.car_count = 8, race.laps_setting = 2, race.player = 0;
    race.track = 5, race.models[3] = 203, race.grid[0] = 7, race.catch_up = 4;
    race.entries[0].car = 4, strcpy(race.entries[0].name, "Mom?");
    race.mirrored = 1, race.arcade = 1, race.port_time_of_day = 2;
    race_replay_image_setup(&race, 0x31c3f86a, setup);
    CHECK(word(setup, 0x324 + 4 * 5) == 2 && word(setup, 0x324) == 1 && word(setup, 0x324 + 4 * 8) == 1);  /* 004fbae0 */
    CHECK(memcmp(setup + 0xa, "tr6\0", 4) == 0);
    CHECK(word(setup, 0x14) == 2 && word(setup, 0x20) == 8 && word(setup, 0x38) == 0x31c3f86a);
    CHECK(word(setup, 0x308) == 2 && word(setup, 0x2d8 + 12) == 203 && word(setup, 0x294) == 7 && word(setup, 0x304) == 4);
    CHECK(word(setup, 0x3e + 9) == 4 && word(setup, 6) == 5);
    ReplayRace r;
    CHECK(race_replay_image_race(setup, &r));
    CHECK(strcmp(r.stem, "TR6") == 0 && r.field.racers == 8 && r.field.race_type == 2 && r.laps == 2);
    CHECK(r.field.seed == 0x31c3f86a && r.field.entries[0].car == 4 && strcmp(r.field.entries[0].name, "Mom?") == 0);
    CHECK(r.field.mirrored == 1 && r.arcade == 1);                                     /* 0x31c, 0x320 */
    race.track_flags = 8, race.entries[3].car = 11;                                    /* TURBO, the Warrior as a rival */
    race_replay_image_setup(&race, 0x31c3f86a, setup);
    CHECK(race_replay_image_race(setup, &r) && (r.track_flags & 8) != 0 && r.field.entries[3].car == 11);
    race.entries[0].car = 11;                                                          /* and as the player's car */
    race_replay_image_setup(&race, 0x31c3f86a, setup);
    CHECK(race_replay_image_race(setup, &r) && r.field.entries[0].car == 11);
    race.track_flags = 0, race.entries[3].car = 0, race.entries[0].car = 4;
    race_replay_image_setup(&race, 0x31c3f86a, setup);
    CHECK(race_replay_image_race(setup, &r));
    CHECK(!r.explode && setup[0x3c] == 0 && setup[0x3d] == 0);                         /* no cheat: the exe's bytes */
    race.unknown_4c5cd4 = 1;                                                           /* EXPLODE: the replay keeps it */
    race_replay_image_setup(&race, 0x31c3f86a, setup);
    CHECK(race_replay_image_race(setup, &r) && r.explode);
    setup[0x3d] = 0;                                                                   /* an unmarked word is not one */
    CHECK(race_replay_image_race(setup, &r) && !r.explode);
    race.unknown_4c5cd4 = 0;
    race_replay_image_setup(&race, 0x31c3f86a, setup);

    uint8_t bad[REPLAY_SETUP_SIZE];
    memcpy(bad, setup, sizeof bad), bad[0xa] = 0;                                      /* no stem */
    CHECK(!race_replay_image_race(bad, &r));
    memcpy(bad, setup, sizeof bad), bad[0x20] = 9;                                     /* nine racers */
    CHECK(!race_replay_image_race(bad, &r));
    memcpy(bad, setup, sizeof bad), bad[0x3e + 9] = 12;                                /* no such car */
    CHECK(!race_replay_image_race(bad, &r));
    memcpy(bad, setup, sizeof bad), bad[0x310] = 8;                                    /* following no racer */
    CHECK(!race_replay_image_race(bad, &r));

    fill(cuts, sizeof cuts, 1), fill(seek, sizeof seek, 2), fill(records, sizeof records, 3);
    race_replay_image_get(&race, &dir, cuts, seek, records);
    CHECK(race.replay_cuts[1].clock == word(cuts, 12) && race.notes[0] == word(cuts, 0x200));
    CHECK(dir.end_clock == word(seek, 4) && dir.saved_due == word(seek, 0xcc4));
    CHECK(memcmp((uint8_t *)race.players + 8, records, 8) == 0);
    race_replay_image_put(&race, &dir, cuts2, seek2, records2);
    CHECK(memcmp(cuts, cuts2, sizeof cuts) == 0);                                      /* every word of the cuts mapped */
    CHECK(memcmp(seek + 0x10, seek2 + 0x10, sizeof seek - 0x10) == 0 && word(seek2, 4) == word(seek, 4));
    CHECK(memcmp(records, records2, sizeof records - 8) == 0 && memcmp(records + sizeof records - 2, records2 + sizeof records - 2, 2) == 0);
}

/* 004275f8: the player's controls (004fb724, 00426ddc's from config.dat) ride in its entry at +0x31 (0042702c's
 * local_39, which 004273d0 copies into the setup), so the replay's setup keeps them; after the setup is in place
 * (a replay's read by 0044d3b0) every human's controls come back out of the entries. A CD demo recorded on a pad
 * (1, 3, 0) is steered as a pad's, not as keys. */
static void controls(void)
{
    CHECK(offsetof(RaceEntry, controls) == 0x31 && sizeof(ControlSetup) == 12);
    memset(&race, 0, sizeof race);
    strcpy(race.track_name, "cl1");
    race.racers = 2, race.humans = 1, race.laps_setting = 8;
    race.entries[0].car = 9, race.entries[1].car = 4;
    race.entries[0].controls = (ControlSetup){ 1, 3, 0 };
    race_replay_image_setup(&race, 1, setup);
    CHECK(word(setup, 0x3e + 0x31) == 1 && word(setup, 0x3e + 0x35) == 3 && word(setup, 0x3e + 0x39) == 0);
    ReplayRace r;
    CHECK(race_replay_image_race(setup, &r));
    CHECK(r.field.entries[0].controls.analog_steering == 1 && r.field.entries[0].controls.pedals == 3);

    memset(&race, 0, sizeof race);
    race.humans = 1;
    race.controls[0] = (ControlSetup){ 0, 0, 0 };                 /* the keys, as this machine's config.dat has them */
    race.controls[1] = (ControlSetup){ 7, 7, 7 };
    memcpy(race.entries, r.field.entries, sizeof race.entries);
    race.entries[1].controls = (ControlSetup){ 0x7b0800, 0, 0x28000 };  /* the CD's second entry: no human there */
    race_controls_from_entries(&race);
    CHECK(race.controls[0].analog_steering == 1 && race.controls[0].pedals == 3 && race.controls[0].gears == 0);
    CHECK(race.controls[1].analog_steering == 7);                 /* only the humans' */

    race.humans = 2;
    race.entries[1].controls = (ControlSetup){ 0, 1, 3 };
    race_controls_from_entries(&race);
    CHECK(race.controls[1].analog_steering == 0 && race.controls[1].pedals == 1 && race.controls[1].gears == 3);
}

/* A CD replay: its race read, its blocks back as they came (the seek block's three unnamed words and the records'
 * six unnamed bytes aside) */
static void cd_replay(const char *path)
{
    static uint8_t file[REPLAY_FILE_SIZE + 1];
    FILE *f = fopen(path, "rb");
    const size_t size = f != NULL ? fread(file, 1, sizeof file, f) : 0;
    if (f != NULL) fclose(f);
    CHECK(size == REPLAY_FILE_SIZE);
    if (size != REPLAY_FILE_SIZE) return;
    ReplayRace r;
    CHECK(race_replay_image_race(file, &r));
    CHECK(r.stem[0] != 0 && r.field.racers >= 1 && r.laps >= 1);
    CHECK(r.field.entries[0].controls.analog_steering == 1 && r.field.entries[0].controls.pedals == 3);  /* a pad's */
    const uint8_t *c = file + REPLAY_SETUP_SIZE, *s = c + REPLAY_CUTS_SIZE + REPLAY_RECORDING_BYTES,
                  *rec = s + REPLAY_SEEK_SIZE;
    memset(&race, 0, sizeof race), memset(&dir, 0, sizeof dir);
    race_replay_image_get(&race, &dir, c, s, rec);
    race_replay_image_put(&race, &dir, cuts2, seek2, records2);
    CHECK(memcmp(c, cuts2, REPLAY_CUTS_SIZE) == 0);
    CHECK(memcmp(s + 0x10, seek2 + 0x10, REPLAY_SEEK_SIZE - 0x10) == 0 && word(s, 4) == word(seek2, 4));
    CHECK(memcmp(rec, records2, sizeof records2 - 8) == 0);
}

/* port (replay colours): the paints a race wore travel after the exe's blocks; a file of the exe's exact size (the CD's, an older port's) has none and the pick is the race rules' */
static void paints(void)
{
    static uint8_t file[REPLAY_FILE_SIZE + REPLAY_PAINT_TRAILER_SIZE];
    uint8_t wore[RACE_PLAYERS] = { 4, 5, 2, 3, 0, 3, 2, 1 }, got[RACE_PLAYERS];
    race_replay_paint_put(wore, file + REPLAY_FILE_SIZE);
    CHECK(race_replay_paint_get(file, sizeof file, got) == 1 && memcmp(got, wore, sizeof got) == 0);
    CHECK(race_replay_paint_get(file, REPLAY_FILE_SIZE, got) == 0 && got[0] == REPLAY_PAINT_NONE && got[7] == REPLAY_PAINT_NONE);   /* the exe's size: none */
    file[REPLAY_FILE_SIZE] ^= 1;
    CHECK(race_replay_paint_get(file, sizeof file, got) == 0);   /* not our marker */
    CHECK(race_replay_paint_get(NULL, 0, got) == 0);
}

/* port (snow_weather): the replay says whether its race ran on snow, after the paints; a file without that (the CD's, an older port's) means no snow, whatever this machine has on */
static void snow_trailer(void)
{
    static uint8_t file[REPLAY_FILE_SIZE + REPLAY_PAINT_TRAILER_SIZE + REPLAY_SNOW_TRAILER_SIZE];
    uint8_t wore[RACE_PLAYERS] = { 0 };
    race_replay_paint_put(wore, file + REPLAY_FILE_SIZE);
    int snow = 7;
    CHECK(race_replay_snow_get(file, REPLAY_FILE_SIZE + REPLAY_PAINT_TRAILER_SIZE, &snow) == 0 && snow == 0);   /* paints only: no snow */
    race_replay_snow_put(1, file + REPLAY_FILE_SIZE + REPLAY_PAINT_TRAILER_SIZE);
    CHECK(race_replay_snow_get(file, sizeof file, &snow) == 1 && snow == 1);
    race_replay_snow_put(0, file + REPLAY_FILE_SIZE + REPLAY_PAINT_TRAILER_SIZE);
    CHECK(race_replay_snow_get(file, sizeof file, &snow) == 1 && snow == 0);   /* recorded as not snow */
    race_replay_snow_put(1, file + REPLAY_FILE_SIZE + REPLAY_PAINT_TRAILER_SIZE);
    file[REPLAY_FILE_SIZE + REPLAY_PAINT_TRAILER_SIZE] ^= 1;
    CHECK(race_replay_snow_get(file, sizeof file, &snow) == 0 && snow == 0);   /* not our marker */
    CHECK(race_replay_snow_get(NULL, 0, &snow) == 0);
    CHECK(race_replay_snow_get(file, REPLAY_FILE_SIZE, &snow) == 0);          /* the exe's size */
}

/* port (replay conditions): the time of day, weather and direction a race ran with travel after the snow record; a file without them (the CD's demos, an older port's) is the original's: day, clear, forward */
static void conditions_trailer(void)
{
    enum { AT = REPLAY_FILE_SIZE + REPLAY_PAINT_TRAILER_SIZE + REPLAY_SNOW_TRAILER_SIZE };
    static uint8_t file[AT + REPLAY_CONDITIONS_TRAILER_SIZE];
    ReplayConditions c = { 9, 9, 9 };
    CHECK(race_replay_conditions_get(file, AT, &c) == 0 && c.light_time == 0 && c.weather == 0 && c.reverse == 0);   /* the snow's size: day, clear, forward */
    const ReplayConditions night = { 3, 2, 1 };
    race_replay_conditions_put(&night, file + AT);
    c = (ReplayConditions){ 0, 0, 0 };
    CHECK(race_replay_conditions_get(file, sizeof file, &c) == 1 && c.light_time == 3 && c.weather == 2 && c.reverse == 1);
    file[AT + 8] = 7;
    CHECK(race_replay_conditions_get(file, sizeof file, &c) == 0 && c.light_time == 0);   /* not a time of ours */
    race_replay_conditions_put(&night, file + AT);
    file[AT] ^= 1;
    CHECK(race_replay_conditions_get(file, sizeof file, &c) == 0 && c.weather == 0);   /* not our marker */
    CHECK(race_replay_conditions_get(NULL, 0, &c) == 0);
}

int main(int argc, char **argv)
{
    conditions_trailer();
    snow_trailer();
    paints();
    synthetic();
    controls();
    for (int i = 1; i < argc; i++) cd_replay(argv[i]);
    return TEST_REPORT();
}
