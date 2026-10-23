/* Where a finished race's records go (records_route.c): config.dat's image for a normal race without cheats, the
 * port's tables for a variant or a cheat, the marks that follow the rows. record_check.c's own routines run on them. */
#include "game/frontend/config_dat.h"
#include "game/frontend/records_glue.h"
#include "game/frontend/records_route.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

static char stored_name[32];
static uint8_t stored[0x40000];
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
bool platform_key_down(int scancode) { (void)scancode; return false; }

enum { TR1 = 3, WARRIOR = 0xb, ME = 0x95, LAPS = 9, TOP = 0x6d, TOTAL = 0x65, FIRST = 0xb5 };

typedef struct { uint8_t image[RECORD_CHECK_IMAGE]; uint8_t game[RECORD_CHECK_GAME]; } Race;

static void p16(uint8_t *at, int v) { at[0] = (uint8_t)v, at[1] = (uint8_t)(v >> 8); }
static void p32(uint8_t *at, int32_t v) { memcpy(at, &v, 4); }
static int32_t g32(const uint8_t *at) { int32_t v; memcpy(&v, at, 4); return v; }

/* a fresh image (the defaults) and a race on a circuit with one lap of `lap` ticks, a top speed and a total */
static void race(Race *r, int car, int difficulty, int lap, int top, int total)
{
    ConfigDat c;
    config_dat_defaults(&c, 0);
    memcpy(r->image, c.image, sizeof r->image);
    memset(r->game, 0, sizeof r->game);
    p16(r->image + 0x95, TR1);
    p16(r->image + 0x9b, car);
    p16(r->image + 0xb3, difficulty);
    memcpy(r->game, "ME", 3);
    for (int i = 0; i < 20; i++) p32(r->game + ME * 0 + LAPS + 4 * i, 0x7fffffff);
    p32(r->game + LAPS, lap);
    p32(r->game + TOP, top);
    p32(r->game + TOTAL, total);
}

static const uint8_t *block_of(const uint8_t *image) { return image + FIRST + RECORDS_BLOCK * TR1; }

static void fresh(void)
{
    stored_present = 0;
    saves = 0;
    port_records_load();
}

static void test_a_normal_race_without_cheats_takes_the_images_table(void)
{
    fresh();
    Race r;
    race(&r, 0, 0, 1, 99999, 1);
    const Race before = r;
    RaceKind kind = { RECORDS_NORMAL, 0 };
    CHECK_EQ_INT(records_route(kind, ROUTE_LAP, r.image, r.game, 0, NULL), 1);
    CHECK(memcmp(block_of(r.image), block_of(before.image), RECORDS_BLOCK) != 0);   /* config.dat's */
    CHECK_EQ_INT(g32(block_of(r.image) + 0xd0 - FIRST), 1);
    CHECK_EQ_INT(saves, 0);                                                          /* the port's file untouched */
    CHECK(memcmp(port_records_block(TR1, RECORDS_MIRROR, 0), block_of(before.image), RECORDS_BLOCK) == 0);
}

static void test_a_mirror_race_leaves_the_images_table(void)
{
    fresh();
    Race r;
    race(&r, 0, 0, 1, 99999, 1);
    const Race before = r;
    RaceKind kind = { RECORDS_MIRROR, 0 };
    CHECK_EQ_INT(records_route(kind, ROUTE_LAP, r.image, r.game, 0, NULL), 1);
    CHECK(memcmp(r.image, before.image, sizeof r.image) == 0);                                  /* config.dat's image */
    CHECK_EQ_INT(g32(port_records_block(TR1, RECORDS_MIRROR, 0) + 0xd0 - FIRST), 1);
    CHECK(memcmp(port_records_block(TR1, RECORDS_RALLY, 0), block_of(before.image), RECORDS_BLOCK) == 0);
    CHECK(memcmp(port_records_block(TR1, RECORDS_NORMAL, 1), block_of(before.image), RECORDS_BLOCK) == 0);
    CHECK_EQ_INT(saves, 1);
    CHECK_EQ_INT(port_records_marks(TR1, RECORDS_MIRROR)[SLOT_SEGMENT], 0);                       /* no cheats, no mark */
}

