/* The port's records (port_records.c): every block starts as the original's defaults, a variant's or the cheats' table
 * is its own, and what was written comes back from records.dat. */
#include "tests/exe_tables_install.h"
#include "game/frontend/config_dat.h"
#include "game/sys/port_records.h"
#include "test.h"

#include "game/platform/platform.h"
#include "game/sys/resource.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One in-memory user file stands in for the platform's storage. */
static char stored_name[32];
static uint8_t stored[0x40000];
static size_t stored_size;
static int stored_present;

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
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

/* The CD's files: read from the disk as the platform would (the test's own copy, not the library's). */
uint8_t *platform_read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *data = malloc(0x80000);
    *size = fread(data, 1, 0x80000, f);
    fclose(f);
    return data;
}

enum { TR1 = 3, ROW = 0x27, MAGIC_TEXT_SIZE = 8 };
static const char MAGIC_TEXT[MAGIC_TEXT_SIZE] = { 'T', 'N', 'F', 'S', 'R', 'E', 'C', '1' };

static const uint8_t *defaults_of(int track)
{
    static ConfigDat config;
    config_dat_defaults(&config, 0);
    return config.image + 0xb5 + RECORDS_BLOCK * track;
}

static void test_a_block_starts_as_the_originals_defaults(void)
{
    CHECK(!port_records_load());   /* no file */
    for (int t = 0; t < RECORDS_TRACKS; t++) {
        CHECK(memcmp(port_records_block(t, RECORDS_MIRROR, 0), defaults_of(t), RECORDS_BLOCK) == 0);
        CHECK(memcmp(port_records_block(t, RECORDS_RALLY, 1), defaults_of(t), RECORDS_BLOCK) == 0);
        CHECK(memcmp(port_records_block(t, RECORDS_NORMAL, 1), defaults_of(t), RECORDS_BLOCK) == 0);
    }
    const uint8_t *marks = port_records_marks(TR1, RECORDS_MIRROR);
    for (int i = 0; i < RECORDS_MARK_SLOTS; i++) CHECK_EQ_INT(marks[i], 0);
    CHECK_EQ_INT(port_records_tournament(), 0);
}

static void test_normal_without_cheats_is_config_dats(void)
{
    CHECK(port_records_block(TR1, RECORDS_NORMAL, 0) == NULL);
    CHECK(port_records_block(-1, RECORDS_MIRROR, 0) == NULL);
    CHECK(port_records_block(RECORDS_TRACKS, RECORDS_MIRROR, 0) == NULL);
    CHECK(port_records_block(TR1, (RecordVariant)3, 1) == NULL);
    CHECK(port_records_marks(TR1, (RecordVariant)-1) == NULL);
}

static void test_a_record_in_one_table_leaves_the_others(void)
{
    port_records_load();
    port_records_block(TR1, RECORDS_MIRROR, 0)[0x10] ^= 0xff;
    CHECK(memcmp(port_records_block(TR1, RECORDS_MIRROR, 0), defaults_of(TR1), RECORDS_BLOCK) != 0);
    CHECK(memcmp(port_records_block(TR1, RECORDS_MIRROR, 1), defaults_of(TR1), RECORDS_BLOCK) == 0);   /* its cheats table */
    CHECK(memcmp(port_records_block(TR1, RECORDS_RALLY, 0), defaults_of(TR1), RECORDS_BLOCK) == 0);
    CHECK(memcmp(port_records_block(TR1, RECORDS_NORMAL, 1), defaults_of(TR1), RECORDS_BLOCK) == 0);
    CHECK(memcmp(port_records_block(TR1 + 1, RECORDS_MIRROR, 0), defaults_of(TR1 + 1), RECORDS_BLOCK) == 0);
}

