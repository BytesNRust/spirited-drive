/* port (dep55): race_records.h's store. Tables keyed by track, layout (all eight), cheats and laps (0 for a stage), made empty and found
 * again; the best lap keyed without the laps; the file round-trips every field, a damaged one is kept as race_records.bad (.bad2 .. when
 * taken) and the store starts empty; with no copy kept saving is held; a loaded name ends in its field. */
#include "game/sys/race_records.h"
#include "game/sys/extras_marks.h"
#include "game/platform/platform.h"
#include "test.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum { FILES = 4 };
static struct {
    char name[32];
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

static int fail_writes;   /* the disk refuses every write */

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    if (fail_writes) return false;
    const int i = slot_of(name, 1);
    if (i < 0) return false;
    free(files[i].data);
    files[i].data = malloc(size ? size : 1);
    memcpy(files[i].data, data, size);
    files[i].size = size;
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

static void files_clear(void)
{
    for (int i = 0; i < FILES; i++) free(files[i].data), files[i].data = NULL;
}

static int32_t row_time(const RecordRow *r)
{
    int32_t t;
    memcpy(&t, r->row + RECORD_ROW_TIME, 4);
    return t;
}

static void row_fill(RecordRow *r, int seed)
{
    for (int i = 0; i < RACE_RECORDS_ROW; i++) r->row[i] = (uint8_t)(seed * 31 + i);
    r->row[RECORD_ROW_NAME_MAX - 1] = 0;   /* a name ends in its field, as the game writes it */
    r->marks.cheats = (uint8_t)(seed & 15), r->marks.weather = (uint8_t)(RECORD_MARK_KNOWN | (seed & 7)),
    r->marks.time = (uint8_t)(RECORD_MARK_KNOWN | 2), r->marks.difficulty = (uint8_t)(RECORD_MARK_KNOWN | 0x21);
}

static void test_tables_by_key(void)
{
    race_records_reset();
    CHECK(race_records_find(5, LAYOUT_NORMAL, 0, 6) == NULL);
    RecordTable *t = race_records_make(5, LAYOUT_NORMAL, 0, 6);
    CHECK(t != NULL);
    for (int i = 0; i < RACE_RECORDS_ROWS; i++) CHECK(!race_records_row_set(&t->totals[i]));
    CHECK_EQ_INT(row_time(&t->totals[0]), INT32_MAX);   /* empty: any race beats it */
    CHECK(race_records_find(5, LAYOUT_NORMAL, 0, 6) == t);
    CHECK(race_records_make(5, LAYOUT_NORMAL, 0, 6) == t);
    /* each part of the key is its own table: all eight layouts, both cheats, every lap count */
    int made = 0;
    for (int layout = 0; layout < RACE_RECORDS_LAYOUTS; layout++)
        for (int cheats = 0; cheats < 2; cheats++) made += race_records_make(5, layout, cheats, 7) != NULL;
    CHECK_EQ_INT(made, 16);
    CHECK(race_records_find(5, LAYOUT_MIRROR | LAYOUT_REVERSE | LAYOUT_RALLY, 1, 7) != race_records_find(5, LAYOUT_RALLY, 1, 7));
    CHECK(race_records_find(5, LAYOUT_NORMAL, 0, 7) != t);
    /* a stage has laps 0 only, a circuit 1..99 */
    CHECK(race_records_make(1, LAYOUT_NORMAL, 0, 0) != NULL);
    CHECK(race_records_make(1, LAYOUT_NORMAL, 0, 4) == NULL);
    CHECK(race_records_make(5, LAYOUT_NORMAL, 0, 0) == NULL);
    CHECK(race_records_make(5, LAYOUT_NORMAL, 0, 100) == NULL);
    CHECK(race_records_make(9, LAYOUT_NORMAL, 0, 6) == NULL);
    CHECK(race_records_make(5, 8, 0, 6) == NULL);
    CHECK(race_records_make(5, LAYOUT_NORMAL, 2, 6) == NULL);
    (void)race_records_make(5, LAYOUT_NORMAL, 0, 99);
    (void)race_records_make(5, LAYOUT_NORMAL, 0, 1);
    int laps[8];
    CHECK_EQ_INT(race_records_laps(5, LAYOUT_NORMAL, 0, laps, 8), 4);
    CHECK_EQ_INT(laps[0], 1);
    CHECK_EQ_INT(laps[1], 6);
    CHECK_EQ_INT(laps[2], 7);
    CHECK_EQ_INT(laps[3], 99);
    CHECK_EQ_INT(race_records_laps(5, LAYOUT_NORMAL, 0, laps, 2), 2);
}

static void test_best_lap_without_laps(void)
{
    race_records_reset();
    RecordRow *lap = race_records_lap_make(4, LAYOUT_MIRROR, 1);
    CHECK(lap != NULL);
    CHECK(!race_records_row_set(lap));
    CHECK(race_records_lap_find(4, LAYOUT_MIRROR, 1) == lap);
    CHECK(race_records_lap_find(4, LAYOUT_MIRROR, 0) == NULL);
    CHECK(race_records_lap_find(4, LAYOUT_NORMAL, 1) == NULL);
    (void)race_records_make(4, LAYOUT_MIRROR, 1, 2);
    int laps[4];
    CHECK_EQ_INT(race_records_laps(4, LAYOUT_MIRROR, 1, laps, 4), 1);   /* a lap is not a table */
}

static void test_the_file_round_trips(void)
{
    files_clear();
    race_records_reset();
    RecordTable *t = race_records_make(8, LAYOUT_REVERSE | LAYOUT_RALLY, 1, 33);
    for (int i = 0; i < RACE_RECORDS_ROWS; i++) row_fill(&t->totals[i], i + 1);
    row_fill(&t->top, 40);
    RecordTable *s = race_records_make(0, LAYOUT_MIRROR, 0, 0);
    for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) {
        row_fill(&s->segment_time[k], 50 + k), row_fill(&s->segment_speed[k], 60 + k);
        s->stage_speeds[k] = 1000 + k, s->stage_totals[k] = -2000 - k;
    }
    row_fill(race_records_lap_make(8, LAYOUT_REVERSE | LAYOUT_RALLY, 1), 70);
    race_records_set_tournament(EXTRAS_MARK_NITRO | EXTRAS_MARK_WARRIOR);   /* the Warrior is not a tournament's cheat */
    const RecordTable t_was = *t, s_was = *s;
    const RecordRow lap_was = *race_records_lap_find(8, LAYOUT_REVERSE | LAYOUT_RALLY, 1);
    CHECK(race_records_save());
    race_records_reset();
    CHECK(race_records_find(8, LAYOUT_REVERSE | LAYOUT_RALLY, 1, 33) == NULL);
    CHECK_EQ_INT(race_records_load(), RACE_RECORDS_LOADED);
    t = race_records_find(8, LAYOUT_REVERSE | LAYOUT_RALLY, 1, 33);
    s = race_records_find(0, LAYOUT_MIRROR, 0, 0);
    CHECK(t != NULL && s != NULL);
    if (t == NULL || s == NULL) return;
    CHECK(memcmp(t, &t_was, sizeof *t) == 0);
    CHECK(memcmp(s, &s_was, sizeof *s) == 0);
    CHECK(memcmp(race_records_lap_find(8, LAYOUT_REVERSE | LAYOUT_RALLY, 1), &lap_was, sizeof lap_was) == 0);
    CHECK_EQ_INT(race_records_tournament(), EXTRAS_MARK_NITRO);
}

