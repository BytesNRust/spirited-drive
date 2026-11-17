/* dep59 step 4 (Task 4): the records an edition shows (game/sys/race_records_merge.h). 1000 races at random (a fixed seed, times all
 * different) put into race_records.dat's table and their car's, as every Restomod race is: the stock cars' tables joined give
 * race_records.dat's ten, top speed and segments; one car's give its rows alone; none give nothing. The screens read race_records.dat in
 * the standard edition and the edition's cars' tables in one of its own. The player folder is a scratch one (TNFS_PREF_DIR). */
#include "formats/stock_content.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "game/sys/race_records.h"
#include "game/sys/race_records_added.h"
#include "game/sys/race_records_merge.h"
#include "game/sys/race_records_tracks.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { STAGE = 1, CIRCUIT = 3, LAPS = 2, RACES = 1000, ROWS = RACE_RECORDS_ROWS, SEGMENTS = RACE_RECORDS_SEGMENTS };

static const int CARS[] = { 0, 1, 2, 3, 4, 5, 6, 7, STOCK_CAR_WARRIOR };
enum { CAR_COUNT = (int)(sizeof CARS / sizeof CARS[0]) };

static char base[256], pref[300];

static void make_dirs(const char *path)
{
    char buf[512];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; p++)
        if (*p == '/') { *p = '\0'; mkdir(buf, 0777); *p = '/'; }
    mkdir(buf, 0777);
}

static void write_file(const char *rel, const char *text)
{
    char path[600], dir[600];
    snprintf(path, sizeof path, "%s/%s", pref, rel);
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash != NULL) *slash = '\0';
    make_dirs(dir);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) fputs(text, f), fclose(f);
}

static int32_t field(const RecordRow *r, int at)
{
    int32_t v;
    memcpy(&v, r->row + at, sizeof v);
    return v;
}

/* a race's row: its car, time and speed, a name from its number */
static RecordRow row_of(int car, int32_t time, int32_t speed, int race)
{
    RecordRow r;
    memset(&r, 0, sizeof r);
    snprintf((char *)r.row, RECORD_ROW_NAME_MAX, "P%d", race % 1000);
    memcpy(r.row + RECORD_ROW_CAR, &car, 4), memcpy(r.row + RECORD_ROW_CAR_B, &car, 4);
    memcpy(r.row + RECORD_ROW_TIME, &time, 4), memcpy(r.row + RECORD_ROW_SPEED, &speed, 4);
    const int32_t set = 1;   /* a row the check took (race_records_row_set) */
    memcpy(r.row + RECORD_ROW_SET, &set, 4);
    r.marks.cheats = (uint8_t)race;
    return r;
}

/* the original's check (record_check.c): a total before the first row it beats (strictly), the rest one lower; a top speed or a segment
 * when strictly better */
static void put(RecordTable *t, const RecordRow *r, int stage, int segment)
{
    const int32_t time = field(r, RECORD_ROW_TIME);
    for (int i = 0; i < ROWS; i++) {
        if (!(time < field(&t->totals[i], RECORD_ROW_TIME))) continue;
        memmove(&t->totals[i + 1], &t->totals[i], sizeof t->totals[0] * (size_t)(ROWS - 1 - i));
        t->totals[i] = *r;
        break;
    }
    if (!stage && field(&t->top, RECORD_ROW_SPEED) < field(r, RECORD_ROW_SPEED)) t->top = *r;
    if (stage) {
        if (time < field(&t->segment_time[segment], RECORD_ROW_TIME)) t->segment_time[segment] = *r;
        if (field(&t->segment_speed[segment], RECORD_ROW_SPEED) < field(r, RECORD_ROW_SPEED)) t->segment_speed[segment] = *r;
    }
}

static void put_lap(RecordRow *lap, const RecordRow *r)
{
    if (field(r, RECORD_ROW_TIME) < field(lap, RECORD_ROW_TIME)) *lap = *r;
}

