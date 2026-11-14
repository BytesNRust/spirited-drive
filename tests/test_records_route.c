/* Where a finished race's records go (records_route.c, records_glue.c). With the original's records (Stock, Restored: records_unified 0)
 * config.dat's image takes every race, mixed, byte for byte as record_check.c's routines alone would, and no file is written. With
 * Restomod's (race_records.h) each race takes the table of its track, layout (all eight), cheats and laps, every row with its marks (the
 * cheats, the Warrior, the moon's gravity, the weather, the time of day, the difficulty); the best lap is shared by every lap count; a
 * stage keeps its segments; a table that changed is saved at once. */
#include "tests/exe_tables_install.h"
#include "game/frontend/config_dat.h"
#include "game/frontend/records_glue.h"
#include "game/frontend/records_route.h"
#include "game/race/difficulty.h"
#include "game/race/weather_snow.h"
#include "game/race/weather.h"
#include "game/sys/extras_marks.h"
#include "game/sys/free_laps.h"
#include "game/sys/port_settings.h"
#include "game/sys/race_records.h"
#include "game/sys/reverse_track.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

enum { FILES = 6 };
static struct {
    char name[32];
    uint8_t *data;
    size_t size;
} files[FILES];
static int writes;

static int slot_of(const char *name, int make)
{
    for (int i = 0; i < FILES; i++)
        if (files[i].data != NULL && strcmp(files[i].name, name) == 0) return i;
    if (!make) return -1;
    for (int i = 0; i < FILES; i++)
        if (files[i].data == NULL) return snprintf(files[i].name, sizeof files[i].name, "%s", name), i;
    return -1;
}

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    const int i = slot_of(name, 0);
    if (i < 0) return NULL;
    uint8_t *copy = malloc(files[i].size ? files[i].size : 1);
    memcpy(copy, files[i].data, files[i].size);
    *size = files[i].size;
    return copy;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    const int i = slot_of(name, 1);
    if (i < 0) return false;
    free(files[i].data);
    files[i].data = malloc(size ? size : 1);
    memcpy(files[i].data, data, size);
    files[i].size = size;
    writes++;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }
bool platform_user_file_exists(const char *name)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(name, &size);
    if (data != NULL) platform_free_file(data);
    return data != NULL;
}
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path, *size = 0; return NULL; }
bool platform_key_down(int scancode) { (void)scancode; return false; }

static void files_clear(void)
{
    for (int i = 0; i < FILES; i++) free(files[i].data), files[i].data = NULL;
    writes = 0;
}

enum { TR1 = 3, COASTAL = 1, WARRIOR = 0xb, ME = 0x95, LAPS = 9, TIME = 0x59, SPEED = 0x61, TOP = 0x6d, TOTAL = 0x65 };

typedef struct {
    uint8_t image[RECORD_CHECK_IMAGE];
    uint8_t game[RECORD_CHECK_GAME];
} Race;

static void p16(uint8_t *at, int v) { at[0] = (uint8_t)v, at[1] = (uint8_t)(v >> 8); }
static void p32(uint8_t *at, int32_t v) { memcpy(at, &v, 4); }
static int32_t g32(const uint8_t *at)
{
    int32_t v;
    memcpy(&v, at, 4);
    return v;
}

/* a fresh image (the defaults) and a race on `track` by car `car`, laps choice 1, with a lap of `lap` ticks, a top speed, a total, a
 * segment's time and speed */
static void race(Race *r, int track, int car, int lap, int top, int total)
{
    ConfigDat c;
    config_dat_defaults(&c, 0);
    memcpy(r->image, c.image, sizeof r->image);
    memset(r->game, 0, sizeof r->game);
    p16(r->image + 0x95, track);
    p16(r->image + 0x9b, car);
    p16(r->image + 0xb3, 1);
    p16(r->image + 0x93, 1);
    memcpy(r->game, "ME", 3);
    for (int i = 0; i < 20; i++) p32(r->game + ME * 0 + LAPS + 4 * i, 0x7fffffff);
    p32(r->game + LAPS, lap);
    p32(r->game + TOP, top);
    p32(r->game + TOTAL, total);
    p32(r->game + TIME, total / 3);
    p32(r->game + SPEED, top);
}

