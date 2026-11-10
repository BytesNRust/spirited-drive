/* port (free records): the per-lap-count tables of a free race's ten best totals (free_records.h): sparse by (track, variant, cheats,
 * laps), made from a seed, kept in free_records.dat, and a damaged file is no file. */
#include "tests/exe_tables_install.h"
#include "game/sys/free_records.h"
#include "game/sys/free_laps.h"
#include "game/platform/platform.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static char stored_name[32];
static uint8_t stored[0x80000];
static size_t stored_size;
static int stored_present, saves;

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    if (!stored_present || strcmp(name, stored_name) != 0) return NULL;
    uint8_t *copy = malloc(stored_size);
    memcpy(copy, stored, stored_size);
    *size = stored_size;
    return copy;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    if (size > sizeof stored) return false;
    snprintf(stored_name, sizeof stored_name, "%s", name);
    memcpy(stored, data, size);
    stored_size = size;
    stored_present = 1;
    saves++;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }
uint8_t *platform_read_file(const char *path, size_t *size) { return (void)path, (void)size, NULL; }   /* no CD: the built-in defaults */

static uint8_t seed[FREE_RECORDS_TABLE];

static void fresh(void)
{
    stored_present = 0;
    saves = 0;
    free_records_reset();
    for (int i = 0; i < FREE_RECORDS_TABLE; i++) seed[i] = (uint8_t)(i * 7 + 1);
}

static void test_a_table_is_made_from_the_seed_and_found_again(void)
{
    fresh();
    CHECK(free_records_find(5, RECORDS_NORMAL, 0, 7) == NULL);
    FreeTable *t = free_records_make(5, RECORDS_NORMAL, 0, 7, seed);
    CHECK(t != NULL);
    CHECK(memcmp(t->rows, seed, sizeof seed) == 0);
    for (int i = 0; i < FREE_RECORDS_ROWS; i++) CHECK_EQ_INT(t->marks[i], 0);
    CHECK(free_records_find(5, RECORDS_NORMAL, 0, 7) == t);
    CHECK(free_records_make(5, RECORDS_NORMAL, 0, 7, seed) == t);   /* made once */
    CHECK(free_records_find(5, RECORDS_NORMAL, 0, 8) == NULL);      /* the key is the whole of it */
    CHECK(free_records_find(5, RECORDS_NORMAL, 1, 7) == NULL);
    CHECK(free_records_find(5, RECORDS_MIRROR, 0, 7) == NULL);
    CHECK(free_records_find(6, RECORDS_NORMAL, 0, 7) == NULL);
    CHECK_EQ_INT(saves, 0);   /* making a table is not a record */
}

static void test_only_free_circuits_and_counts_have_tables(void)
{
    fresh();
    CHECK(free_records_make(2, RECORDS_NORMAL, 0, 7, seed) == NULL);    /* a stage has no laps */
    CHECK(free_records_make(9, RECORDS_NORMAL, 0, 7, seed) == NULL);
    CHECK(free_records_make(5, RECORDS_NORMAL, 0, 0, seed) == NULL);
    CHECK(free_records_make(5, RECORDS_NORMAL, 0, 100, seed) == NULL);
    CHECK(free_records_make(5, RECORDS_NORMAL, 2, 7, seed) == NULL);    /* cheats is 0 or 1 */
    CHECK(free_records_make(5, (RecordVariant)3, 0, 7, seed) == NULL);
}

static void test_the_laps_with_a_table_come_ascending(void)
{
    fresh();
    free_records_make(4, RECORDS_NORMAL, 0, 20, seed);
    free_records_make(4, RECORDS_NORMAL, 0, 3, seed);
    free_records_make(4, RECORDS_NORMAL, 0, 99, seed);
    free_records_make(4, RECORDS_NORMAL, 1, 5, seed);
    free_records_make(8, RECORDS_NORMAL, 0, 1, seed);
    int laps[8];
    CHECK_EQ_INT(free_records_laps(4, RECORDS_NORMAL, 0, laps, 8), 3);
    CHECK_EQ_INT(laps[0], 3);
    CHECK_EQ_INT(laps[1], 20);
    CHECK_EQ_INT(laps[2], 99);
    CHECK_EQ_INT(free_records_laps(4, RECORDS_NORMAL, 1, laps, 8), 1);
    CHECK_EQ_INT(laps[0], 5);
    CHECK_EQ_INT(free_records_laps(4, RECORDS_NORMAL, 0, laps, 2), 2);   /* at most `max` */
    CHECK_EQ_INT(free_records_laps(7, RECORDS_NORMAL, 0, laps, 8), 0);
}

