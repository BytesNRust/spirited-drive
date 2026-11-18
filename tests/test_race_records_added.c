/* dep59 1b: the records by car (game/sys/race_records_added.h), a store of its own beside race_records.dat: a table per key and car
 * id, so the original's ranking runs in each car's table; its file read back byte for byte, anything that is not exactly a good file
 * refused, a damaged one set aside as race_records_cars.bad.
 * dep59 step 4: the stock cars' ids too (formats/stock_content.h); the file is race_records_cars.dat (TNFSRRA2, with the flag that the
 * standard rows were copied in), and the step-3 file, race_records_added.dat (TNFSRRA1), is read when there is no new one; the standard
 * store's rows go to their cars' tables once (race_records_cars_import). */
#include "game/sys/race_records_added.h"
#include "game/sys/race_records.h"
#include "game/sys/race_records_io.h"
#include "game/sys/port_log.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

enum { FILES = 6 };
static struct {
    char name[40];
    uint8_t *data;
    size_t size;
} files[FILES];

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
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }
bool platform_user_file_exists(const char *name) { return slot_of(name, 0) >= 0; }

static void files_clear(void)
{
    for (int i = 0; i < FILES; i++) free(files[i].data), files[i].data = NULL;
}

static void set_time(RecordRow *r, int32_t t)
{
    memcpy(r->row + RECORD_ROW_TIME, &t, 4);
    const int32_t one = 1;
    memcpy(r->row + RECORD_ROW_SET, &one, 4);
}

static int32_t time_of(const RecordRow *r)
{
    int32_t t;
    memcpy(&t, r->row + RECORD_ROW_TIME, 4);
    return t;
}

enum { TR1 = 3, HEADER = 20 };

static void test_empty(void)
{
    race_records_added_reset();
    size_t size = 0;
    uint8_t *img = race_records_added_image(&size);
    CHECK(img != NULL && size == HEADER && memcmp(img, "TNFSRRA3", 8) == 0);
    CHECK_EQ_INT(race_records_added_parse(img, size), 1);
    free(img);
}

static void test_a_table_per_car(void)
{
    race_records_added_reset();
    RecordTable *f40 = race_records_added_make(TR1, 0, 0, 4, "f40", "F40");
    RecordTable *alpine = race_records_added_make(TR1, 0, 0, 4, "alpine", "A110");
    CHECK(f40 != NULL && alpine != NULL && f40 != alpine);
    CHECK(race_records_added_find(TR1, 0, 0, 4, "f40") == f40);
    CHECK(race_records_added_find(TR1, 0, 0, 4, "alpine") == alpine);
    CHECK(race_records_added_find(TR1, 0, 0, 4, "gto") == NULL);
    CHECK(race_records_added_find(TR1, 0, 0, 5, "f40") == NULL);
    CHECK(race_records_added_make(TR1, 0, 0, 4, "f40", "F40") == f40);   /* made once */
    CHECK(!race_records_row_set(&f40->totals[0]));                       /* made empty */
    CHECK(race_records_added_make(TR1, 0, 0, 4, "F40", "F") == NULL);    /* not a mod's id */
    CHECK(race_records_added_make(TR1, 0, 0, 4, "", "F") == NULL);
    CHECK(race_records_added_make(TR1, 0, 0, 4, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "F") == NULL);   /* 33 */
    CHECK(race_records_added_make(TR1, 0, 0, 4, NULL, "F") == NULL);
    CHECK(race_records_added_make(9, 0, 0, 4, "f40", "F") == NULL);       /* key out of range */
    CHECK(race_records_added_make(0, 0, 0, 4, "f40", "F") == NULL);       /* a stage has no laps */
    CHECK(strcmp(race_records_added_short_name(TR1, 0, 0, 4, "alpine"), "A110") == 0);
    RecordRow *lap = race_records_added_lap_make(TR1, 1, 0, "f40", "F40");
    CHECK(lap != NULL && race_records_added_lap_find(TR1, 1, 0, "f40") == lap);
    CHECK(race_records_added_lap_find(TR1, 1, 0, "alpine") == NULL);
}