static void unified(int on)
{
    setenv("TNFS_RECORDS_UNIFIED", on ? "1" : "0", 1);
    files_clear();
    race_records_reset();
    free_laps_set(0);
    reverse_track_set(0);
    race_records_lunar_set(0);
    race_records_conditions_set(race_records_conditions_of(WEATHER_CLEAR, 0, 0, LIGHT_TIME_DAY, 0, 0, 0));
}

static const int32_t TIMES[RECORD_CHECK_TIMES] = { 0, 11, 12, 13, 21, 22, 23 };

/* The original's records, in Stock and Restored alike: every kind of race, every op, the image ends as record_check.c's routines alone
 * leave it, and no file is written */
static void test_the_originals_records_are_the_exes_byte_for_byte(void)
{
    const RaceKind kinds[] = { { LAYOUT_NORMAL, 0 }, { LAYOUT_MIRROR, 0 }, { LAYOUT_RALLY, EXTRAS_MARK_NITRO }, { LAYOUT_MIRROR | LAYOUT_RALLY, EXTRAS_MARK_TURBO } };
    const int cars[] = { 0, WARRIOR };
    for (size_t k = 0; k < sizeof kinds / sizeof kinds[0]; k++)
        for (int c = 0; c < 2; c++)
            for (int track = 0; track < 9; track += 4) {   /* a stage and two circuits */
                unified(0);
                static Race r, direct;
                race(&r, track, cars[c], 1, 99999, 2);
                direct = r;
                const int32_t a[4] = { records_route(kinds[k], ROUTE_SEGMENT, r.image, r.game, 0, NULL),
                                       records_route(kinds[k], ROUTE_LAP, r.image, r.game, 0, NULL),
                                       records_route(kinds[k], ROUTE_TOP, r.image, r.game, 0, NULL),
                                       records_route(kinds[k], ROUTE_TOTALS, r.image, r.game, 0, TIMES) };
                const int32_t b[4] = { record_check(direct.image, direct.game, 0), record_check_lap(direct.image, direct.game, 0),
                                       record_check_top(direct.image, direct.game, 0), record_check_totals(direct.image, direct.game, 0, TIMES) };
                CHECK(memcmp(a, b, sizeof a) == 0);
                CHECK(memcmp(r.image, direct.image, sizeof r.image) == 0);
                CHECK_EQ_INT(writes, 0);
            }
}

/* The original's: a free race on a count its track has goes to that choice's table; another count keeps no total, no segment */
static void test_the_originals_free_race(void)
{
    unified(0);
    static Race r, before;
    race(&r, TR1, 0, 1, 99999, 2);   /* TR1's counts: 4, 8, 16 */
    p16(r.image + 0xb3, 0);
    free_laps_set(16);
    before = r;
    CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_TOTALS, r.image, r.game, 0, TIMES), 0);
    CHECK_EQ_INT(g32(r.image + 0xb5 + 0xa6b * TR1 + 0xea + 0x186 * 2 + 0x1b), 2);   /* choice 2's table, not the menu's 0 */
    CHECK_EQ_INT(g32(r.image + 0xb5 + 0xa6b * TR1 + 0xea + 0x1b), g32(before.image + 0xb5 + 0xa6b * TR1 + 0xea + 0x1b));
    free_laps_set(7);
    before = r;
    CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_TOTALS, r.image, r.game, 0, TIMES), RECORD_CHECK_NONE);
    CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_SEGMENT, r.image, r.game, 0, NULL), 0);
    CHECK(memcmp(r.image, before.image, sizeof r.image) == 0);
    CHECK_EQ_INT(writes, 0);
}

/* the race now running as the original could not run it: one of a reversed track, Easy, snow (k 0..2); -1 all back */
static void unusual(int k)
{
    const RaceDifficulty original = difficulty_from_extras(1), easy = difficulty_from_extras(0);   /* extras levels: 0 Easy, 1 Original */
    reverse_track_set(k == 0);
    difficulty_running_set(k == 1 ? &easy : &original);
    snow_grip_set(k == 2);
}