static void test_saved_tables_come_back(void)
{
    fresh();
    FreeTable *a = free_records_make(5, RECORDS_MIRROR, 1, 7, seed);
    a->rows[3] = 0xAA, a->marks[2] = 5;
    FreeTable *b = free_records_make(3, RECORDS_NORMAL, 0, 99, seed);
    b->rows[FREE_RECORDS_TABLE - 1] = 0x55;
    CHECK(free_records_save());
    CHECK_EQ_INT(saves, 1);
    free_records_reset();
    CHECK(free_records_find(5, RECORDS_MIRROR, 1, 7) == NULL);   /* the store is empty until loaded */
    CHECK(free_records_load());
    a = free_records_find(5, RECORDS_MIRROR, 1, 7);
    b = free_records_find(3, RECORDS_NORMAL, 0, 99);
    CHECK(a != NULL && b != NULL);
    if (a != NULL) CHECK(a->rows[3] == 0xAA && a->marks[2] == 5);
    if (b != NULL) CHECK(b->rows[FREE_RECORDS_TABLE - 1] == 0x55);
}

static void test_a_damaged_file_is_no_file(void)
{
    fresh();
    free_records_make(5, RECORDS_NORMAL, 0, 7, seed);
    CHECK(free_records_save());
    const size_t good = stored_size;
    uint8_t copy[sizeof stored];
    memcpy(copy, stored, good);
    for (int damage = 0; damage < 4; damage++) {
        memcpy(stored, copy, good);
        stored_size = good;
        if (damage == 0) stored[0] = 'X';                       /* the magic */
        if (damage == 1) stored_size = good - 1;                /* truncated */
        if (damage == 2) stored[8] = 9;                         /* a count the file cannot hold */
        if (damage == 3) stored[12] = 12;                       /* a track that is not a circuit */
        free_records_reset();
        CHECK(!free_records_load());
        CHECK(free_records_find(5, RECORDS_NORMAL, 0, 7) == NULL);
    }
    memcpy(stored, copy, good);
    stored_size = good;
    free_records_reset();
    CHECK(free_records_load());
    CHECK(free_records_find(5, RECORDS_NORMAL, 0, 7) != NULL);
}

static void test_no_file_is_an_empty_store(void)
{
    fresh();
    CHECK(!free_records_load());
    int laps[4];
    CHECK_EQ_INT(free_records_laps(5, RECORDS_NORMAL, 0, laps, 4), 0);
}

/* what a new table is seeded with: the exe's table of the lap count nearest, its times scaled by the laps asked over the laps it is for */
static void test_a_seed_scales_the_nearest_exe_tables_times(void)
{
    fresh();
    uint8_t rows[FREE_RECORDS_TABLE];
    free_records_seed(5, 3, rows);       /* track 5 is a short one: 2, 6, 12 laps: 3 is near 2 */
    uint8_t exe[FREE_RECORDS_TABLE];
    CHECK(free_records_exe_rows(5, 0, exe));
    for (int i = 0; i < FREE_RECORDS_ROWS; i++) {
        int32_t was, now;
        memcpy(&was, exe + 0x27 * i + 0x1b, 4);
        memcpy(&now, rows + 0x27 * i + 0x1b, 4);
        CHECK_EQ_INT(now, (int32_t)((int64_t)was * 3 / 2));
        CHECK(memcmp(exe + 0x27 * i, rows + 0x27 * i, 0x1b) == 0);   /* the name and the rest as they were */
    }
    free_records_seed(5, 12, rows);      /* the exe's own count scales by one */
    CHECK(free_records_exe_rows(5, 2, exe));
    CHECK(memcmp(exe, rows, sizeof rows) == 0);
    free_records_seed(3, 10, rows);      /* track 3 is a long one: 4, 8, 16: 10 is nearer 8 */
    CHECK(free_records_exe_rows(3, 1, exe));
    int32_t was, now;
    memcpy(&was, exe + 0x1b, 4);
    memcpy(&now, rows + 0x1b, 4);
    CHECK_EQ_INT(now, (int32_t)((int64_t)was * 10 / 8));
}

int main(void)
{
    exe_tables_for_test();
    test_a_table_is_made_from_the_seed_and_found_again();
    test_only_free_circuits_and_counts_have_tables();
    test_the_laps_with_a_table_come_ascending();
    test_saved_tables_come_back();
    test_a_damaged_file_is_no_file();
    test_no_file_is_an_empty_store();
    test_a_seed_scales_the_nearest_exe_tables_times();
    return TEST_REPORT();
}