static void test_no_file_and_a_damaged_one(void)
{
    files_clear();
    CHECK_EQ_INT(race_records_load(), RACE_RECORDS_NO_FILE);
    CHECK(race_records_make(5, LAYOUT_NORMAL, 0, 6) != NULL);
    CHECK(race_records_save());
    const int i = slot_of("race_records.dat", 0);
    CHECK(i >= 0);
    if (i < 0) return;
    CHECK_EQ_INT(race_records_load(), RACE_RECORDS_LOADED);
    const size_t cut = files[i].size - 3;
    files[i].size = cut;   /* cut short */
    CHECK_EQ_INT(race_records_load(), RACE_RECORDS_DAMAGED);
    CHECK(race_records_find(5, LAYOUT_NORMAL, 0, 6) == NULL);
    const int bad = slot_of("race_records.bad", 0);
    CHECK(bad >= 0);
    if (bad >= 0) CHECK_EQ_INT((int)files[bad].size, (int)cut);
    memcpy(files[i].data, "NOTOURS!", 8);
    CHECK_EQ_INT(race_records_load(), RACE_RECORDS_DAMAGED);   /* a second damaged file: its own copy, the first kept */
    const int bad2 = slot_of("race_records.bad2", 0);
    CHECK(bad2 >= 0 && memcmp(files[bad2].data, "NOTOURS!", 8) == 0);
    CHECK(bad >= 0 && files[bad].size == cut && memcmp(files[bad].data, "TNFSRRD1", 8) == 0);
    CHECK(race_records_save());   /* a copy kept: saving goes on */
}

/* a damaged file no copy of could be written: saving is held, so the file is not lost; on again when let go */
static void test_no_copy_holds_saving(void)
{
    files_clear();
    CHECK(platform_write_user_file("race_records.dat", (const uint8_t *)"TNFSRRD1junk", 12));
    fail_writes = 1;
    CHECK_EQ_INT(race_records_load(), RACE_RECORDS_DAMAGED);
    fail_writes = 0;
    CHECK(race_records_make(5, LAYOUT_NORMAL, 0, 6) != NULL);
    CHECK(!race_records_save());
    const int i = slot_of("race_records.dat", 0);
    CHECK(i >= 0 && files[i].size == 12);   /* untouched */
    race_records_hold(0);
    CHECK(race_records_save());
}

/* a name that runs past its field in the file ends inside it once loaded */
static void test_a_loaded_name_ends_in_its_field(void)
{
    files_clear();
    race_records_reset();
    RecordTable *t = race_records_make(5, LAYOUT_NORMAL, 0, 6);
    CHECK(t != NULL);
    if (t == NULL) return;
    memset(t->totals[0].row, 'A', RECORD_ROW_NAME_MAX);
    CHECK(race_records_save());
    CHECK_EQ_INT(race_records_load(), RACE_RECORDS_LOADED);
    t = race_records_find(5, LAYOUT_NORMAL, 0, 6);
    CHECK(t != NULL && strlen((const char *)t->totals[0].row) == RECORD_ROW_NAME_MAX - 1);
}

int main(void)
{
    test_tables_by_key();
    test_best_lap_without_laps();
    test_the_file_round_trips();
    test_no_file_and_a_damaged_one();
    test_no_copy_holds_saving();
    test_a_loaded_name_ends_in_its_field();
    files_clear();
    race_records_reset();
    return TEST_REPORT();
}