/* The original's: a race the original could not run (reversed, not Original difficulty, snow) keeps nothing in its mixed tables;
 * Restomod's keeps it in the race's own table */
static void test_the_originals_keep_no_race_it_could_not_run(void)
{
    for (int k = 0; k < 3; k++) {
        unified(0);
        unusual(k);
        static Race r, before;
        race(&r, TR1, 0, 1, 99999, 2);
        before = r;
        CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_TOTALS, r.image, r.game, 0, TIMES), RECORD_CHECK_NONE);
        CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_LAP, r.image, r.game, 0, NULL), 0);
        CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_TOP, r.image, r.game, 0, NULL), 0);
        CHECK(memcmp(r.image, before.image, sizeof r.image) == 0);
        CHECK_EQ_INT(writes, 0);
        unified(1);
        unusual(k);
        race(&r, TR1, 0, 1, 99999, 2);
        CHECK_EQ_INT(records_route((RaceKind){ k == 0 ? LAYOUT_REVERSE : 0, 0 }, ROUTE_TOTALS, r.image, r.game, 0, TIMES), 0);
        CHECK(race_records_find(TR1, k == 0 ? LAYOUT_REVERSE : 0, 0, 8) != NULL);
    }
    unusual(-1);
}

/* Restomod's: all eight layouts, with and without cheats, are sixteen tables; config.dat's image is not touched */
static void test_every_layout_and_cheats_is_its_own_table(void)
{
    unified(1);
    static Race r, before;
    int total = 1000;
    for (int layout = 0; layout < RACE_RECORDS_LAYOUTS; layout++)
        for (int cheats = 0; cheats < 2; cheats++) {
            race(&r, TR1, 0, 1, 99999, ++total);
            before = r;
            const RaceKind kind = { layout, cheats ? EXTRAS_MARK_EXPLODE : 0 };
            CHECK_EQ_INT(records_route(kind, ROUTE_TOTALS, r.image, r.game, 0, TIMES), 0);
            CHECK(memcmp(r.image, before.image, sizeof r.image) == 0);
        }
    total = 1000;
    for (int layout = 0; layout < RACE_RECORDS_LAYOUTS; layout++)
        for (int cheats = 0; cheats < 2; cheats++) {
            const RecordTable *t = race_records_find(TR1, layout, cheats, 8);   /* choice 1 on TR1: 8 laps */
            CHECK(t != NULL);
            if (t == NULL) continue;
            CHECK_EQ_INT(g32(t->totals[0].row + RECORD_ROW_TIME), ++total);
            CHECK(!race_records_row_set(&t->totals[1]));   /* one race each: one row */
            CHECK_EQ_INT(t->totals[0].marks.cheats, cheats ? EXTRAS_MARK_EXPLODE : 0);
        }
}

/* Every row keeps the marks of the race that set it, and they follow the rows down */
static void test_rows_keep_their_marks(void)
{
    unified(1);
    static Race r;
    const RecordMarks wet = race_records_conditions_of(WEATHER_RAIN, 0, 1, LIGHT_TIME_DUSK, 2, 2, 2);
    race_records_conditions_set(wet);
    race(&r, TR1, 0, 1, 99999, 500);
    CHECK_EQ_INT(records_route((RaceKind){ LAYOUT_MIRROR, EXTRAS_MARK_NITRO }, ROUTE_TOTALS, r.image, r.game, 0, TIMES), 0);
    race_records_conditions_set(race_records_conditions_of(WEATHER_CLEAR, 1, 0, LIGHT_TIME_NIGHT, 0, 1, 0));
    race(&r, TR1, 0, 1, 99999, 400);
    CHECK_EQ_INT(records_route((RaceKind){ LAYOUT_MIRROR, EXTRAS_MARK_TURBO }, ROUTE_TOTALS, r.image, r.game, 0, TIMES), 0);
    const RecordTable *t = race_records_find(TR1, LAYOUT_MIRROR, 1, 8);
    CHECK(t != NULL);
    if (t == NULL) return;
    CHECK_EQ_INT(t->totals[0].marks.cheats, EXTRAS_MARK_TURBO);
    CHECK_EQ_INT(t->totals[0].marks.weather, RECORD_MARK_KNOWN | WEATHER_CLEAR | RECORD_WEATHER_SNOW);
    CHECK_EQ_INT(t->totals[0].marks.time, RECORD_MARK_KNOWN | LIGHT_TIME_NIGHT);
    CHECK_EQ_INT(t->totals[0].marks.difficulty, RECORD_MARK_KNOWN | 1 << 2);
    CHECK_EQ_INT(t->totals[1].marks.cheats, EXTRAS_MARK_NITRO);   /* pushed down with its row */
    CHECK_EQ_INT(t->totals[1].marks.weather, wet.weather);
    CHECK_EQ_INT(t->totals[1].marks.time, wet.time);
    CHECK_EQ_INT(t->totals[1].marks.difficulty, wet.difficulty);
    CHECK_EQ_INT(g32(t->totals[1].row + RECORD_ROW_TIME), 500);
}

