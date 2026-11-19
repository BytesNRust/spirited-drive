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
    CHECK_EQ_INT(r.best, 0);
    CHECK_EQ_INT(r.speed, 0);
    car_records_find(NULL, CIRCUIT, NULL, 0, &r);
    CHECK_EQ_INT(r.found, 0);
}

/* a circuit: the lap of the normal layout without cheats; the other keys' laps are not read */
static void test_a_circuit_gives_its_best_lap_of_the_normal_key(void)
{
    race_records_added_reset(), race_records_tracks_reset();
    set_time(race_records_added_lap_make(CIRCUIT, LAYOUT_NORMAL, 0, "f40", "F40"), 1700);
    set_time(race_records_added_lap_make(CIRCUIT, LAYOUT_MIRROR, 0, "f40", "F40"), 1500);   /* faster, another layout */
    set_time(race_records_added_lap_make(CIRCUIT, LAYOUT_NORMAL, 1, "f40", "F40"), 1400);   /* faster, cheats */
    CarRecord r;
    car_records_find("f40", CIRCUIT, NULL, 0, &r);
    CHECK_EQ_INT(r.found, 1);
    CHECK_EQ_INT(r.stage, 0);
    CHECK_EQ_INT(r.best, 1700);
    CHECK_EQ_INT(r.speed, 0);
}

/* the top speed: the greatest of the normal key's tables, one for each lap count, and not of the mirror's or the cheats' */
static void test_the_top_speed_is_the_greatest_of_the_normal_tables(void)
{
    race_records_added_reset(), race_records_tracks_reset();
    set_speed(&race_records_added_make(CIRCUIT, LAYOUT_NORMAL, 0, 3, "f40", "F40")->top, 700);
    set_speed(&race_records_added_make(CIRCUIT, LAYOUT_NORMAL, 0, 8, "f40", "F40")->top, 910);
    set_speed(&race_records_added_make(CIRCUIT, LAYOUT_NORMAL, 0, 5, "f40", "F40")->top, 850);
    set_speed(&race_records_added_make(CIRCUIT, LAYOUT_MIRROR, 0, 3, "f40", "F40")->top, 990);
    set_speed(&race_records_added_make(CIRCUIT, LAYOUT_NORMAL, 1, 3, "f40", "F40")->top, 999);
    CarRecord r;
    car_records_find("f40", CIRCUIT, NULL, 0, &r);
    CHECK_EQ_INT(r.found, 1);
    CHECK_EQ_INT(r.speed, 910);
    CHECK_EQ_INT(r.best, 0);   /* no lap kept: a speed alone is a row */
}

/* a stage: the total of the whole course, never a segment's own time */
static void test_a_stage_gives_its_full_course_not_a_segment(void)
{
    race_records_added_reset(), race_records_tracks_reset();
    RecordTable *t = race_records_added_make(STAGE, LAYOUT_NORMAL, 0, 0, "f40", "F40");
    set_time(&t->segment_time[0], 100), set_time(&t->segment_time[2], 300);   /* a segment, ended without the course finished */
    set_speed(&t->segment_speed[1], 880);
    CarRecord r;
    car_records_find("f40", STAGE, NULL, 1, &r);
    CHECK_EQ_INT(r.found, 0);   /* segments alone are no record of the stage */
    CHECK_EQ_INT(r.best, 0);
    set_time(&t->totals[0], 9000);
    set_speed(&t->top, 640);
    car_records_find("f40", STAGE, NULL, 1, &r);
    CHECK_EQ_INT(r.found, 1);
    CHECK_EQ_INT(r.stage, 1);
    CHECK_EQ_INT(r.best, 9000);
    CHECK_EQ_INT(r.speed, 640);
}

static void test_an_added_track_is_read_by_its_id(void)
{
    race_records_added_reset(), race_records_tracks_reset();
    set_speed(&race_records_tracks_make("dune", LAYOUT_NORMAL, 0, 2, "f40", "F40")->top, 777);
    set_speed(&race_records_tracks_make("dune", LAYOUT_NORMAL, 0, 2, "alpine", "A110")->top, 666);
    set_time(race_records_tracks_lap_make("dune", LAYOUT_NORMAL, 0, "f40", "F40"), 2200);
    CarRecord r;
    car_records_find("f40", -1, "dune", 0, &r);
    CHECK_EQ_INT(r.found, 1);
    CHECK_EQ_INT(r.speed, 777);   /* the car's own, not another's */
    CHECK_EQ_INT(r.best, 2200);
    car_records_find("gto", -1, "dune", 0, &r);
    CHECK_EQ_INT(r.found, 0);
}

int main(void)
{
    test_no_record();
    test_a_circuit_gives_its_best_lap_of_the_normal_key();
    test_the_top_speed_is_the_greatest_of_the_normal_tables();
    test_a_stage_gives_its_full_course_not_a_segment();
    test_an_added_track_is_read_by_its_id();
    return TEST_REPORT();
}
