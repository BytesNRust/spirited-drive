/* port: records per variant and the table with cheats; see port_records.h. */
#include "game/sys/port_records.h"

#include "game/frontend/config_dat.h"
#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

enum { FIRST_RECORD = 0xb5, VARIANTS = 3, TABLES = 2, MAGIC_SIZE = 8 };

static const char MAGIC[MAGIC_SIZE] = { 'T', 'N', 'F', 'S', 'R', 'E', 'C', '1' };

static struct {
    uint8_t block[RECORDS_TRACKS][VARIANTS][TABLES][RECORDS_BLOCK];
    uint8_t marks[RECORDS_TRACKS][VARIANTS][RECORDS_MARK_SLOTS];
    uint8_t tournament;
    int ready;
} store;

/* every block back to the original's defaults, no marks */
static void defaults(void)
{
    static ConfigDat config;
    memset(&store, 0, sizeof store);
    config_dat_defaults(&config, 0);
    for (int t = 0; t < RECORDS_TRACKS; t++)
        for (int v = 0; v < VARIANTS; v++)
            for (int w = 0; w < TABLES; w++)
                memcpy(store.block[t][v][w], config.image + FIRST_RECORD + RECORDS_BLOCK * t, RECORDS_BLOCK);
    store.ready = 1;
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
    }
    platform_free_file(data);
    if (!ok) defaults();
    return ok;
}
