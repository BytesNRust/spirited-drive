/* The first run of Restomod's records (records_import.c): config.dat's records seed the normal tables without cheats; the earlier
 * builds' records.dat and free_records.dat are read into their tables with their rows' cheats, the CD's default rows left out; the old
 * files stay byte for byte as they were; one of them that is not ours is not read, and the seed still is. */
#include "tests/exe_tables_install.h"
#include "game/frontend/config_dat.h"
#include "game/frontend/records_import.h"
#include "game/frontend/records_route.h"
#include "game/sys/extras_marks.h"
#include "game/sys/race_records.h"
#include "test.h"

#include "game/platform/platform.h"

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
    uint8_t *copy = malloc(files[i].size);
    memcpy(copy, files[i].data, files[i].size);
    *size = files[i].size;
    return copy;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    const int i = slot_of(name, 1);
    if (i < 0) return false;
    free(files[i].data);
    files[i].data = malloc(size);
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
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path, *size = 0; return NULL; }   /* no CD: the built-in defaults */

static void files_clear(void)
{
    for (int i = 0; i < FILES; i++) free(files[i].data), files[i].data = NULL;
}

static void put(const char *name, const uint8_t *data, size_t size)
{
    (void)platform_write_user_file(name, data, size);
}

enum {
    FIRST_BLOCK = 0xb5, BLOCK = 0xa6b, ROW = RACE_RECORDS_ROW, CHOICE = 0x186, TOTALS_AT = 0xea, VARIANTS = 3, MARKS = 64,
    OLD_SIZE = 8 + 1 + RACE_RECORDS_TRACKS * ((2 * VARIANTS - 1) * BLOCK + VARIANTS * MARKS), FREE_ENTRY = 4 + CHOICE + RACE_RECORDS_ROWS,
    STAGE = 1, CIRCUIT = 3
};

static int32_t g32(const uint8_t *at)
{
    int32_t v;
    memcpy(&v, at, 4);
    return v;
}
static void p32(uint8_t *at, int32_t v) { memcpy(at, &v, 4); }

/* a row of the player's: a name, a set word, a time */
static void player_row(uint8_t *row, const char *name, int32_t time)
{
    memset(row, 0, ROW);
    snprintf((char *)row, RECORD_ROW_NAME_MAX, "%s", name);
    p32(row + RECORD_ROW_SET, 1);
    p32(row + RECORD_ROW_TIME, time);
}

static uint8_t defaults[RECORD_CHECK_IMAGE];

/* records.dat as the earlier builds wrote it, every block the CD's; `at` the offset of (track, variant, cheats)'s block and of its marks */
static size_t old_block(int track, int variant, int cheats)
{
    size_t at = 9 + (size_t)track * ((2 * VARIANTS - 1) * BLOCK + VARIANTS * MARKS);
    for (int v = 0; v < variant; v++) at += (size_t)(v == 0 ? 1 : 2) * BLOCK + MARKS;
    return at + (size_t)(variant == 0 ? 0 : cheats) * BLOCK;
}
static size_t old_marks(int track, int variant) { return old_block(track, variant, 1) + BLOCK; }

static uint8_t *old_records(void)
{
    uint8_t *d = calloc(1, OLD_SIZE);
    memcpy(d, "TNFSREC1", 8);
    d[8] = EXTRAS_MARK_NITRO;   /* the tournament's */
    for (int t = 0; t < RACE_RECORDS_TRACKS; t++)
        for (int v = 0; v < VARIANTS; v++)
            for (int c = v == 0; c < 2; c++) memcpy(d + old_block(t, v, c), defaults + FIRST_BLOCK + BLOCK * t, BLOCK);
    return d;
}

static void setup(void)
{
    ConfigDat c;
    config_dat_defaults(&c, 0);
    memcpy(defaults, c.image, sizeof defaults);
    files_clear();
    race_records_reset();
}

/* config.dat's records are the normal tables without cheats, each laps choice its count's, its rows as they are (the CD's too) */
static void test_config_dat_seeds_the_normal_tables(void)
{
    setup();
    static uint8_t image[RECORD_CHECK_IMAGE];
    memcpy(image, defaults, sizeof image);
    player_row(image + FIRST_BLOCK + BLOCK * CIRCUIT + TOTALS_AT + CHOICE * 2, "MINE", 4321);   /* choice 2: 16 laps */
    records_import(image);
    const RecordTable *t = race_records_find(CIRCUIT, LAYOUT_NORMAL, 0, 16);
    CHECK(t != NULL && strcmp((const char *)t->totals[0].row, "MINE") == 0);
    CHECK(t != NULL && g32(t->totals[0].row + RECORD_ROW_TIME) == 4321);
    CHECK(t != NULL && t->totals[0].marks.weather == 0);   /* no conditions known: "-" */
    t = race_records_find(CIRCUIT, LAYOUT_NORMAL, 0, 4);
    CHECK(t != NULL && memcmp(t->totals[1].row, image + FIRST_BLOCK + BLOCK * CIRCUIT + TOTALS_AT + ROW, ROW) == 0);
    CHECK(race_records_find(CIRCUIT, LAYOUT_NORMAL, 0, 8) != NULL);
    CHECK(race_records_find(STAGE, LAYOUT_NORMAL, 0, 0) != NULL);
    CHECK(race_records_lap_find(CIRCUIT, LAYOUT_NORMAL, 0) != NULL);
    CHECK(race_records_find(CIRCUIT, LAYOUT_MIRROR, 0, 4) == NULL);   /* nothing else without the old files */
    CHECK(slot_of("race_records.dat", 0) >= 0);   /* saved */
}