static uint8_t *two_tables_and_a_lap(size_t *size)
{
    race_records_added_reset();
    set_time(&race_records_added_make(TR1, 0, 0, 4, "f40", "F40")->totals[0], 1234);
    set_time(&race_records_added_make(TR1, 0, 0, 4, "alpine", "A110")->totals[1], 5678);
    set_time(&race_records_added_make(0, 2, 1, 0, "f40", "F40")->segment_time[1], 99);
    set_time(race_records_added_lap_make(TR1, 1, 0, "f40", "F40"), 4321);
    return race_records_added_image(size);
}

static void test_there_and_back(void)
{
    size_t size = 0;
    uint8_t *img = two_tables_and_a_lap(&size);
    CHECK(img != NULL);
    race_records_added_reset();
    CHECK(race_records_added_find(TR1, 0, 0, 4, "f40") == NULL);
    CHECK_EQ_INT(race_records_added_parse(img, size), 1);
    CHECK_EQ_INT(time_of(&race_records_added_find(TR1, 0, 0, 4, "f40")->totals[0]), 1234);
    CHECK_EQ_INT(time_of(&race_records_added_find(TR1, 0, 0, 4, "alpine")->totals[1]), 5678);
    CHECK_EQ_INT(time_of(&race_records_added_find(0, 2, 1, 0, "f40")->segment_time[1]), 99);
    CHECK_EQ_INT(time_of(race_records_added_lap_find(TR1, 1, 0, "f40")), 4321);
    CHECK(strcmp(race_records_added_short_name(TR1, 0, 0, 4, "alpine"), "A110") == 0);
    size_t again_size = 0;
    uint8_t *again = race_records_added_image(&again_size);
    CHECK(again != NULL && again_size == size && memcmp(again, img, size) == 0);
    free(again);
    free(img);
}

/* the image of two_tables_and_a_lap with `edit` applied (its size in `size`), parsed: the answer */
static int parse_edited(void (*edit)(uint8_t *img, size_t *size))
{
    size_t size = 0;
    uint8_t *img = two_tables_and_a_lap(&size);
    uint8_t *big = malloc(size + 1);
    memcpy(big, img, size);
    free(img);
    edit(big, &size);
    const int ok = race_records_added_parse(big, size);
    free(big);
    return ok;
}

enum { PRINTS = 33, TABLE_ENTRY = 4 + 33 + 9 + PRINTS + RACE_RECORDS_TABLE_BYTES };   /* key, id, short name, the waiting byte and the print, the table */
static void bad_magic(uint8_t *img, size_t *size) { (void)size, img[7] = '4'; }
static void bad_flags(uint8_t *img, size_t *size) { (void)size, img[8] = 2; }   /* a flag this version does not know */
static void one_short(uint8_t *img, size_t *size) { (void)img, (*size)--; }
static void one_more(uint8_t *img, size_t *size) { img[*size] = 0, (*size)++; }
static void swapped(uint8_t *img, size_t *size)   /* the first two tables, (TR1 f40) and (TR1 alpine) sorted as alpine then f40, swapped */
{
    (void)size;
    uint8_t *tmp = malloc(TABLE_ENTRY);
    uint8_t *first = img + HEADER + TABLE_ENTRY, *second = first + TABLE_ENTRY;   /* after the stage table (track 0) */
    memcpy(tmp, first, TABLE_ENTRY), memcpy(first, second, TABLE_ENTRY), memcpy(second, tmp, TABLE_ENTRY);
    free(tmp);
}
static void twice(uint8_t *img, size_t *size) { (void)size, memcpy(img + HEADER + 2 * TABLE_ENTRY, img + HEADER + TABLE_ENTRY, TABLE_ENTRY); }
static void track_9(uint8_t *img, size_t *size) { (void)size, img[HEADER + 2 * TABLE_ENTRY] = 9; }
static void layout_8(uint8_t *img, size_t *size) { (void)size, img[HEADER + 2 * TABLE_ENTRY + 1] = 8; }
static void id_no_nul(uint8_t *img, size_t *size) { (void)size, memset(img + HEADER + 4, 'a', 33); }
static void id_capital(uint8_t *img, size_t *size) { (void)size, img[HEADER + 4] = 'F'; }
static void short_no_nul(uint8_t *img, size_t *size) { (void)size, memset(img + HEADER + 4 + 33, 'X', 9); }
static void lap_with_laps(uint8_t *img, size_t *size) { img[*size - (4 + 33 + 9 + PRINTS + RACE_RECORDS_ROW_BYTES) + 3] = 4; }