static void test_a_race_with_nitro_goes_to_the_cheats_table_with_its_mark(void)
{
    fresh();
    Race r;
    race(&r, 0, 0, 1, 99999, 1);
    const Race before = r;
    RaceKind kind = { RECORDS_NORMAL, EXTRAS_MARK_NITRO };
    CHECK_EQ_INT(records_route(kind, ROUTE_LAP, r.image, r.game, 0, NULL), 1);
    CHECK(memcmp(r.image, before.image, sizeof r.image) == 0);
    CHECK_EQ_INT(g32(port_records_block(TR1, RECORDS_NORMAL, 1) + 0xd0 - FIRST), 1);
    CHECK_EQ_INT(port_records_marks(TR1, RECORDS_NORMAL)[SLOT_SEGMENT], EXTRAS_MARK_NITRO);
    kind.marks = EXTRAS_MARK_NITRO | EXTRAS_MARK_TURBO;
    race(&r, 0, 0, 1, 99999, 1);   /* not better than the 1 now there: not taken */
    CHECK_EQ_INT(records_route(kind, ROUTE_LAP, r.image, r.game, 0, NULL), 0);
    CHECK_EQ_INT(port_records_marks(TR1, RECORDS_NORMAL)[SLOT_SEGMENT], EXTRAS_MARK_NITRO);
}

static void test_totals_marks_follow_their_rows(void)
{
    fresh();
    Race r;
    int32_t times[RECORD_CHECK_TIMES] = { 0 };
    RaceKind nitro = { RECORDS_NORMAL, EXTRAS_MARK_NITRO }, turbo = { RECORDS_NORMAL, EXTRAS_MARK_TURBO };
    race(&r, 0, 1, 5000, 99999, 100);
    CHECK_EQ_INT(records_route(nitro, ROUTE_TOTALS, r.image, r.game, 0, times), 0);   /* the first row */
    race(&r, 0, 1, 5000, 99999, 50);
    CHECK_EQ_INT(records_route(turbo, ROUTE_TOTALS, r.image, r.game, 0, times), 0);   /* in front of it */
    const uint8_t *marks = port_records_marks(TR1, RECORDS_NORMAL);
    const int base = records_slot_totals(TR1, 1, 0);
    CHECK_EQ_INT(base, SLOT_TOTALS + SLOT_CHOICE);
    CHECK_EQ_INT(marks[base], EXTRAS_MARK_TURBO);
    CHECK_EQ_INT(marks[base + 1], EXTRAS_MARK_NITRO);
    CHECK_EQ_INT(marks[base + 2], 0);
    CHECK_EQ_INT(marks[records_slot_totals(TR1, 0, 0)], 0);   /* the other laps choice has its own */
}

static void test_the_warrior_saves_nothing(void)
{
    fresh();
    Race r;
    race(&r, WARRIOR, 0, 1, 99999, 1);
    RaceKind kind = { RECORDS_MIRROR, 0 };
    CHECK_EQ_INT(records_route(kind, ROUTE_LAP, r.image, r.game, 0, NULL), 0);
    CHECK_EQ_INT(saves, 0);
    int32_t times[RECORD_CHECK_TIMES] = { 0 };
    CHECK_EQ_INT(records_route(kind, ROUTE_TOTALS, r.image, r.game, 0, times), RECORD_CHECK_NONE);
}

static void test_a_track_with_no_table_is_not_taken(void)
{
    fresh();
    Race r;
    race(&r, 0, 0, 1, 99999, 1);
    p16(r.image + 0x95, 9);
    RaceKind kind = { RECORDS_RALLY, 0 };
    CHECK_EQ_INT(records_route(kind, ROUTE_LAP, r.image, r.game, 0, NULL), 0);
    CHECK_EQ_INT(saves, 0);
}