/* The Warrior's times are kept, on the cheats table with W; the moon's gravity is a cheat too (L), through the glue */
static void test_the_warrior_and_the_moon_are_cheats(void)
{
    unified(1);
    static Race r;
    race(&r, TR1, WARRIOR, 1, 99999, 300);
    CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_TOTALS, r.image, r.game, 0, TIMES), 0);
    const RecordTable *t = race_records_find(TR1, 0, 1, 8);
    CHECK(t != NULL && t->totals[0].marks.cheats == EXTRAS_MARK_WARRIOR);
    CHECK(race_records_find(TR1, 0, 0, 8) == NULL);
    records_glue_begin(r.image, 1, (const uint8_t[18]){ 0 }, 0, 0, 0, 0, 0);
    race_records_lunar_set(1);
    race(&r, TR1, 0, 1, 99999, 200);
    CHECK_EQ_INT(records_glue_route(ROUTE_TOTALS, r.image, r.game, 0, TIMES), 0);
    t = race_records_find(TR1, 0, 1, 8);
    CHECK(t != NULL && t->totals[0].marks.cheats == EXTRAS_MARK_LUNAR);
    char letters[6];
    records_mark_text(EXTRAS_MARK_NITRO | EXTRAS_MARK_WARRIOR | EXTRAS_MARK_LUNAR, letters);
    CHECK(strcmp(letters, "NWL") == 0);
}

/* A tournament's totals take the cheats it began with, the moon's gravity of its first race among them */
static void test_a_tournament_keeps_its_cheats(void)
{
    unified(1);
    static Race r;
    race(&r, TR1, 0, 1, 99999, 200);
    const uint8_t first[18] = { 0 }, later[18] = { 1 };
    records_glue_begin(r.image, RECORDS_TOURNAMENT, first, 0, 0, 0, 1, 0);   /* nitro on */
    race_records_lunar_set(1);
    CHECK_EQ_INT(records_glue_kind(ROUTE_TOTALS).marks, EXTRAS_MARK_NITRO | EXTRAS_MARK_LUNAR);
    records_glue_begin(r.image, RECORDS_TOURNAMENT, later, 0, 0, 0, 0, 0);   /* the cheats cleared, no moon: the tournament's still */
    race_records_lunar_set(0);
    CHECK_EQ_INT(records_glue_kind(ROUTE_TOTALS).marks, EXTRAS_MARK_NITRO | EXTRAS_MARK_LUNAR);
    CHECK_EQ_INT(records_glue_kind(ROUTE_LAP).marks, 0);   /* its own race's lap: its own cheats */
}

/* The layout as raced: the reversed track (known once it ran) is a bit of its own, both variants together too */
static void test_the_layout_comes_from_the_flags_and_the_race(void)
{
    unified(1);
    static Race r;
    race(&r, TR1, 0, 1, 99999, 200);
    records_glue_begin(r.image, 1, (const uint8_t[18]){ 0 }, 1, 1, 0, 0, 0);
    reverse_track_set(1);
    CHECK_EQ_INT(records_glue_kind(ROUTE_TOTALS).layout, LAYOUT_MIRROR | LAYOUT_REVERSE | LAYOUT_RALLY);
    reverse_track_set(0);
    CHECK_EQ_INT(records_race_kind(0, 1, 0, 1, 0, 1).layout, LAYOUT_RALLY);
    CHECK_EQ_INT(records_race_kind(0, 1, 0, 1, 0, 1).marks, EXTRAS_MARK_EXPLODE | EXTRAS_MARK_TURBO);
}