static void test_save_and_load_keep_everything(void)
{
    port_records_load();
    port_records_block(TR1, RECORDS_MIRROR, 0)[0x10] = 0x5a;
    port_records_block(TR1, RECORDS_NORMAL, 1)[0xa6a] = 0x77;   /* the block's last byte */
    port_records_block(8, RECORDS_RALLY, 1)[0] = 0x33;
    port_records_marks(TR1, RECORDS_NORMAL)[5] = EXTRAS_MARK_NITRO | EXTRAS_MARK_TURBO;
    port_records_marks(8, RECORDS_RALLY)[63] = EXTRAS_MARK_EXPLODE;
    port_records_set_tournament(EXTRAS_MARK_NITRO);
    CHECK(port_records_save());
    CHECK(strcmp(stored_name, "records.dat") == 0);
    port_records_block(TR1, RECORDS_MIRROR, 0)[0x10] = 0;   /* lost in memory, not in the file */
    CHECK(port_records_load());
    CHECK_EQ_INT(port_records_block(TR1, RECORDS_MIRROR, 0)[0x10], 0x5a);
    CHECK_EQ_INT(port_records_block(TR1, RECORDS_NORMAL, 1)[0xa6a], 0x77);
    CHECK_EQ_INT(port_records_block(8, RECORDS_RALLY, 1)[0], 0x33);
    CHECK_EQ_INT(port_records_marks(TR1, RECORDS_NORMAL)[5], EXTRAS_MARK_NITRO | EXTRAS_MARK_TURBO);
    CHECK_EQ_INT(port_records_marks(8, RECORDS_RALLY)[63], EXTRAS_MARK_EXPLODE);
    CHECK_EQ_INT(port_records_tournament(), EXTRAS_MARK_NITRO);
    CHECK_EQ_INT(port_records_marks(TR1, RECORDS_MIRROR)[5], 0);
}

static void test_a_file_that_is_not_ours_is_ignored(void)
{
    port_records_load();
    port_records_block(TR1, RECORDS_MIRROR, 0)[0x10] = 0x5a;
    CHECK(port_records_save());
    stored[3] ^= 1;   /* the magic */
    CHECK(!port_records_load());
    CHECK(memcmp(port_records_block(TR1, RECORDS_MIRROR, 0), defaults_of(TR1), RECORDS_BLOCK) == 0);
    stored[3] ^= 1;
    stored_size--;    /* short */
    CHECK(!port_records_load());
    CHECK(memcmp(port_records_block(TR1, RECORDS_MIRROR, 0), defaults_of(TR1), RECORDS_BLOCK) == 0);
}

static void test_the_tournament_keeps_only_marks(void)
{
    port_records_load();
    port_records_set_tournament(0xff);
    CHECK_EQ_INT(port_records_tournament(), EXTRAS_MARK_NITRO | EXTRAS_MARK_EXPLODE | EXTRAS_MARK_TURBO);
}

/* The CD's CONFIG.DAT, the file the user's config.dat starts from. Read here as the test's own copy. */
#ifdef TNFS_SOURCE_DIR
static const char CD_CONFIG[] = TNFS_SOURCE_DIR "/original/GAMEDATA/CONFIG/CONFIG.DAT";
static const char NO_DATA[] = "/nonexistent-tnfs-data";
#endif

/* 1 when the CD's file is there and parses (the tests skip otherwise). */
static int cd_image(ConfigDat *out)
{
#ifdef TNFS_SOURCE_DIR
    uint8_t data[CONFIG_DAT_SIZE];
    FILE *f = fopen(CD_CONFIG, "rb");
    if (f == NULL) return 0;
    const int whole = fread(data, 1, sizeof data, f) == sizeof data;
    fclose(f);
    return whole && config_dat_parse(out, data, sizeof data) == CONFIG_OK;
#else
    (void)out;
    return 0;
#endif
}

/* records.dat as an earlier build wrote it: every block the built-in defaults, no marks, no tournament */
static void stored_as_built_in_defaults(void)
{
    static ConfigDat builtin;
    uint8_t *at = stored;
    config_dat_defaults(&builtin, 0);
    memcpy(at, MAGIC_TEXT, MAGIC_TEXT_SIZE), at += MAGIC_TEXT_SIZE;
    *at++ = 0;
    for (int t = 0; t < RECORDS_TRACKS; t++)
        for (int v = 0; v < 3; v++) {
            for (int w = v == RECORDS_NORMAL; w < 2; w++) {
                memcpy(at, builtin.image + 0xb5 + RECORDS_BLOCK * t, RECORDS_BLOCK);
                at += RECORDS_BLOCK;
            }
            memset(at, 0, RECORDS_MARK_SLOTS), at += RECORDS_MARK_SLOTS;
        }
    stored_size = (size_t)(at - stored);
    stored_present = 1;
}