static void test_the_kind_comes_from_the_flags(void)
{
    RaceKind k = records_race_kind(0, 0, 0, 0, 0);
    CHECK_EQ_INT(k.variant, RECORDS_NORMAL);
    CHECK_EQ_INT(k.marks, 0);
    k = records_race_kind(1, 0, 1, 1, 8);
    CHECK_EQ_INT(k.variant, RECORDS_MIRROR);
    CHECK_EQ_INT(k.marks, EXTRAS_MARK_NITRO | EXTRAS_MARK_EXPLODE | EXTRAS_MARK_TURBO);
    CHECK_EQ_INT(records_race_kind(1, 1, 0, 0, 0).variant, RECORDS_RALLY);
    CHECK_EQ_INT(records_race_kind(0, 0, 0, 0, 0).marks, 0);
}


/* The results screen shows the table its race was checked on: a race with a cheat shows the cheats table's rows, so
 * the row it highlights is the one it set, and a normal race shows config.dat's (owner's case: TJD's 1:58.5 is the
 * normal table's; a 2:00.4 cheats race beats the cheats table's seed and must show that row, not the normal one). */
static void test_the_results_show_the_table_the_race_was_checked_on(void)
{
    static ConfigDat config;
    static uint8_t shown[RECORD_CHECK_IMAGE];
    static uint8_t game[RECORD_CHECK_GAME];
    static const uint8_t no_races[18];
    int32_t times[RECORD_CHECK_TIMES] = { 0 };
    config_dat_defaults(&config, 0);
    uint8_t *const image = config.image;
    fresh();
    memset(game, 0, sizeof game);
    memcpy(game, "TJD", 4);
    p32(game + 0x59, 7224);   /* 2:00.4 */
    p32(game + 0x61, 5932531);
    p32(game + 0x6d, 5932531);
    memcpy(image + 0xb5, "TJD", 4);   /* the normal table's segment 1 (config.dat's, 1:58.5) */
    p32(image + 0xd0, 7112);
    p32(image + 0x149, 5932531);   /* its top speed, as the owner's: equal to this race's */
    image[0x93] = 1;   /* segment 1 */

    records_glue_begin(0, no_races, 0, 0, 0, 1, 0);   /* a race with NITRO */
    records_glue_shown(shown, image);
    CHECK_EQ_INT(records_glue_route(ROUTE_SEGMENT, image, game, 0, times), 3);
    records_glue_shown(shown, image);
    CHECK(memcmp(shown + 0xb5, "TJD", 4) == 0);
    CHECK_EQ_INT(g32(shown + 0xd0), 7224);
    CHECK_EQ_INT(g32(image + 0xd0), 7112);   /* the normal table is not the cheats race's */

    records_glue_begin(0, no_races, 0, 0, 0, 0, 0);   /* a normal race: the results show config.dat's */
    records_glue_shown(shown, image);
    CHECK(memcmp(shown, image, RECORD_CHECK_IMAGE) == 0);
    CHECK_EQ_INT(records_glue_route(ROUTE_SEGMENT, image, game, 0, times), 0);   /* 2:00.4 does not beat 1:58.5 */
    records_glue_shown(shown, image);
    CHECK_EQ_INT(g32(shown + 0xd0), 7112);
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

int main(void)
{
    test_the_results_show_the_table_the_race_was_checked_on();
    test_a_normal_race_without_cheats_takes_the_images_table();
    test_a_mirror_race_leaves_the_images_table();
    test_a_race_with_nitro_goes_to_the_cheats_table_with_its_mark();
    test_totals_marks_follow_their_rows();
    test_the_warrior_saves_nothing();
    test_a_track_with_no_table_is_not_taken();
    test_the_kind_comes_from_the_flags();
    return TEST_REPORT();
}
