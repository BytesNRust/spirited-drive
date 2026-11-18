/* dep59 step 5 T5: the records of the tracks race_records.dat has no table for (game/sys/race_records_tracks.h): Lunar Springs ("lunar")
 * and the added tracks, a table per key, track id and car id; its file read back byte for byte, anything that is not exactly a good file
 * refused, a damaged one set aside as race_records_tracks.bad; race_records.dat and race_records_cars2.dat never touched. */
#include "game/sys/race_records_tracks.h"
#include "game/sys/race_records.h"
#include "game/sys/race_records_io.h"
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

enum { HEADER = 20, NAMES = 4 + 33 + 33 + 9, PRINTS = 64, TABLE_ENTRY = NAMES + PRINTS + RACE_RECORDS_TABLE_BYTES, LAP_ENTRY = NAMES + PRINTS + RACE_RECORDS_ROW_BYTES };

/* the prints the registries give: "hill-climb" and "mod-car" are pieces with one (a test sets them), the rest have none */
static uint8_t hill_print[32], mod_print[32];
static void track_print_of(const char *id, uint8_t out[32]) { if (strcmp(id, "hill-climb") == 0) memcpy(out, hill_print, 32); }
static void car_print_of(const char *id, uint8_t out[32]) { if (strcmp(id, "mod-car") == 0) memcpy(out, mod_print, 32); }

static void test_empty(void)
{
    race_records_tracks_reset();
    size_t size = 0;
    uint8_t *img = race_records_tracks_image(&size);
    CHECK(img != NULL && size == HEADER && memcmp(img, "TNFSRRT2", 8) == 0);
    CHECK_EQ_INT(race_records_tracks_parse(img, size), 1);
    free(img);
}