static void test_parse_refuses(void)
{
    void (*const BAD[])(uint8_t *, size_t *) = { bad_magic, bad_flags, one_short, one_more, swapped, twice, track_9, layout_8, id_no_nul, id_capital,
                                                 short_no_nul, lap_with_laps };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        if (parse_edited(BAD[i]) != 0) fprintf(stderr, "bad image %zu parsed\n", i);
        CHECK_EQ_INT(parse_edited(BAD[i]), 0);
        CHECK(race_records_added_find(TR1, 0, 0, 4, "f40") == NULL);   /* nothing of a refused file is kept */
    }
}

static void test_a_damaged_file_is_set_aside(void)
{
    files_clear();
    race_records_added_reset();
    CHECK_EQ_INT(race_records_added_load(), RACE_RECORDS_NO_FILE);
    size_t size = 0;
    uint8_t *img = two_tables_and_a_lap(&size);
    CHECK(platform_write_user_file("race_records_cars2.dat", img, size - 5));   /* truncated */
    free(img);
    CHECK_EQ_INT(race_records_added_load(), RACE_RECORDS_DAMAGED);
    CHECK(platform_user_file_exists("race_records_cars2.bad"));
    CHECK(race_records_added_find(TR1, 0, 0, 4, "f40") == NULL);
    set_time(&race_records_added_make(TR1, 0, 0, 4, "f40", "F40")->totals[0], 777);
    CHECK(race_records_added_save());
    race_records_added_reset();
    CHECK_EQ_INT(race_records_added_load(), RACE_RECORDS_LOADED);
    CHECK_EQ_INT(time_of(&race_records_added_find(TR1, 0, 0, 4, "f40")->totals[0]), 777);
    CHECK(!platform_user_file_exists("race_records.dat"));   /* the original store's file is not this one's */
}

/* the image of the store in an older file: step 3's (TNFSRRA1: no flags) or the one before the prints (TNFSRRA2): each entry without its print */
static uint8_t *as_old(const uint8_t *img, size_t size, size_t *old_size, int version)
{
    const uint32_t n_tables = race_records_get32(img + 12), n_laps = race_records_get32(img + 16);
    const size_t header = version == 1 ? HEADER - 4 : HEADER;
    uint8_t *old = malloc(size), *at = old;
    if (version == 1) memcpy(at, img, 8), memcpy(at + 8, img + 12, 8);
    else memcpy(at, img, HEADER);
    at[7] = (uint8_t)('0' + version), at += header;
    const uint8_t *from = img + HEADER;
    for (uint32_t i = 0; i < n_tables + n_laps; i++) {
        const size_t body = i < n_tables ? RACE_RECORDS_TABLE_BYTES : RACE_RECORDS_ROW_BYTES;
        memcpy(at, from, 4 + 33 + 9), memcpy(at + 4 + 33 + 9, from + 4 + 33 + 9 + PRINTS, body);
        at += 4 + 33 + 9 + body, from += 4 + 33 + 9 + PRINTS + body;
    }
    *old_size = (size_t)(at - old);
    return old;
}

static uint8_t *as_v1(const uint8_t *img, size_t size, size_t *v1_size) { return as_old(img, size, v1_size, 1); }