/* The best lap is the track's, layout's and cheats', whatever the lap count; the tables are the lap count's */
static void test_the_best_lap_is_shared_by_every_lap_count(void)
{
    unified(1);
    static Race r;
    race(&r, TR1, 0, 700, 99999, 9000);
    free_laps_set(7);
    CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_LAP, r.image, r.game, 0, NULL), 1);
    CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_TOTALS, r.image, r.game, 0, TIMES), 0);
    race(&r, TR1, 0, 800, 99999, 9500);
    free_laps_set(9);
    CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_LAP, r.image, r.game, 0, NULL), 0);   /* slower: the 7-lap race's stays */
    CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_TOTALS, r.image, r.game, 0, TIMES), 0);   /* its own table's first */
    race(&r, TR1, 0, 650, 99999, 9600);
    CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_LAP, r.image, r.game, 0, NULL), 1);
    const RecordRow *lap = race_records_lap_find(TR1, 0, 0);
    CHECK(lap != NULL && g32(lap->row + RECORD_ROW_TIME) == 650);
    CHECK(race_records_find(TR1, 0, 0, 7) != NULL && race_records_find(TR1, 0, 0, 9) != NULL);
    int laps[4];
    CHECK_EQ_INT(race_records_laps(TR1, 0, 0, laps, 4), 2);
    CHECK_EQ_INT(records_race_laps(r.image), 9);
    free_laps_set(0);
    CHECK_EQ_INT(records_race_laps(r.image), 8);   /* the menu's choice 1 */
}

/* A stage keeps its segments' times and speeds (no laps); the circuit's segment check and the stage's lap check take nothing */
static void test_a_stage_keeps_its_segments(void)
{
    unified(1);
    static Race r;
    race(&r, COASTAL, 0, 1, 99999, 3000);
    p16(r.image + 0x93, 2);   /* the second segment */
    CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_SEGMENT, r.image, r.game, 0, NULL), 3);
    const RecordTable *t = race_records_find(COASTAL, 0, 0, 0);
    CHECK(t != NULL);
    if (t != NULL) {
        CHECK_EQ_INT(g32(t->segment_time[1].row + RECORD_ROW_TIME), 1000);
        CHECK_EQ_INT(g32(t->segment_speed[1].row + RECORD_ROW_SPEED), 99999);
        CHECK(!race_records_row_set(&t->segment_time[0]));
    }
    CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_TOTALS, r.image, r.game, 0, TIMES), 0);
    if (t != NULL) {
        CHECK_EQ_INT(t->stage_speeds[0], 11);
        CHECK_EQ_INT(t->stage_totals[2], 23);
    }
    CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_LAP, r.image, r.game, 0, NULL), 0);
    race(&r, TR1, 0, 1, 99999, 3000);
    CHECK_EQ_INT(records_route((RaceKind){ 0, 0 }, ROUTE_SEGMENT, r.image, r.game, 0, NULL), 0);
}

/* A row taken is saved at once: the file holds it */
static void test_a_record_is_saved_at_once(void)
{
    unified(1);
    static Race r;
    race(&r, TR1, 0, 1, 99999, 1234);
    CHECK_EQ_INT(records_route((RaceKind){ LAYOUT_REVERSE, 0 }, ROUTE_TOTALS, r.image, r.game, 0, TIMES), 0);
    CHECK(slot_of("race_records.dat", 0) >= 0);
    race_records_reset();
    CHECK_EQ_INT(race_records_load(), RACE_RECORDS_LOADED);
    const RecordTable *t = race_records_find(TR1, LAYOUT_REVERSE, 0, 8);
    CHECK(t != NULL && g32(t->totals[0].row + RECORD_ROW_TIME) == 1234);
}