/* The CD's rows: a fresh table of every track, variant and choice starts as the CD's CONFIG.DAT records (skipped when
 * original/ is not there). */
static void test_fresh_tables_are_the_cd_records(void)
{
#ifdef TNFS_SOURCE_DIR
    static ConfigDat cd;
    if (!cd_image(&cd)) return;
    resource_init(TNFS_SOURCE_DIR "/original", 0);
    stored_present = 0;
    CHECK(!port_records_load());
    for (int t = 0; t < RECORDS_TRACKS; t++)
        for (int v = 0; v < 3; v++)
            for (int w = 0; w < 2; w++) {
                if (v == RECORDS_NORMAL && w == 0) continue;   /* config.dat's own */
                CHECK(memcmp(port_records_block(t, (RecordVariant)v, w), cd.image + 0xb5 + RECORDS_BLOCK * t,
                             RECORDS_BLOCK) == 0);
            }
    /* track 4 (Autumn Valley), short race: row 6 is the CD's Dippy at 3:02.3 (10942 ticks) */
    const uint8_t *row = port_records_block(4, RECORDS_NORMAL, 1) + 0xea + 5 * ROW;
    CHECK(memcmp(row, "Dippy", 6) == 0);
    CHECK_EQ_INT(row[0x1b] | row[0x1c] << 8 | row[0x1d] << 16 | (uint32_t)row[0x1e] << 24, 10942);
    resource_init(NO_DATA, 0);
#endif
}

/* A records.dat from an earlier build: a row the player set stays (and the marks), and a row still the built-in default
 * takes the CD's row (skipped when original/ is not there). */
static void test_migration_keeps_the_players_rows_and_takes_the_cd_rows(void)
{
#ifdef TNFS_SOURCE_DIR
    static ConfigDat cd;
    if (!cd_image(&cd)) return;
    stored_as_built_in_defaults();
    /* track 4's chunk: its first block is the normal table with cheats; its row 6 (index 5) is the player's */
    uint8_t *const chunk = stored + MAGIC_TEXT_SIZE + 1 + 4 * (5 * RECORDS_BLOCK + 3 * RECORDS_MARK_SLOTS);
    memcpy(chunk + 0xea + 5 * ROW, "PLAYER", 7);
    resource_init(TNFS_SOURCE_DIR "/original", 0);
    CHECK(port_records_load());
    const uint8_t *block = port_records_block(4, RECORDS_NORMAL, 1);
    CHECK(memcmp(block + 0xea + 5 * ROW, "PLAYER", 7) == 0);   /* the player's row stays */
    CHECK(memcmp(block + 0xea + 6 * ROW, cd.image + 0xb5 + 4 * RECORDS_BLOCK + 0xea + 6 * ROW, ROW) == 0);
    CHECK(memcmp(block + 0xea + 6 * ROW, "Silver Spyder", 13) == 0);
    CHECK(memcmp(port_records_block(4, RECORDS_MIRROR, 0) + 0xea, cd.image + 0xb5 + 4 * RECORDS_BLOCK + 0xea, ROW) == 0);
    resource_init(NO_DATA, 0);
#endif
}

int main(void)
{
    exe_tables_for_test();
    test_a_block_starts_as_the_originals_defaults();
    test_normal_without_cheats_is_config_dats();
    test_a_record_in_one_table_leaves_the_others();
    test_save_and_load_keep_everything();
    test_a_file_that_is_not_ours_is_ignored();
    test_the_tournament_keeps_only_marks();
    test_fresh_tables_are_the_cd_records();
    test_migration_keeps_the_players_rows_and_takes_the_cd_rows();
    return TEST_REPORT();
}