/* RACES races on `track`, each into race_records.dat's table and its car's, every time different (a shuffled run of them) */
static void race_at_random(int track, unsigned seed)
{
    static int32_t times[RACES];
    for (int i = 0; i < RACES; i++) times[i] = 60000 + 7 * i;
    srand(seed);
    for (int i = RACES - 1; i > 0; i--) {
        const int j = rand() % (i + 1);
        const int32_t x = times[i];
        times[i] = times[j], times[j] = x;
    }
    const int stage = track < RACE_RECORDS_FIRST_CIRCUIT, laps = stage ? 0 : LAPS;
    for (int i = 0; i < RACES; i++) {
        const int car = CARS[rand() % CAR_COUNT];
        const RecordRow r = row_of(car, times[i], 100 + (int32_t)((times[i] * 31) % 9973), i);
        const int segment = rand() % SEGMENTS;
        put(race_records_make(track, 0, 0, laps), &r, stage, segment);
        put(race_records_added_make(track, 0, 0, laps, stock_car_id(car), NULL), &r, stage, segment);
        if (!stage) {
            put_lap(race_records_lap_make(track, 0, 0), &r);
            put_lap(race_records_added_lap_make(track, 0, 0, stock_car_id(car), NULL), &r);
        }
    }
}

static int same_row(const RecordRow *a, const RecordRow *b) { return memcmp(a, b, sizeof *a) == 0; }

static void stock_ids(const char *ids[])
{
    for (int c = 0; c < CAR_COUNT; c++) ids[c] = stock_car_id(CARS[c]);
}

static void test_the_stock_cars_joined_are_the_standard_table(void)
{
    race_records_reset(), race_records_added_reset();
    race_at_random(CIRCUIT, 59);
    const char *ids[CAR_COUNT];
    stock_ids(ids);
    RecordTable joined;
    CHECK(race_records_cars_merge(CIRCUIT, 0, 0, LAPS, ids, CAR_COUNT, &joined));
    const RecordTable *const standard = race_records_find(CIRCUIT, 0, 0, LAPS);
    CHECK(standard != NULL);
    if (standard == NULL) return;
    for (int i = 0; i < ROWS; i++) CHECK(same_row(&joined.totals[i], &standard->totals[i]));
    CHECK(same_row(&joined.top, &standard->top));
    RecordRow lap;
    CHECK(race_records_cars_lap_merge(CIRCUIT, 0, 0, ids, CAR_COUNT, &lap));
    CHECK(same_row(&lap, race_records_lap_find(CIRCUIT, 0, 0)));
}

static void test_a_stage_joins_its_segments(void)
{
    race_records_reset(), race_records_added_reset();
    race_at_random(STAGE, 4);
    const char *ids[CAR_COUNT];
    stock_ids(ids);
    RecordTable joined;
    CHECK(race_records_cars_merge(STAGE, 0, 0, 0, ids, CAR_COUNT, &joined));
    const RecordTable *const standard = race_records_find(STAGE, 0, 0, 0);
    CHECK(standard != NULL);
    if (standard == NULL) return;
    for (int i = 0; i < ROWS; i++) CHECK(same_row(&joined.totals[i], &standard->totals[i]));
    for (int k = 0; k < SEGMENTS; k++) {
        CHECK(same_row(&joined.segment_time[k], &standard->segment_time[k]));
        CHECK(same_row(&joined.segment_speed[k], &standard->segment_speed[k]));
    }
}

/* one car: its rows alone, its own ten; no car, or a key no car has: nothing */
static void test_one_car_and_none(void)
{
    race_records_reset(), race_records_added_reset();
    race_at_random(CIRCUIT, 7);
    const char *rx7[] = { "rx7" };
    RecordTable joined;
    CHECK(race_records_cars_merge(CIRCUIT, 0, 0, LAPS, rx7, 1, &joined));
    const RecordTable *const own = race_records_added_find(CIRCUIT, 0, 0, LAPS, "rx7");
    CHECK(own != NULL);
    for (int i = 0; i < ROWS; i++) {
        CHECK_EQ_INT(field(&joined.totals[i], RECORD_ROW_CAR), 0);
        if (own != NULL) CHECK(same_row(&joined.totals[i], &own->totals[i]));
    }
    CHECK(!race_records_cars_merge(CIRCUIT, 0, 0, LAPS, rx7, 0, &joined));
    CHECK(!race_records_row_set(&joined.totals[0]));
    CHECK(!race_records_cars_merge(CIRCUIT, LAYOUT_MIRROR, 0, LAPS, rx7, 1, &joined));
    RecordRow lap;
    CHECK(!race_records_cars_lap_merge(CIRCUIT, LAYOUT_MIRROR, 0, rx7, 1, &lap));
    CHECK(!race_records_row_set(&lap));
}