/* The results show the race's table: its rows in the first choice's place (the choice word 0), its cheats in the slots, its words */
static void test_the_results_show_the_races_table(void)
{
    unified(1);
    static Race r;
    static uint8_t shown[RECORD_CHECK_IMAGE];
    uint8_t slots[RECORDS_MARK_SLOTS];
    race(&r, TR1, 0, 1, 99999, 777);
    CHECK_EQ_INT(records_route((RaceKind){ LAYOUT_RALLY, EXTRAS_MARK_NITRO }, ROUTE_TOTALS, r.image, r.game, 0, TIMES), 0);
    records_route_shown((RaceKind){ LAYOUT_RALLY, EXTRAS_MARK_NITRO }, r.image, shown, slots);
    CHECK_EQ_INT(g32(shown + 0xb5 + 0xa6b * TR1 + 0xea + 0x1b), 777);
    CHECK_EQ_INT(shown[0xb3], 0);
    CHECK_EQ_INT(slots[records_slot_totals(TR1, 0, 0)], EXTRAS_MARK_NITRO);
    CHECK_EQ_INT(records_choice(r.image), 0);
    char line[64];
    records_table_line(LAYOUT_MIRROR | LAYOUT_RALLY, 0, 6, line, sizeof line);
    CHECK(strcmp(line, "MIRROR + RALLY - NO CHEATS - 6 LAPS") == 0);
    records_table_line(LAYOUT_NORMAL, 1, 0, line, sizeof line);
    CHECK(strcmp(line, "NORMAL - CHEATS") == 0);
    records_table_line(LAYOUT_REVERSE, 0, 1, line, sizeof line);
    CHECK(strcmp(line, "REVERSE - NO CHEATS - 1 LAP") == 0);
    /* the original's: the image as it is, no marks, no words */
    unified(0);
    records_route_shown((RaceKind){ LAYOUT_RALLY, EXTRAS_MARK_NITRO }, r.image, shown, slots);
    CHECK(memcmp(shown, r.image, sizeof shown) == 0);
    CHECK(records_glue_line(ROUTE_TOTALS, r.image) == NULL);
    CHECK_EQ_INT(records_glue_cheats(ROUTE_TOTALS, r.image), 0);
}

/* The conditions are the race's: the original's own time of day maps to the nearest */
static void test_the_conditions(void)
{
    CHECK_EQ_INT(race_records_original_time(0), LIGHT_TIME_DAWN);
    CHECK_EQ_INT(race_records_original_time(1), LIGHT_TIME_DAY);
    CHECK_EQ_INT(race_records_original_time(2), LIGHT_TIME_DUSK);
    const RecordMarks m = race_records_conditions_of(WEATHER_STORM, 0, 1, LIGHT_TIME_DAWN, 1, 1, 1);
    CHECK_EQ_INT(m.weather, RECORD_MARK_KNOWN | WEATHER_STORM | RECORD_WEATHER_WET);
    CHECK_EQ_INT(m.time, RECORD_MARK_KNOWN | LIGHT_TIME_DAWN);
    CHECK_EQ_INT(m.difficulty, RECORD_MARK_KNOWN | 1 | 1 << 2 | 1 << 4);
    CHECK_EQ_INT(m.cheats, 0);
}

int main(void)
{
    exe_tables_for_test();
    test_the_originals_records_are_the_exes_byte_for_byte();
    test_the_originals_free_race();
    test_the_originals_keep_no_race_it_could_not_run();
    test_every_layout_and_cheats_is_its_own_table();
    test_rows_keep_their_marks();
    test_the_warrior_and_the_moon_are_cheats();
    test_a_tournament_keeps_its_cheats();
    test_the_layout_comes_from_the_flags_and_the_race();
    test_the_best_lap_is_shared_by_every_lap_count();
    test_a_stage_keeps_its_segments();
    test_a_record_is_saved_at_once();
    test_the_results_show_the_races_table();
    test_the_conditions();
    files_clear();
    free_laps_set(0);
    return TEST_REPORT();
}
