/* dep59 1b: the records of added cars (game/sys/race_records_added.h), a store of its own beside race_records.dat: a table per key and car
 * id, so the original's ranking runs in each car's table; its file read back byte for byte, anything that is not exactly a good file
 * refused, a damaged one set aside as race_records_added.bad. */
#include "game/sys/race_records_added.h"
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

enum { TR1 = 3, HEADER = 16 };

static void test_empty(void)
{
    race_records_added_reset();
    size_t size = 0;
    uint8_t *img = race_records_added_image(&size);
    CHECK(img != NULL && size == HEADER && memcmp(img, "TNFSRRA1", 8) == 0);
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

enum { TABLE_ENTRY = 4 + 33 + 9 + RACE_RECORDS_TABLE_BYTES };   /* key, id, short name, the table */
static void bad_magic(uint8_t *img, size_t *size) { (void)size, img[7] = '2'; }
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
static void lap_with_laps(uint8_t *img, size_t *size) { img[*size - (4 + 33 + 9 + RACE_RECORDS_ROW_BYTES) + 3] = 4; }

static void test_parse_refuses(void)
{
    void (*const BAD[])(uint8_t *, size_t *) = { bad_magic, one_short, one_more, swapped, twice, track_9, layout_8, id_no_nul, id_capital,
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
    CHECK(platform_write_user_file("race_records_added.dat", img, size - 5));   /* truncated */
    free(img);
    CHECK_EQ_INT(race_records_added_load(), RACE_RECORDS_DAMAGED);
    CHECK(platform_user_file_exists("race_records_added.bad"));
    CHECK(race_records_added_find(TR1, 0, 0, 4, "f40") == NULL);
    set_time(&race_records_added_make(TR1, 0, 0, 4, "f40", "F40")->totals[0], 777);
    CHECK(race_records_added_save());
    race_records_added_reset();
    CHECK_EQ_INT(race_records_added_load(), RACE_RECORDS_LOADED);
    CHECK_EQ_INT(time_of(&race_records_added_find(TR1, 0, 0, 4, "f40")->totals[0]), 777);
    CHECK(!platform_user_file_exists("race_records.dat"));   /* the original store's file is not this one's */
}

int main(void)
{
    test_empty();
    test_a_table_per_car();
    test_there_and_back();
    test_parse_refuses();
    test_a_damaged_file_is_set_aside();
    files_clear();
    race_records_added_reset();
    return TEST_REPORT();
}