static void test_a_table_per_track_and_car(void)
{
    race_records_tracks_reset();
    RecordTable *moon = race_records_tracks_make("lunar", 0, 0, 4, "nsx", NULL);
    RecordTable *moon_f40 = race_records_tracks_make("lunar", 0, 0, 4, "f40", "F40");
    RecordTable *added = race_records_tracks_make("hill-climb", 0, 0, 0, "nsx", NULL);
    CHECK(moon != NULL && moon_f40 != NULL && added != NULL && moon != moon_f40 && moon != added);
    CHECK(race_records_tracks_find("lunar", 0, 0, 4, "nsx") == moon);
    CHECK(race_records_tracks_find("lunar", 0, 0, 4, "f40") == moon_f40);
    CHECK(race_records_tracks_find("lunar", 1, 0, 4, "nsx") == NULL);
    CHECK(race_records_tracks_find("lunar", 0, 0, 5, "nsx") == NULL);
    CHECK(race_records_tracks_find("rusty", 0, 0, 4, "nsx") == NULL);
    CHECK(race_records_tracks_make("lunar", 0, 0, 4, "nsx", NULL) == moon);   /* made once */
    CHECK(!race_records_row_set(&moon->totals[0]));                            /* made empty */
    CHECK(strcmp(race_records_tracks_short_name("lunar", 0, 0, 4, "f40"), "F40") == 0);
    CHECK(strcmp(race_records_tracks_short_name("lunar", 0, 0, 4, "nsx"), "") == 0);
    CHECK(race_records_tracks_make("Lunar", 0, 0, 4, "nsx", NULL) == NULL);   /* not an id */
    CHECK(race_records_tracks_make("lunar", 0, 0, 4, "", NULL) == NULL);
    CHECK(race_records_tracks_make(NULL, 0, 0, 4, "nsx", NULL) == NULL);
    CHECK(race_records_tracks_make("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 0, 0, 4, "nsx", NULL) == NULL);   /* 33 */
    CHECK(race_records_tracks_make("lunar", 8, 0, 4, "nsx", NULL) == NULL);    /* key out of range */
    CHECK(race_records_tracks_make("lunar", 0, 2, 4, "nsx", NULL) == NULL);
    CHECK(race_records_tracks_make("lunar", 0, 0, 100, "nsx", NULL) == NULL);
    RecordRow *lap = race_records_tracks_lap_make("lunar", 1, 0, "nsx", NULL);
    CHECK(lap != NULL && race_records_tracks_lap_find("lunar", 1, 0, "nsx") == lap);
    CHECK(race_records_tracks_lap_find("lunar", 1, 0, "f40") == NULL);
}

static uint8_t *three_tables_and_a_lap(size_t *size)
{
    race_records_tracks_reset();
    set_time(&race_records_tracks_make("lunar", 0, 0, 4, "nsx", NULL)->totals[0], 1234);
    set_time(&race_records_tracks_make("lunar", 0, 0, 4, "f40", "F40")->totals[1], 5678);
    set_time(&race_records_tracks_make("hill-climb", 2, 1, 0, "nsx", NULL)->segment_time[1], 99);
    set_time(race_records_tracks_lap_make("lunar", 1, 0, "nsx", NULL), 4321);
    return race_records_tracks_image(size);
}

static void test_there_and_back(void)
{
    size_t size = 0;
    uint8_t *img = three_tables_and_a_lap(&size);
    CHECK(img != NULL && size == HEADER + 3 * TABLE_ENTRY + LAP_ENTRY);
    race_records_tracks_reset();
    CHECK(race_records_tracks_find("lunar", 0, 0, 4, "nsx") == NULL);
    CHECK_EQ_INT(race_records_tracks_parse(img, size), 1);
    CHECK_EQ_INT(time_of(&race_records_tracks_find("lunar", 0, 0, 4, "nsx")->totals[0]), 1234);
    CHECK_EQ_INT(time_of(&race_records_tracks_find("lunar", 0, 0, 4, "f40")->totals[1]), 5678);
    CHECK_EQ_INT(time_of(&race_records_tracks_find("hill-climb", 2, 1, 0, "nsx")->segment_time[1]), 99);
    CHECK_EQ_INT(time_of(race_records_tracks_lap_find("lunar", 1, 0, "nsx")), 4321);
    CHECK(strcmp(race_records_tracks_short_name("lunar", 0, 0, 4, "f40"), "F40") == 0);
    size_t again_size = 0;
    uint8_t *again = race_records_tracks_image(&again_size);
    CHECK(again != NULL && again_size == size && memcmp(again, img, size) == 0);
    free(again);
    free(img);
}

static int parse_edited(void (*edit)(uint8_t *img, size_t *size))
{
    size_t size = 0;
    uint8_t *img = three_tables_and_a_lap(&size);
    uint8_t *big = malloc(size + 1);
    memcpy(big, img, size);
    free(img);
    edit(big, &size);
    const int ok = race_records_tracks_parse(big, size);
    free(big);
    return ok;
}

/* the tables sorted by key then track then car: (0 0 4 lunar f40), (0 0 4 lunar nsx), (2 1 0 hill-climb nsx) */
static void bad_magic(uint8_t *img, size_t *size) { (void)size, img[7] = '3'; }
static void bad_flags(uint8_t *img, size_t *size) { (void)size, img[8] = 1; }
static void one_short(uint8_t *img, size_t *size) { (void)img, (*size)--; }
static void one_more(uint8_t *img, size_t *size) { img[*size] = 0, (*size)++; }
static void swapped(uint8_t *img, size_t *size)
{
    (void)size;
    uint8_t *t = malloc(TABLE_ENTRY);
    memcpy(t, img + HEADER, TABLE_ENTRY);
    memcpy(img + HEADER, img + HEADER + TABLE_ENTRY, TABLE_ENTRY);
    memcpy(img + HEADER + TABLE_ENTRY, t, TABLE_ENTRY);
    free(t);
}
static void twice(uint8_t *img, size_t *size) { (void)size, memcpy(img + HEADER + TABLE_ENTRY, img + HEADER, TABLE_ENTRY); }
static void layout_8(uint8_t *img, size_t *size) { (void)size, img[HEADER] = 8; }
static void pad_set(uint8_t *img, size_t *size) { (void)size, img[HEADER + 3] = 2; }
static void track_no_nul(uint8_t *img, size_t *size) { (void)size, memset(img + HEADER + 4, 'a', 33); }
static void car_capital(uint8_t *img, size_t *size) { (void)size, img[HEADER + 4 + 33] = 'F'; }
static void short_no_nul(uint8_t *img, size_t *size) { (void)size, memset(img + HEADER + 4 + 66, 'X', 9); }
static void lap_with_laps(uint8_t *img, size_t *size) { img[*size - LAP_ENTRY + 2] = 4; }

static void test_parse_refuses(void)
{
    void (*const edits[])(uint8_t *, size_t *) = { bad_magic, bad_flags, one_short, one_more, swapped, twice, layout_8, pad_set,
                                                    track_no_nul, car_capital, short_no_nul, lap_with_laps };
    for (size_t i = 0; i < sizeof edits / sizeof edits[0]; i++) {
        CHECK_EQ_INT(parse_edited(edits[i]), 0);
        CHECK(race_records_tracks_find("lunar", 0, 0, 4, "nsx") == NULL);   /* refused: the store empty */
    }
}

static void test_the_file(void)
{
    files_clear();
    race_records_tracks_reset();
    CHECK_EQ_INT(race_records_tracks_load(), RACE_RECORDS_NO_FILE);
    set_time(&race_records_tracks_make("lunar", 0, 0, 4, "nsx", NULL)->totals[0], 777);
    CHECK(race_records_tracks_save());
    CHECK(platform_user_file_exists("race_records_tracks2.dat") && !platform_user_file_exists("race_records_tracks.dat"));
    CHECK(!platform_user_file_exists("race_records.dat") && !platform_user_file_exists("race_records_cars2.dat"));   /* never theirs */
    race_records_tracks_reset();
    CHECK_EQ_INT(race_records_tracks_load(), RACE_RECORDS_LOADED);
    CHECK_EQ_INT(time_of(&race_records_tracks_find("lunar", 0, 0, 4, "nsx")->totals[0]), 777);
    size_t size = 0;
    uint8_t *img = three_tables_and_a_lap(&size);
    CHECK(platform_write_user_file("race_records_tracks2.dat", img, size - 5));   /* truncated */
    free(img);
    CHECK_EQ_INT(race_records_tracks_load(), RACE_RECORDS_DAMAGED);
    CHECK(platform_user_file_exists("race_records_tracks.bad"));
    CHECK(race_records_tracks_find("lunar", 0, 0, 4, "nsx") == NULL);
    CHECK(race_records_tracks_save());   /* the copy kept: saving goes on */
}

/* the track the race running keeps its records under: set at its start, NULL for a track race_records.dat has */
static void test_the_race_track(void)
{
    race_records_tracks_race_set(NULL);
    CHECK(race_records_tracks_race() == NULL);
    race_records_tracks_race_set("lunar");
    CHECK(race_records_tracks_race() != NULL && strcmp(race_records_tracks_race(), "lunar") == 0);
    race_records_tracks_race_set("Not an id");
    CHECK(race_records_tracks_race() == NULL);
    race_records_tracks_race_set(NULL);
}


static void set_prints(uint8_t hill, uint8_t mod)
{
    memset(hill_print, hill, 32), memset(mod_print, mod, 32);
}

/* a piece that changed is another track or car: its records are not the old ones, and are there again when the piece is */
static void test_a_changed_piece_has_its_own_records(void)
{
    race_records_tracks_reset();
    race_records_tracks_prints_set(track_print_of, car_print_of);
    set_prints(1, 2);
    set_time(&race_records_tracks_make("hill-climb", 0, 0, 0, "mod-car", NULL)->totals[0], 111);
    set_time(race_records_tracks_lap_make("hill-climb", 0, 0, "mod-car", NULL), 11);
    CHECK_EQ_INT(time_of(&race_records_tracks_find("hill-climb", 0, 0, 0, "mod-car")->totals[0]), 111);
    set_prints(9, 2);   /* the track changed */
    CHECK(race_records_tracks_find("hill-climb", 0, 0, 0, "mod-car") == NULL && race_records_tracks_lap_find("hill-climb", 0, 0, "mod-car") == NULL);
    set_time(&race_records_tracks_make("hill-climb", 0, 0, 0, "mod-car", NULL)->totals[0], 222);
    set_prints(1, 9);   /* the car changed */
    CHECK(race_records_tracks_find("hill-climb", 0, 0, 0, "mod-car") == NULL);
    set_prints(1, 2);   /* back as it was: the first records, the other ones kept */
    CHECK_EQ_INT(time_of(&race_records_tracks_find("hill-climb", 0, 0, 0, "mod-car")->totals[0]), 111);
    CHECK_EQ_INT(time_of(race_records_tracks_lap_find("hill-climb", 0, 0, "mod-car")), 11);
    set_prints(9, 2);
    CHECK_EQ_INT(time_of(&race_records_tracks_find("hill-climb", 0, 0, 0, "mod-car")->totals[0]), 222);
    size_t size = 0;   /* all of them on disk, and read back */
    uint8_t *img = race_records_tracks_image(&size);
    CHECK(img != NULL && size == HEADER + 2 * TABLE_ENTRY + LAP_ENTRY);
    race_records_tracks_reset();
    CHECK_EQ_INT(race_records_tracks_parse(img, size), 1);
    set_prints(1, 2);
    CHECK_EQ_INT(time_of(&race_records_tracks_find("hill-climb", 0, 0, 0, "mod-car")->totals[0]), 111);
    free(img);
    race_records_tracks_prints_set(NULL, NULL);
}

/* a track or car with no print (the stock ones, Lunar) keeps its records as ever, whatever else changes */
static void test_no_print_is_one_key(void)
{
    race_records_tracks_reset();
    race_records_tracks_prints_set(track_print_of, car_print_of);
    set_prints(1, 2);
    set_time(&race_records_tracks_make("lunar", 0, 0, 4, "nsx", NULL)->totals[0], 5);
    set_prints(7, 8);
    CHECK_EQ_INT(time_of(&race_records_tracks_find("lunar", 0, 0, 4, "nsx")->totals[0]), 5);
    race_records_tracks_prints_set(NULL, NULL);
    CHECK_EQ_INT(time_of(&race_records_tracks_find("lunar", 0, 0, 4, "nsx")->totals[0]), 5);
}

/* the cars listed for a track are those whose records are under the pieces as they are now */
static void test_the_cars_of_a_track_follow_the_prints(void)
{
    race_records_tracks_reset();
    race_records_tracks_prints_set(track_print_of, car_print_of);
    set_prints(1, 2);
    race_records_tracks_make("hill-climb", 0, 0, 0, "mod-car", NULL);
    race_records_tracks_make("hill-climb", 0, 0, 0, "nsx", NULL);
    const char *ids[4];
    CHECK_EQ_INT(race_records_tracks_cars("hill-climb", ids, 4), 2);
    set_prints(1, 9);
    CHECK_EQ_INT(race_records_tracks_cars("hill-climb", ids, 4), 1);
    CHECK(strcmp(ids[0], "nsx") == 0);
    set_prints(5, 2);
    CHECK_EQ_INT(race_records_tracks_cars("hill-climb", ids, 4), 0);
    race_records_tracks_prints_set(NULL, NULL);
}

/* a v1 file (no prints) as the ones before this: its image from a v2 one with the prints cut out */
static uint8_t *as_v1(const uint8_t *v2, size_t size, size_t *out_size)
{
    const uint32_t n_tables = race_records_get32(v2 + 12), n_laps = race_records_get32(v2 + 16);
    uint8_t *out = malloc(size), *at = out;
    memcpy(at, v2, HEADER), memcpy(at, "TNFSRRT1", 8), at += HEADER;
    const uint8_t *from = v2 + HEADER;
    for (uint32_t i = 0; i < n_tables + n_laps; i++) {
        const size_t body = i < n_tables ? RACE_RECORDS_TABLE_BYTES : RACE_RECORDS_ROW_BYTES;
        memcpy(at, from, NAMES), at[3] = 0, memcpy(at + NAMES, from + NAMES + PRINTS, body);
        at += NAMES + body, from += NAMES + PRINTS + body;
    }
    *out_size = (size_t)(at - out);
    return out;
}

/* the records an older version wrote are the ones of the pieces as they are when this one first looks, and then theirs */
static void test_the_old_file_is_adopted_once(void)
{
    files_clear();
    race_records_tracks_reset();
    race_records_tracks_prints_set(NULL, NULL);
    size_t size = 0, old_size = 0;
    uint8_t *img = three_tables_and_a_lap(&size), *old = as_v1(img, size, &old_size);
    free(img);
    CHECK(platform_write_user_file("race_records_tracks.dat", old, old_size));
    race_records_tracks_prints_set(track_print_of, car_print_of);
    set_prints(1, 2);
    CHECK_EQ_INT(race_records_tracks_load(), RACE_RECORDS_LOADED);
    CHECK_EQ_INT(time_of(&race_records_tracks_find("lunar", 0, 0, 4, "nsx")->totals[0]), 1234);   /* no print: as ever */
    CHECK_EQ_INT(time_of(&race_records_tracks_find("hill-climb", 2, 1, 0, "nsx")->segment_time[1]), 99);   /* the track's print now */
    set_prints(9, 2);
    CHECK(race_records_tracks_find("hill-climb", 2, 1, 0, "nsx") == NULL);   /* not adopted again: it is the old track's */
    set_prints(1, 2);
    CHECK_EQ_INT(time_of(&race_records_tracks_find("hill-climb", 2, 1, 0, "nsx")->segment_time[1]), 99);
    CHECK(race_records_tracks_save());
    CHECK(platform_user_file_exists("race_records_tracks2.dat"));
    size_t kept = 0;
    uint8_t *v1 = platform_read_user_file("race_records_tracks.dat", &kept);
    CHECK(v1 != NULL && kept == old_size && memcmp(v1, old, old_size) == 0);   /* the old file never touched */
    platform_free_file(v1);
    race_records_tracks_reset();   /* the new one is what is read now, the old ignored */
    CHECK(platform_write_user_file("race_records_tracks.dat", old, 8));
    CHECK_EQ_INT(race_records_tracks_load(), RACE_RECORDS_LOADED);
    CHECK_EQ_INT(time_of(&race_records_tracks_find("hill-climb", 2, 1, 0, "nsx")->segment_time[1]), 99);
    free(old);
    race_records_tracks_prints_set(NULL, NULL);
}

/* a track that is not in the registry when the old file is read keeps its old records for the piece that comes back */
static void test_the_old_records_wait_for_the_piece(void)
{
    files_clear();
    race_records_tracks_reset();
    size_t size = 0, old_size = 0;
    uint8_t *img = three_tables_and_a_lap(&size), *old = as_v1(img, size, &old_size);
    free(img);
    CHECK(platform_write_user_file("race_records_tracks.dat", old, old_size));
    free(old);
    race_records_tracks_prints_set(track_print_of, car_print_of);
    set_prints(0, 0);   /* the piece is not there: no print */
    CHECK_EQ_INT(race_records_tracks_load(), RACE_RECORDS_LOADED);
    set_prints(4, 0);   /* it is, and its first look adopts them */
    const char *ids[4];
    CHECK_EQ_INT(race_records_tracks_cars("hill-climb", ids, 4), 1);
    CHECK_EQ_INT(time_of(&race_records_tracks_find("hill-climb", 2, 1, 0, "nsx")->segment_time[1]), 99);
    race_records_tracks_prints_set(NULL, NULL);
}

/* a damaged old file is set aside as the file always was, and the store starts empty */
static void test_a_damaged_old_file(void)
{
    files_clear();
    race_records_tracks_reset();
    race_records_tracks_prints_set(NULL, NULL);
    CHECK(platform_write_user_file("race_records_tracks.dat", (const uint8_t *)"TNFSRRT1 junk", 13));
    CHECK_EQ_INT(race_records_tracks_load(), RACE_RECORDS_DAMAGED);
    CHECK(platform_user_file_exists("race_records_tracks.bad"));
}

int main(void)
{
    test_empty();
    test_a_table_per_track_and_car();
    test_there_and_back();
    test_parse_refuses();
    test_the_file();
    test_the_race_track();
    test_a_changed_piece_has_its_own_records();
    test_no_print_is_one_key();
    test_the_cars_of_a_track_follow_the_prints();
    test_the_old_file_is_adopted_once();
    test_the_old_records_wait_for_the_piece();
    test_a_damaged_old_file();
    race_records_tracks_reset();
    files_clear();
    return TEST_REPORT();
}
