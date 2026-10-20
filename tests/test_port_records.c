/* The port's records (port_records.c): every block starts as the original's defaults, a variant's or the cheats' table
 * is its own, and what was written comes back from records.dat. */
#include "game/frontend/config_dat.h"
#include "game/sys/port_records.h"
#include "test.h"

#include "game/platform/platform.h"

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

enum { TR1 = 3, ROW = 0x27 };

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

int main(void)
{
    test_a_block_starts_as_the_originals_defaults();
    test_normal_without_cheats_is_config_dats();
    test_a_record_in_one_table_leaves_the_others();
    test_save_and_load_keep_everything();
    test_a_file_that_is_not_ours_is_ignored();
    test_the_tournament_keeps_only_marks();
    return TEST_REPORT();
}