/* equal times: the edition's car order (the first car's row first) */
static void test_equal_times_keep_the_car_order(void)
{
    race_records_reset(), race_records_added_reset();
    const RecordRow a = row_of(1, 70000, 150, 1), b = row_of(2, 70000, 150, 2);
    put(race_records_added_make(CIRCUIT, 0, 0, LAPS, "nsx", NULL), &a, 0, 0);
    put(race_records_added_make(CIRCUIT, 0, 0, LAPS, "supra", NULL), &b, 0, 0);
    const char *order[] = { "supra", "nsx" };
    RecordTable joined;
    CHECK(race_records_cars_merge(CIRCUIT, 0, 0, LAPS, order, 2, &joined));
    CHECK(same_row(&joined.totals[0], &b));
    CHECK(same_row(&joined.totals[1], &a));
    CHECK(same_row(&joined.top, &b));
    CHECK_EQ_INT(race_records_row_index(joined.totals, ROWS, &a), 1);
    const RecordRow renumbered = row_of(40, 70000, 150, 1);   /* a's but for its car words */
    CHECK_EQ_INT(race_records_row_index(joined.totals, ROWS, &renumbered), 1);
    const RecordRow other = row_of(3, 1, 1, 3);
    CHECK_EQ_INT(race_records_row_index(joined.totals, ROWS, &other), -1);
}

static void edition_set(const char *id)
{
    PortSettings s = port_settings_of_profile(PROFILE_RESTOMOD);
    snprintf(s.edition, sizeof s.edition, "%s", id);
    const PortExtras x = port_extras_of_profile(PROFILE_RESTOMOD);
    CHECK(port_mode_save(&s, &x));
}

/* the screens: the standard edition reads race_records.dat (a row of a car with no id there, the PACK's, is shown), an edition its
 * cars' tables (the PACK's row is not), Stock race_records.dat too */