static void test_the_step_3_file_is_read(void)
{
    size_t size = 0, v1_size = 0;
    uint8_t *img = two_tables_and_a_lap(&size);
    uint8_t *v1 = as_v1(img, size, &v1_size);
    CHECK_EQ_INT(race_records_added_parse(v1, v1_size), 1);
    CHECK_EQ_INT(time_of(&race_records_added_find(TR1, 0, 0, 4, "f40")->totals[0]), 1234);
    CHECK_EQ_INT(race_records_cars_imported(), 0);
    /* in a file of its own, with no new one: read, and saved as the new one (the old one left as it was) */
    files_clear();
    CHECK(platform_write_user_file("race_records_added.dat", v1, v1_size));
    race_records_added_reset();
    port_log_watch("records: race_records_added.dat read: 3 tables, 1 laps, imported 0");   /* the log says what was read */
    CHECK_EQ_INT(race_records_added_load(), RACE_RECORDS_LOADED);
    CHECK(port_log_watch_hit());
    CHECK_EQ_INT(time_of(race_records_added_lap_find(TR1, 1, 0, "f40")), 4321);
    CHECK(race_records_added_save());
    CHECK(platform_user_file_exists("race_records_cars2.dat"));
    const int old = slot_of("race_records_added.dat", 0);
    CHECK(old >= 0 && files[old].size == v1_size && memcmp(files[old].data, v1, v1_size) == 0);
    /* the new one there: the old one is not read any more */
    race_records_added_reset();
    set_time(&race_records_added_make(TR1, 0, 0, 4, "f40", "F40")->totals[0], 55);
    race_records_cars_set_imported(1);
    CHECK(race_records_added_save());
    race_records_added_reset();
    port_log_watch("records: race_records_cars2.dat read: 1 tables, 0 laps, imported 1");
    CHECK_EQ_INT(race_records_added_load(), RACE_RECORDS_LOADED);
    CHECK(port_log_watch_hit());
    CHECK_EQ_INT(time_of(&race_records_added_find(TR1, 0, 0, 4, "f40")->totals[0]), 55);
    CHECK(race_records_added_lap_find(TR1, 1, 0, "f40") == NULL);
    /* a damaged old one with no new one is set aside under its own name */
    files_clear();
    CHECK(platform_write_user_file("race_records_added.dat", v1, v1_size - 3));
    CHECK_EQ_INT(race_records_added_load(), RACE_RECORDS_DAMAGED);
    CHECK(platform_user_file_exists("race_records_added.bad"));
    free(v1);
    free(img);
}

static void test_the_imported_flag_is_kept(void)
{
    race_records_added_reset();
    CHECK_EQ_INT(race_records_cars_imported(), 0);
    race_records_cars_set_imported(1);
    size_t size = 0;
    uint8_t *img = race_records_added_image(&size);
    CHECK(img != NULL && img[8] == 1);
    race_records_added_reset();
    CHECK_EQ_INT(race_records_cars_imported(), 0);
    CHECK_EQ_INT(race_records_added_parse(img, size), 1);
    CHECK_EQ_INT(race_records_cars_imported(), 1);
    free(img);
}

static void test_the_stock_cars_have_tables(void)
{
    race_records_added_reset();
    CHECK(race_records_added_make(TR1, 0, 0, 4, "rx7", NULL) != NULL);
    CHECK(race_records_added_make(TR1, 0, 1, 4, "warrior", NULL) != NULL);
    CHECK(race_records_added_make(TR1, 0, 0, 4, "911", NULL) != NULL);
    CHECK(race_records_added_lap_make(TR1, 0, 0, "512tr", NULL) != NULL);
}

static void set_car(RecordRow *r, int32_t car)
{
    memcpy(r->row + RECORD_ROW_CAR, &car, 4);
    memcpy(r->row + RECORD_ROW_CAR_B, &car, 4);
}

/* a standard table's row at `i`: its time and car */
static void standard_row(RecordTable *t, int i, int32_t time, int32_t car)
{
    set_time(&t->totals[i], time);
    set_car(&t->totals[i], car);
    t->totals[i].marks.cheats = (uint8_t)(i + 1);
}

