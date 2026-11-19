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
    CHECK_EQ_INT(r.laps, 1);   /* a lap says so */
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

/* owner 2026-11-19 (point 28): records made before the records by car (race_records.dat, the standard rows), copied into the cars' tables by the import */
static void set_car(RecordRow *r, int32_t car)
{
    memcpy(r->row + RECORD_ROW_CAR, &car, 4), memcpy(r->row + RECORD_ROW_CAR_B, &car, 4);
}

enum { DIABLO = 7, TR512 = 6, RUSTY = 3, CITY = 0 };

static void test_imported_records_show_for_each_car(void)
{
    race_records_reset(), race_records_added_reset(), race_records_tracks_reset();
    race_records_cars_set_imported(0);
    /* Rusty Springs, normal, no cheats, 8 laps: the Diablo's 5:23.3 first, the 512TR's 5:29.9 second, the 512TR's top speed */
    RecordTable *rusty = race_records_make(RUSTY, LAYOUT_NORMAL, 0, 8);
    set_time(&rusty->totals[0], 32330), set_car(&rusty->totals[0], DIABLO);
    set_time(&rusty->totals[1], 32990), set_car(&rusty->totals[1], TR512);
    set_speed(&rusty->top, 2565), set_car(&rusty->top, TR512);
    /* City, a stage: the Diablo has the total and the top speed, the 512TR a total */
    RecordTable *city = race_records_make(CITY, LAYOUT_NORMAL, 0, 0);
    set_time(&city->totals[0], 40000), set_car(&city->totals[0], DIABLO);
    set_time(&city->totals[1], 44950), set_car(&city->totals[1], TR512);
    set_speed(&city->top, 3251), set_car(&city->top, DIABLO);
    CHECK_EQ_INT(race_records_cars_import(), 1);
    CarRecord r;
    car_records_find("512tr", RUSTY, NULL, 0, &r);
    CHECK_EQ_INT(r.speed, 2565);   /* its top speed is its own */
    CHECK_EQ_INT(r.best, 32990);   /* no lap row: the best total it has is what the track's Best Times shows */
    CHECK_EQ_INT(r.laps, 8);       /* said to be a race's, with its laps */
    car_records_find("diablo", RUSTY, NULL, 0, &r);
    CHECK_EQ_INT(r.best, 32330);
    CHECK_EQ_INT(r.speed, 0);   /* time and no speed: each column on its own */
    car_records_find("512tr", CITY, NULL, 1, &r);
    CHECK_EQ_INT(r.found, 1);
    CHECK_EQ_INT(r.best, 44950);
    CHECK_EQ_INT(r.laps, 0);   /* a stage's total is the course's */
    CHECK_EQ_INT(r.speed, 0);   /* the 512TR has the time and not the speed */
    car_records_find("diablo", CITY, NULL, 1, &r);
    CHECK_EQ_INT(r.best, 40000);
    CHECK_EQ_INT(r.speed, 3251);
}

int main(void)
{
    test_no_record();
    test_a_circuit_gives_its_best_lap_of_the_normal_key();
    test_the_top_speed_is_the_greatest_of_the_normal_tables();
    test_a_stage_gives_its_full_course_not_a_segment();
    test_an_added_track_is_read_by_its_id();
    test_imported_records_show_for_each_car();
    return TEST_REPORT();
}