static void test_what_the_screens_read(void)
{
    race_records_reset(), race_records_added_reset();
    const RecordRow pack = row_of(8, 50000, 200, 8), viper = row_of(5, 60000, 190, 5), rx7 = row_of(0, 65000, 180, 0);
    put(race_records_make(CIRCUIT, 0, 0, LAPS), &pack, 0, 0);
    put(race_records_make(CIRCUIT, 0, 0, LAPS), &viper, 0, 0);
    put(race_records_make(CIRCUIT, 0, 0, LAPS), &rx7, 0, 0);
    put(race_records_added_make(CIRCUIT, 0, 0, LAPS, "viper", NULL), &viper, 0, 0);
    put(race_records_added_make(CIRCUIT, 0, 0, LAPS, "rx7", NULL), &rx7, 0, 0);
    put_lap(race_records_lap_make(CIRCUIT, 0, 0), &pack);
    put_lap(race_records_added_lap_make(CIRCUIT, 0, 0, "rx7", NULL), &rx7);
    write_file("editions/duo.ini", "name=Duo\ncars=rx7,gone-car\ntracks=rusty\n");

    CHECK(port_profile_select(PROFILE_RESTOMOD));
    edition_set("");
    const RecordTable *t = race_records_shown(CIRCUIT, 0, 0, LAPS);
    CHECK(t == race_records_find(CIRCUIT, 0, 0, LAPS));
    CHECK(t != NULL && same_row(&t->totals[0], &pack));
    CHECK(race_records_shown_lap(CIRCUIT, 0, 0) == race_records_lap_find(CIRCUIT, 0, 0));

    edition_set("duo");
    port_log_watch("records: edition Duo joins rx7");   /* the log says which cars an edition's records are (a car not there left out) */
    t = race_records_shown(CIRCUIT, 0, 0, LAPS);
    CHECK(port_log_watch_hit());
    port_log_watch("records: edition Duo joins");   /* once: the screens read it again and again */
    (void)race_records_shown(CIRCUIT, 0, 0, LAPS);
    CHECK(!port_log_watch_hit());
    CHECK(t != NULL && t != race_records_find(CIRCUIT, 0, 0, LAPS));
    if (t != NULL) {
        CHECK(same_row(&t->totals[0], &rx7));
        CHECK(!race_records_row_set(&t->totals[1]));
        CHECK(same_row(&t->top, &rx7));
    }
    const RecordRow *lap = race_records_shown_lap(CIRCUIT, 0, 0);
    CHECK(lap != NULL && same_row(lap, &rx7));
    CHECK(race_records_shown(CIRCUIT, LAYOUT_MIRROR, 0, LAPS) == NULL);   /* none of its cars has raced there */
    put(race_records_added_make(CIRCUIT, 0, 0, 7, "viper", NULL), &viper, 0, 0);   /* a lap count only a car not in it has */
    put(race_records_added_make(CIRCUIT, 0, 0, 5, "rx7", NULL), &rx7, 0, 0);
    int laps[RACE_RECORDS_LAPS_MAX];
    CHECK_EQ_INT(race_records_shown_laps(CIRCUIT, 0, 0, laps, RACE_RECORDS_LAPS_MAX), 2);
    CHECK_EQ_INT(laps[0], LAPS);
    CHECK_EQ_INT(laps[1], 5);

    edition_set("missing");   /* an edition that cannot be used: the standard one */
    CHECK(race_records_shown(CIRCUIT, 0, 0, LAPS) == race_records_find(CIRCUIT, 0, 0, LAPS));

    edition_set("duo");
    CHECK(port_profile_select(PROFILE_STOCK));   /* Stock has no edition */
    CHECK(race_records_shown(CIRCUIT, 0, 0, LAPS) == race_records_find(CIRCUIT, 0, 0, LAPS));
    CHECK_EQ_INT(race_records_shown_laps(CIRCUIT, 0, 0, laps, RACE_RECORDS_LAPS_MAX), 1);   /* race_records.dat's: LAPS alone */
}

/* dep59 L6: Lunar Springs' records (race_records_tracks.dat, one table per car) shown the same way: the standard edition joins every car
 * that has a table for the track, an edition of its own only its cars; another track's tables are never joined in */