static void test_the_standard_rows_are_copied_once(void)
{
    files_clear();
    race_records_reset();
    race_records_added_reset();
    RecordTable *t = race_records_make(TR1, 0, 0, 4);
    standard_row(t, 0, 100, 0);    /* rx7 */
    standard_row(t, 1, 200, 5);    /* viper */
    standard_row(t, 2, 300, 8);    /* PACK: no car of the store's */
    standard_row(t, 3, 400, 11);   /* warrior */
    standard_row(t, 4, 500, 0);    /* rx7 again */
    set_time(&t->top, 9), set_car(&t->top, 5);
    RecordRow *lap = race_records_lap_make(TR1, 0, 0);
    set_time(lap, 77), set_car(lap, 2);
    RecordTable *stage = race_records_make(0, LAYOUT_MIRROR, 1, 0);
    standard_row(stage, 0, 1000, 6);
    set_time(&stage->segment_time[1], 333), set_car(&stage->segment_time[1], 7);
    stage->stage_totals[0] = 11, stage->stage_speeds[2] = 22;   /* the best run's, whose car is 512tr's */
    CHECK(race_records_save());
    const int std = slot_of("race_records.dat", 0);
    CHECK(std >= 0);
    const size_t std_size = files[std].size;
    uint8_t *std_before = malloc(std_size);
    memcpy(std_before, files[std].data, std_size);

    CHECK_EQ_INT(race_records_cars_import(), 1);
    CHECK_EQ_INT(race_records_cars_imported(), 1);
    const RecordTable *rx7 = race_records_added_find(TR1, 0, 0, 4, "rx7");
    CHECK(rx7 != NULL && time_of(&rx7->totals[0]) == 100 && time_of(&rx7->totals[1]) == 500 && !race_records_row_set(&rx7->totals[2]));
    CHECK(rx7 != NULL && rx7->totals[1].marks.cheats == 5);   /* its marks kept */
    const RecordTable *viper = race_records_added_find(TR1, 0, 0, 4, "viper");
    CHECK(viper != NULL && time_of(&viper->totals[0]) == 200 && time_of(&viper->top) == 9);
    CHECK(rx7 != NULL && !race_records_row_set(&rx7->top));
    const RecordTable *warrior = race_records_added_find(TR1, 0, 0, 4, "warrior");
    CHECK(warrior != NULL && time_of(&warrior->totals[0]) == 400);
    CHECK(race_records_added_lap_find(TR1, 0, 0, "supra") != NULL && time_of(race_records_added_lap_find(TR1, 0, 0, "supra")) == 77);
    CHECK(race_records_added_lap_find(TR1, 0, 0, "rx7") == NULL);
    const RecordTable *tr512 = race_records_added_find(0, LAYOUT_MIRROR, 1, 0, "512tr");
    CHECK(tr512 != NULL && time_of(&tr512->totals[0]) == 1000 && tr512->stage_totals[0] == 11 && tr512->stage_speeds[2] == 22);
    const RecordTable *diablo = race_records_added_find(0, LAYOUT_MIRROR, 1, 0, "diablo");
    CHECK(diablo != NULL && time_of(&diablo->segment_time[1]) == 333 && !race_records_row_set(&diablo->totals[0]));
    CHECK(diablo != NULL && diablo->stage_totals[0] == 0);   /* not its best run */
    /* the standard store, and its file, as they were */
    CHECK(race_records_save());
    CHECK(files[std].size == std_size && memcmp(files[std].data, std_before, std_size) == 0);
    CHECK(time_of(&race_records_find(TR1, 0, 0, 4)->totals[2]) == 300);
    /* saved with the flag: a second time copies nothing */
    CHECK(race_records_added_save());
    race_records_added_reset();
    CHECK_EQ_INT(race_records_added_load(), RACE_RECORDS_LOADED);
    CHECK_EQ_INT(race_records_cars_import(), 0);
    rx7 = race_records_added_find(TR1, 0, 0, 4, "rx7");
    CHECK(rx7 != NULL && time_of(&rx7->totals[1]) == 500 && !race_records_row_set(&rx7->totals[2]));
    free(std_before);
    race_records_reset();
}


/* the print the registry gives "mod-car" (a test sets it); the other cars have none */
static uint8_t mod_print[32];
static void car_print_of(const char *id, uint8_t out[32]) { if (strcmp(id, "mod-car") == 0) memcpy(out, mod_print, 32); }

/* a car that changed has records of its own, and gets the old ones back when it is as it was; a car with no print never changes */
static void test_a_changed_car_has_its_own_records(void)
{
    race_records_added_reset();
    race_records_added_prints_set(car_print_of);
    memset(mod_print, 1, 32);
    set_time(&race_records_added_make(TR1, 0, 0, 4, "mod-car", NULL)->totals[0], 111);
    set_time(race_records_added_lap_make(TR1, 1, 0, "mod-car", NULL), 11);
    set_time(&race_records_added_make(TR1, 0, 0, 4, "f40", NULL)->totals[0], 5);
    memset(mod_print, 2, 32);
    CHECK(race_records_added_find(TR1, 0, 0, 4, "mod-car") == NULL && race_records_added_lap_find(TR1, 1, 0, "mod-car") == NULL);
    CHECK_EQ_INT(time_of(&race_records_added_find(TR1, 0, 0, 4, "f40")->totals[0]), 5);
    set_time(&race_records_added_make(TR1, 0, 0, 4, "mod-car", NULL)->totals[0], 222);
    memset(mod_print, 1, 32);
    CHECK_EQ_INT(time_of(&race_records_added_find(TR1, 0, 0, 4, "mod-car")->totals[0]), 111);
    size_t size = 0;   /* on disk with their prints, and back */
    uint8_t *img = race_records_added_image(&size);
    CHECK(img != NULL);
    race_records_added_reset();
    CHECK_EQ_INT(race_records_added_parse(img, size), 1);
    CHECK_EQ_INT(time_of(&race_records_added_find(TR1, 0, 0, 4, "mod-car")->totals[0]), 111);
    CHECK_EQ_INT(time_of(race_records_added_lap_find(TR1, 1, 0, "mod-car")), 11);
    memset(mod_print, 2, 32);
    CHECK_EQ_INT(time_of(&race_records_added_find(TR1, 0, 0, 4, "mod-car")->totals[0]), 222);
    size_t again_size = 0;
    uint8_t *again = race_records_added_image(&again_size);
    CHECK(again != NULL && again_size == size && memcmp(again, img, size) == 0);
    free(again);
    free(img);
    race_records_added_prints_set(NULL);
}

