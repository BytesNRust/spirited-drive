/* port: records per variant and the table with cheats; see port_records.h. */
#include "game/sys/port_records.h"

#include "game/frontend/config_dat.h"
#include "game/platform/platform.h"
#include "game/sys/resource.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    FIRST_RECORD = 0xb5, VARIANTS = 3, TABLES = 2, MAGIC_SIZE = 8,
    ROW = 0x27,              /* a record's row (text 0x00 to 0x27, as config.dat keeps it) */
    TIMES = 0xea,            /* the first laps choice's ten rows, from the block's start */
    CHOICE = 0x186,          /* a laps choice's ten rows past the first three tracks */
    TOP = 0x57c,             /* the top speed's row */
    SEGMENT_ROWS = 6,        /* the stage rows: 3 times, then 3 speeds */
    CIRCUIT_FROM = 3, CHOICES = 3, ROWS = 10
};

static const char MAGIC[MAGIC_SIZE] = { 'T', 'N', 'F', 'S', 'R', 'E', 'C', '1' };

static struct {
    uint8_t block[RECORDS_TRACKS][VARIANTS][TABLES][RECORDS_BLOCK];
    uint8_t marks[RECORDS_TRACKS][VARIANTS][RECORDS_MARK_SLOTS];
    uint8_t tournament;
    int ready;
} store;

/* config.dat's CD copy (config.c's load_from_cd reads the same file): the records the tables start as, when it is
 * readable */
static int cd_config(ConfigDat *out)
{
    char path[512], upper[512];
    size_t size = 0;
    snprintf(path, sizeof path, "%s%s", resource_dir(PATH_CONFIG), "config.dat");
    uint8_t *data = platform_read_file(path, &size);
    if (data == NULL) {
        snprintf(upper, sizeof upper, "%s", path);
        for (char *c = upper; *c; c++) *c = (char)toupper((unsigned char)*c);
        data = platform_read_file(upper, &size);
    }
    if (data == NULL) return 0;
    const ConfigResult r = config_dat_parse(out, data, size);
    platform_free_file(data);
    return r == CONFIG_OK;
}

/* the block of a track in a config image: config.dat's records as 00429278 lays them out */
static const uint8_t *block_in(const ConfigDat *c, int track) { return c->image + FIRST_RECORD + RECORDS_BLOCK * track; }

/* every block back to the CD's records (the built-in defaults when there is no CD), no marks */
static void defaults(void)
{
    static ConfigDat builtin, cd;
    config_dat_defaults(&builtin, 0);
    const ConfigDat *const source = cd_config(&cd) ? &cd : &builtin;
    memset(&store, 0, sizeof store);
    for (int t = 0; t < RECORDS_TRACKS; t++)
        for (int v = 0; v < VARIANTS; v++)
            for (int w = 0; w < TABLES; w++) memcpy(store.block[t][v][w], block_in(source, t), RECORDS_BLOCK);
    store.ready = 1;
}

/* the record rows of a track's block (their offsets): the stage rows, the top speed's, and each laps choice's ten */
static int record_rows(int track, size_t *offsets)
{
    int n = 0;
    for (int k = 0; k < SEGMENT_ROWS; k++) offsets[n++] = (size_t)(k * ROW);
    offsets[n++] = TOP;
    for (int ch = 0; ch < (track >= CIRCUIT_FROM ? CHOICES : 1); ch++)
        for (int r = 0; r < ROWS; r++) offsets[n++] = (size_t)(TIMES + ch * CHOICE + r * ROW);
    return n;
}

/* A records.dat from an earlier build seeded its tables from the built-in defaults: a record row still byte-identical to
 * the built-in default at its place takes the CD's; every other row (the player's) stays. */
