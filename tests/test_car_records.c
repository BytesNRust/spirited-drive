/* owner 2026-11-18: a car's records on a track as its RECORDS screen shows them (car_records.h): one key chosen from the car's tables, the
 * standard one when it has a record there, else the best; the figures of that key, from the per-car store (stock tracks) and the tracks'
 * store (an added track, by its id). */
#include "game/frontend/car_records.h"
#include "game/platform/platform.h"
#include "game/sys/race_records.h"
#include "game/sys/race_records_added.h"
#include "game/sys/race_records_tracks.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size) { (void)name, (void)data, (void)size; return true; }
void platform_free_file(uint8_t *data) { free(data); }
bool platform_user_file_exists(const char *name) { (void)name; return false; }

static void set_time(RecordRow *r, int32_t t)
{
    memcpy(r->row + RECORD_ROW_TIME, &t, 4);
    const int32_t one = 1;
    memcpy(r->row + RECORD_ROW_SET, &one, 4);
}

static void set_speed(RecordRow *r, int32_t s)
{
    memcpy(r->row + RECORD_ROW_SPEED, &s, 4);
    const int32_t one = 1;
    memcpy(r->row + RECORD_ROW_SET, &one, 4);
}

enum { CIRCUIT = 3, STAGE = 0 };

static void test_no_record(void)
{
    race_records_added_reset(), race_records_tracks_reset();
    CarRecord r;
    car_records_find("f40", CIRCUIT, NULL, 0, &r);
    CHECK_EQ_INT(r.found, 0);
    CHECK_EQ_INT(r.total, 0);
    car_records_find(NULL, CIRCUIT, NULL, 0, &r);
    CHECK_EQ_INT(r.found, 0);
}

static void test_the_standard_key_wins_over_a_better_time(void)
{
    race_records_added_reset(), race_records_tracks_reset();
    RecordTable *std = race_records_added_make(CIRCUIT, LAYOUT_NORMAL, 0, 3, "f40", "F40");
    RecordTable *mirror = race_records_added_make(CIRCUIT, LAYOUT_MIRROR, 0, 3, "f40", "F40");
    set_time(&std->totals[0], 5000), set_speed(&std->top, 700);
    set_time(&mirror->totals[0], 4000);   /* faster, but not the standard key */
    set_time(race_records_added_lap_make(CIRCUIT, LAYOUT_NORMAL, 0, "f40", "F40"), 1700);
    CarRecord r;
    car_records_find("f40", CIRCUIT, NULL, 0, &r);
    CHECK_EQ_INT(r.found, 1);
    CHECK_EQ_INT(r.layout, LAYOUT_NORMAL);
    CHECK_EQ_INT(r.laps, 3);
    CHECK_EQ_INT(r.total, 5000);
    CHECK_EQ_INT(r.lap, 1700);
    CHECK_EQ_INT(r.speed, 700);
}

static void test_without_a_standard_row_the_best_total_is_taken(void)
{
    race_records_added_reset(), race_records_tracks_reset();
    set_time(&race_records_added_make(CIRCUIT, LAYOUT_MIRROR, 0, 3, "f40", "F40")->totals[0], 4000);
    set_time(&race_records_added_make(CIRCUIT, LAYOUT_REVERSE, 1, 5, "f40", "F40")->totals[0], 3000);
    CarRecord r;
    car_records_find("f40", CIRCUIT, NULL, 0, &r);
    CHECK_EQ_INT(r.found, 1);
    CHECK_EQ_INT(r.layout, LAYOUT_REVERSE);   /* the lower total */
    CHECK_EQ_INT(r.cheats, 1);
    CHECK_EQ_INT(r.laps, 5);
    CHECK_EQ_INT(r.total, 3000);
    CHECK_EQ_INT(r.lap, 0);                   /* no best lap kept */
}

static void test_a_stage_has_segments(void)
{
    race_records_added_reset(), race_records_tracks_reset();
    RecordTable *t = race_records_added_make(STAGE, LAYOUT_NORMAL, 0, 0, "f40", "F40");
    set_time(&t->totals[0], 9000);
    set_time(&t->segment_time[0], 100), set_time(&t->segment_time[2], 300);
    CarRecord r;
    car_records_find("f40", STAGE, NULL, 1, &r);
    CHECK_EQ_INT(r.found, 1);
    CHECK_EQ_INT(r.stage, 1);
    CHECK_EQ_INT(r.total, 9000);
    CHECK_EQ_INT(r.segment[0], 100);
    CHECK_EQ_INT(r.segment[1], 0);
    CHECK_EQ_INT(r.segment[2], 300);
}

static void test_an_added_track_is_read_by_its_id(void)
{
    race_records_added_reset(), race_records_tracks_reset();
    set_time(&race_records_tracks_make("dune", LAYOUT_NORMAL, 0, 2, "f40", "F40")->totals[0], 7777);
    set_time(&race_records_tracks_make("dune", LAYOUT_NORMAL, 0, 2, "alpine", "A110")->totals[0], 6666);
    CarRecord r;
    car_records_find("f40", -1, "dune", 0, &r);
    CHECK_EQ_INT(r.found, 1);
    CHECK_EQ_INT(r.total, 7777);   /* the car's own, not another's */
    car_records_find("gto", -1, "dune", 0, &r);
    CHECK_EQ_INT(r.found, 0);
}

int main(void)
{
    test_no_record();
    test_the_standard_key_wins_over_a_better_time();
    test_without_a_standard_row_the_best_total_is_taken();
    test_a_stage_has_segments();
    test_an_added_track_is_read_by_its_id();
    return TEST_REPORT();
}