/* records.dat's rows of the player's go to their layout's and cheats' tables with their marks; the CD's rows stay out; the file is not
 * touched */
static void test_records_dat_is_imported(void)
{
    setup();
    uint8_t *old = old_records();
    uint8_t *mirror = old + old_block(CIRCUIT, 1, 0) + TOTALS_AT + CHOICE * 1;   /* mirror, no cheats, choice 1 (8 laps) */
    player_row(mirror + ROW * 4, "MIRR", 5000);
    uint8_t *rally_cheats = old + old_block(CIRCUIT, 2, 1) + TOTALS_AT;   /* rally with cheats, choice 0 (4 laps) */
    player_row(rally_cheats, "RALC", 2000);
    old[old_marks(CIRCUIT, 2) + records_slot_totals(CIRCUIT, 0, 0)] = EXTRAS_MARK_TURBO | EXTRAS_MARK_WARRIOR;
    uint8_t *stage_cheats = old + old_block(STAGE, 0, 1);   /* the stage's normal with cheats: segment 2's time */
    player_row(stage_cheats + ROW * 1, "SEGC", 900);
    old[old_marks(STAGE, 0) + SLOT_SEGMENT + 1] = EXTRAS_MARK_EXPLODE;
    put("records.dat", old, OLD_SIZE);
    records_import(defaults);

    const RecordTable *t = race_records_find(CIRCUIT, LAYOUT_MIRROR, 0, 8);
    CHECK(t != NULL);
    if (t != NULL) {
        CHECK(strcmp((const char *)t->totals[0].row, "MIRR") == 0);   /* the player's row first, the CD's around it gone */
        CHECK(!race_records_row_set(&t->totals[1]));
        CHECK_EQ_INT(t->totals[0].marks.cheats, 0);
    }
    t = race_records_find(CIRCUIT, LAYOUT_RALLY, 1, 4);
    CHECK(t != NULL && t->totals[0].marks.cheats == (EXTRAS_MARK_TURBO | EXTRAS_MARK_WARRIOR));
    t = race_records_find(STAGE, LAYOUT_NORMAL, 1, 0);
    CHECK(t != NULL && strcmp((const char *)t->segment_time[1].row, "SEGC") == 0 && t->segment_time[1].marks.cheats == EXTRAS_MARK_EXPLODE);
    CHECK(race_records_find(CIRCUIT, LAYOUT_MIRROR, 1, 4) == NULL);   /* only the CD's rows there: no table */
    CHECK(race_records_find(CIRCUIT, LAYOUT_RALLY, 0, 8) == NULL);
    CHECK_EQ_INT(race_records_tournament(), EXTRAS_MARK_NITRO);
    const int i = slot_of("records.dat", 0);
    CHECK(i >= 0 && files[i].size == OLD_SIZE && memcmp(files[i].data, old, OLD_SIZE) == 0);
    free(old);
}

/* free_records.dat's tables by lap count, their rows' cheats kept; its seed rows (the nearest choice's, scaled) left out */
static void test_free_records_dat_is_imported(void)
{
    setup();
    const size_t size = 12 + FREE_ENTRY;
    uint8_t *d = calloc(1, size);
    memcpy(d, "TNFSFRE1", 8);
    d[8] = 1;
    uint8_t *e = d + 12;
    e[0] = CIRCUIT, e[1] = 1, e[2] = 1, e[3] = 7;   /* mirror, cheats, 7 laps */
    player_row(e + 4, "FREE", 7777);
    e[4 + CHOICE] = EXTRAS_MARK_NITRO;
    put("free_records.dat", d, size);
    records_import(defaults);
    const RecordTable *t = race_records_find(CIRCUIT, LAYOUT_MIRROR, 1, 7);
    CHECK(t != NULL && strcmp((const char *)t->totals[0].row, "FREE") == 0 && t->totals[0].marks.cheats == EXTRAS_MARK_NITRO);
    CHECK(t != NULL && !race_records_row_set(&t->totals[1]));
    const int i = slot_of("free_records.dat", 0);
    CHECK(i >= 0 && files[i].size == size && memcmp(files[i].data, d, size) == 0);
    free(d);
}

/* a records.dat that is not ours (the wrong size) is not read; config.dat's seed is made all the same */
static void test_a_file_not_ours_is_left_out(void)
{
    setup();
    const uint8_t junk[16] = "TNFSREC1junk";
    put("records.dat", junk, sizeof junk);
    put("free_records.dat", junk, 5);
    CHECK(records_import(defaults) > 0);
    CHECK(race_records_find(CIRCUIT, LAYOUT_NORMAL, 0, 4) != NULL);
    CHECK(race_records_find(CIRCUIT, LAYOUT_MIRROR, 0, 4) == NULL);
    const int i = slot_of("records.dat", 0);
    CHECK(i >= 0 && files[i].size == sizeof junk);
}

int main(void)
{
    exe_tables_for_test();
    test_config_dat_seeds_the_normal_tables();
    test_records_dat_is_imported();
    test_free_records_dat_is_imported();
    test_a_file_not_ours_is_left_out();
    files_clear();
    return TEST_REPORT();
}