static void take_cd_rows(void)
{
    static ConfigDat builtin, cd;
    config_dat_defaults(&builtin, 0);
    const ConfigDat *const source = cd_config(&cd) ? &cd : &builtin;
    size_t offsets[SEGMENT_ROWS + 1 + CHOICES * ROWS];
    for (int t = 0; t < RECORDS_TRACKS; t++) {
        const int n = record_rows(t, offsets);
        for (int v = 0; v < VARIANTS; v++)
            for (int w = 0; w < TABLES; w++) {
                uint8_t *const block = store.block[t][v][w];
                const uint8_t *const old = block_in(&builtin, t), *const cd_block = block_in(source, t);
                for (int i = 0; i < n; i++) {
                    if (memcmp(block + offsets[i], old + offsets[i], ROW) == 0)
                        memcpy(block + offsets[i], cd_block + offsets[i], ROW);
                }
            }
    }
}

const uint8_t *port_records_default_block(int track)
{
    static ConfigDat builtin, cd;
    static const ConfigDat *source;
    if (track < 0 || track >= RECORDS_TRACKS) return NULL;
    if (source == NULL) {
        config_dat_defaults(&builtin, 0);
        source = cd_config(&cd) ? &cd : &builtin;
    }
    return block_in(source, track);
}

static void ensure(void)
{
    if (!store.ready) defaults();
}

static int in_range(int track, RecordVariant v) { return track >= 0 && track < RECORDS_TRACKS && v >= 0 && v < VARIANTS; }

uint8_t *port_records_block(int track, RecordVariant v, int with_cheats)
{
    if (!in_range(track, v) || (v == RECORDS_NORMAL && !with_cheats)) return NULL;
    ensure();
    return store.block[track][v][with_cheats != 0];
}

uint8_t *port_records_marks(int track, RecordVariant v)
{
    if (!in_range(track, v)) return NULL;
    ensure();
    return store.marks[track][v];
}

int port_records_tournament(void)
{
    ensure();
    return store.tournament;
}

void port_records_set_tournament(int marks)
{
    ensure();
    store.tournament = (uint8_t)(marks & (EXTRAS_MARK_NITRO | EXTRAS_MARK_EXPLODE | EXTRAS_MARK_TURBO));
}

/* what the file holds: the magic, the tournament's marks, then each track's variants' blocks (the cheats table's for
 * the normal one, both for the others) and the cheats table's marks */
static size_t file_size(void)
{
    return MAGIC_SIZE + 1 +
           (size_t)RECORDS_TRACKS * (((VARIANTS - 1) * TABLES + 1) * RECORDS_BLOCK + VARIANTS * RECORDS_MARK_SLOTS);
}

bool port_records_save(void)
{
    ensure();
    const size_t size = file_size();
    uint8_t *out = malloc(size);
    if (out == NULL) return false;
    uint8_t *at = out;
    memcpy(at, MAGIC, MAGIC_SIZE), at += MAGIC_SIZE;
    *at++ = store.tournament;
    for (int t = 0; t < RECORDS_TRACKS; t++)
        for (int v = 0; v < VARIANTS; v++) {
            for (int w = v == RECORDS_NORMAL; w < TABLES; w++) memcpy(at, store.block[t][v][w], RECORDS_BLOCK), at += RECORDS_BLOCK;
            memcpy(at, store.marks[t][v], RECORDS_MARK_SLOTS), at += RECORDS_MARK_SLOTS;
        }
    const bool ok = platform_write_user_file("records.dat", out, size);
    free(out);
    return ok;
}

bool port_records_load(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file("records.dat", &size);
    defaults();
    if (data == NULL) return false;
    bool ok = size == file_size() && memcmp(data, MAGIC, MAGIC_SIZE) == 0;
    if (ok) {
        const uint8_t *at = data + MAGIC_SIZE;
        store.tournament = (uint8_t)(*at++ & (EXTRAS_MARK_NITRO | EXTRAS_MARK_EXPLODE | EXTRAS_MARK_TURBO));
        for (int t = 0; t < RECORDS_TRACKS; t++)
            for (int v = 0; v < VARIANTS; v++) {
                for (int w = v == RECORDS_NORMAL; w < TABLES; w++) memcpy(store.block[t][v][w], at, RECORDS_BLOCK), at += RECORDS_BLOCK;
                memcpy(store.marks[t][v], at, RECORDS_MARK_SLOTS), at += RECORDS_MARK_SLOTS;
            }
        take_cd_rows();
    }
    platform_free_file(data);
    if (!ok) defaults();
    return ok;
}