/* the records of a file before the prints are taken as the car's as it is at the first look, once; the old file is left as it was */
static void test_the_file_before_the_prints_is_adopted_once(void)
{
    files_clear();
    race_records_added_reset();
    race_records_added_prints_set(NULL);
    size_t size = 0, old_size = 0;
    uint8_t *img = two_tables_and_a_lap(&size), *old = as_old(img, size, &old_size, 2);
    free(img);
    CHECK(platform_write_user_file("race_records_cars.dat", old, old_size));
    race_records_added_prints_set(car_print_of);
    memset(mod_print, 3, 32);
    CHECK_EQ_INT(race_records_added_load(), RACE_RECORDS_LOADED);
    CHECK_EQ_INT(time_of(&race_records_added_find(TR1, 0, 0, 4, "f40")->totals[0]), 1234);   /* a stock car: no print, as ever */
    memset(mod_print, 4, 32);
    CHECK(race_records_added_find(TR1, 0, 0, 4, "f40") != NULL);
    CHECK(race_records_added_save());
    CHECK(platform_user_file_exists("race_records_cars2.dat"));
    const int kept = slot_of("race_records_cars.dat", 0);
    CHECK(kept >= 0 && files[kept].size == old_size && memcmp(files[kept].data, old, old_size) == 0);
    race_records_added_prints_set(NULL);
    free(old);
}

/* an old file's records of a mod's car wait for the car, whichever print it gets first is the one they are put to */
static void test_the_old_records_of_a_mod_car_wait_for_it(void)
{
    files_clear();
    race_records_added_reset();
    race_records_added_prints_set(NULL);
    race_records_added_reset();
    set_time(&race_records_added_make(TR1, 0, 0, 4, "mod-car", NULL)->totals[0], 99);
    size_t size = 0, old_size = 0;
    uint8_t *img = race_records_added_image(&size), *old = as_old(img, size, &old_size, 2);
    free(img);
    CHECK(platform_write_user_file("race_records_cars.dat", old, old_size));
    free(old);
    race_records_added_prints_set(car_print_of);
    memset(mod_print, 0, 32);   /* its piece is not loaded yet: no print */
    CHECK_EQ_INT(race_records_added_load(), RACE_RECORDS_LOADED);
    CHECK(race_records_added_find(TR1, 0, 0, 4, "mod-car") != NULL);   /* waiting, still found as the print-less */
    memset(mod_print, 6, 32);
    CHECK_EQ_INT(time_of(&race_records_added_find(TR1, 0, 0, 4, "mod-car")->totals[0]), 99);   /* taken as the loaded car's */
    memset(mod_print, 7, 32);
    CHECK(race_records_added_find(TR1, 0, 0, 4, "mod-car") == NULL);   /* and not again as another one's */
    race_records_added_prints_set(NULL);
}

int main(void)
{
    test_empty();
    test_a_table_per_car();
    test_there_and_back();
    test_parse_refuses();
    test_a_damaged_file_is_set_aside();
    test_the_step_3_file_is_read();
    test_the_imported_flag_is_kept();
    test_the_stock_cars_have_tables();
    test_the_standard_rows_are_copied_once();
    test_a_changed_car_has_its_own_records();
    test_the_file_before_the_prints_is_adopted_once();
    test_the_old_records_of_a_mod_car_wait_for_it();
    files_clear();
    race_records_added_reset();
    return TEST_REPORT();
}