static void test_a_track_joins_its_cars(void)
{
    race_records_tracks_reset();
    const RecordRow viper = row_of(5, 60000, 190, 5), rx7 = row_of(0, 65000, 180, 0), pack = row_of(8, 50000, 200, 8), elsewhere = row_of(1, 1000, 1, 1);
    put(race_records_tracks_make("lunar", 0, 0, LAPS, "viper", NULL), &viper, 0, 0);
    put(race_records_tracks_make("lunar", 0, 0, LAPS, "rx7", NULL), &rx7, 0, 0);
    put(race_records_tracks_make("lunar", 0, 0, LAPS, "pack", NULL), &pack, 0, 0);
    put(race_records_tracks_make("moonbase", 0, 0, LAPS, "viper", NULL), &elsewhere, 0, 0);
    put_lap(race_records_tracks_lap_make("lunar", 0, 0, "viper", NULL), &viper);
    put_lap(race_records_tracks_lap_make("lunar", 0, 0, "rx7", NULL), &rx7);
    put(race_records_tracks_make("lunar", 0, 0, 5, "viper", NULL), &viper, 0, 0);   /* a lap count only the viper has */
    write_file("editions/duo.ini", "name=Duo\ncars=rx7,gone-car\ntracks=rusty\n");
    CHECK(port_profile_select(PROFILE_RESTOMOD));

    edition_set("");
    const char *ids[8];
    CHECK_EQ_INT(race_records_tracks_cars("lunar", ids, 8), 3);   /* every car with a table there, by id */
    CHECK(strcmp(ids[0], "pack") == 0 && strcmp(ids[1], "rx7") == 0 && strcmp(ids[2], "viper") == 0);
    CHECK_EQ_INT(race_records_tracks_cars("nowhere", ids, 8), 0);
    CHECK_EQ_INT(race_records_tracks_cars("lunar", ids, 2), 2);   /* at most max */
    CHECK(strcmp(ids[0], "pack") == 0 && strcmp(ids[1], "rx7") == 0);   /* the ascending prefix */
    const RecordTable *t = race_records_shown_track("lunar", 0, 0, LAPS);
    CHECK(t != NULL);
    if (t != NULL) {
        CHECK(same_row(&t->totals[0], &pack) && same_row(&t->totals[1], &viper) && same_row(&t->totals[2], &rx7));
        CHECK(!race_records_row_set(&t->totals[3]));
        CHECK(same_row(&t->top, &pack));
    }
    const RecordRow *lap = race_records_shown_track_lap("lunar", 0, 0);
    CHECK(lap != NULL && same_row(lap, &viper));
    int laps[RACE_RECORDS_LAPS_MAX];
    CHECK_EQ_INT(race_records_shown_track_laps("lunar", 0, 0, laps, RACE_RECORDS_LAPS_MAX), 2);
    CHECK(laps[0] == LAPS && laps[1] == 5);
    CHECK(race_records_shown_track("lunar", LAYOUT_MIRROR, 0, LAPS) == NULL);   /* nobody raced it mirrored */
    CHECK(race_records_shown_track("nowhere", 0, 0, LAPS) == NULL);

    edition_set("duo");   /* its cars: the rx7 (the other id has no table) */
    t = race_records_shown_track("lunar", 0, 0, LAPS);
    CHECK(t != NULL && same_row(&t->totals[0], &rx7) && !race_records_row_set(&t->totals[1]));
    lap = race_records_shown_track_lap("lunar", 0, 0);
    CHECK(lap != NULL && same_row(lap, &rx7));
    CHECK_EQ_INT(race_records_shown_track_laps("lunar", 0, 0, laps, RACE_RECORDS_LAPS_MAX), 1);
    CHECK_EQ_INT(laps[0], LAPS);
    CHECK(race_records_shown_track("moonbase", 0, 0, LAPS) == NULL);   /* the rx7 never raced there */

    edition_set("");
    put_lap(race_records_tracks_lap_make("solo", 0, 0, "viper", NULL), &viper);   /* a car with a best lap and no table */
    CHECK_EQ_INT(race_records_tracks_cars("solo", ids, 8), 1);
    CHECK(race_records_shown_track("solo", 0, 0, LAPS) == NULL);
    lap = race_records_shown_track_lap("solo", 0, 0);
    CHECK(lap != NULL && same_row(lap, &viper));
    write_file("editions/absent.ini", "name=Absent\ncars=gone-car\ntracks=rusty\n");
    edition_set("absent");   /* an edition none of whose cars is there: the cars of the file, as the standard edition */
    t = race_records_shown_track("lunar", 0, 0, LAPS);
    CHECK(t != NULL && same_row(&t->totals[0], &pack));

    edition_set("");
    CHECK(port_profile_select(PROFILE_STOCK));   /* no Restomod, no edition, but the join of the file is the same */
    t = race_records_shown_track("lunar", 0, 0, LAPS);
    CHECK(t != NULL && same_row(&t->totals[0], &pack));
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    race_records_tracks_reset();
}

int main(void)
{
    snprintf(base, sizeof base, "%s/tnfs_records_merge_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(pref, sizeof pref, "%s/pref", base);
    make_dirs(pref);
    setenv("TNFS_PREF_DIR", pref, 1);
    test_the_stock_cars_joined_are_the_standard_table();
    test_a_stage_joins_its_segments();
    test_one_car_and_none();
    test_equal_times_keep_the_car_order();
    test_what_the_screens_read();
    test_a_track_joins_its_cars();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
