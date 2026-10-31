/* port (free records): see free_records.h. */
#include "game/sys/free_records.h"

#include "game/platform/platform.h"
#include "game/sys/free_laps.h"
#include "game/sys/port_log.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAGIC_SIZE = 8, HEADER = MAGIC_SIZE + 4, ENTRY = 4 + FREE_RECORDS_TABLE + FREE_RECORDS_ROWS, FIRST_CIRCUIT = 3, LAST_CIRCUIT = 8,
    VARIANTS = 3, CHOICE = 0x186, TIMES = 0xea
};

static const char MAGIC[MAGIC_SIZE] = { 'T', 'N', 'F', 'S', 'F', 'R', 'E', '1' };

typedef struct {
    uint8_t track, variant, cheats, laps;
    FreeTable table;
} Entry;

/* the entries by key, each allocated on its own so a pointer handed out survives the ones made after it */
static Entry **entries;
static int count, capacity, loaded;   /* loaded: the file was read (or the store emptied on purpose) */

bool free_records_load(void);
static void ensure(void)
{
    if (!loaded) free_records_load();
}

static int key_ok(int track, int v, int cheats, int laps)
{
    return track >= FIRST_CIRCUIT && track <= LAST_CIRCUIT && v >= 0 && v < VARIANTS && (cheats == 0 || cheats == 1) && laps >= 1 &&
           laps <= FREE_LAPS_MAX;
}

static long key_of(int track, int v, int cheats, int laps) { return ((track * VARIANTS + v) * 2 + cheats) * 128L + laps; }
static long entry_key(const Entry *e) { return key_of(e->track, e->variant, e->cheats, e->laps); }

/* the index of the first entry whose key is not below `key` */
static int lower_bound(long key)
{
    int lo = 0, hi = count;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (entry_key(entries[mid]) < key) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

FreeTable *free_records_find(int track, RecordVariant v, int cheats, int laps)
{
    ensure();
    if (!key_ok(track, (int)v, cheats, laps)) return NULL;
    const long key = key_of(track, (int)v, cheats, laps);
    const int at = lower_bound(key);
    return at < count && entry_key(entries[at]) == key ? &entries[at]->table : NULL;
}

static Entry *insert(int track, int v, int cheats, int laps)
{
    if (count == capacity) {
        const int bigger = capacity ? capacity * 2 : 16;
        Entry **grown = realloc(entries, (size_t)bigger * sizeof *grown);
        if (grown == NULL) return NULL;
        entries = grown, capacity = bigger;
    }
    Entry *e = calloc(1, sizeof *e);
    if (e == NULL) return NULL;
    e->track = (uint8_t)track, e->variant = (uint8_t)v, e->cheats = (uint8_t)cheats, e->laps = (uint8_t)laps;
    const int at = lower_bound(entry_key(e));
    memmove(entries + at + 1, entries + at, (size_t)(count - at) * sizeof *entries);
    entries[at] = e;
    count++;
    return e;
}

FreeTable *free_records_make(int track, RecordVariant v, int cheats, int laps, const uint8_t seed[FREE_RECORDS_TABLE])
{
    FreeTable *have = free_records_find(track, v, cheats, laps);
    if (have != NULL || !key_ok(track, (int)v, cheats, laps)) return have;
    Entry *e = insert(track, (int)v, cheats, laps);
    if (e == NULL) return NULL;
    memcpy(e->table.rows, seed, FREE_RECORDS_TABLE);
    return &e->table;
}

int free_records_laps(int track, RecordVariant v, int cheats, int *out, int max)
{
    ensure();
    if (!key_ok(track, (int)v, cheats, 1)) return 0;
    int n = 0;
    for (int at = lower_bound(key_of(track, (int)v, cheats, 0)); at < count && n < max; at++) {
        const Entry *e = entries[at];
        if (e->track != track || e->variant != (int)v || e->cheats != cheats) break;
        out[n++] = e->laps;
    }
    return n;
}

int free_records_exe_rows(int track, int choice, uint8_t out[FREE_RECORDS_TABLE])
{
    if (free_laps_exe_laps(track, choice) == 0) return 0;
    const uint8_t *block = port_records_default_block(track);
    if (block == NULL) return 0;
    memcpy(out, block + TIMES + CHOICE * choice, FREE_RECORDS_TABLE);
    return 1;
}

static int32_t scaled(int32_t time, int laps, int exe_laps)
{
    const int64_t v = (int64_t)time * laps / exe_laps;
    return v > INT32_MAX ? INT32_MAX : (int32_t)v;
}

void free_records_seed(int track, int laps, uint8_t out[FREE_RECORDS_TABLE])
{
    int best = 0, best_distance = 1 << 30;
    for (int choice = 0; choice < 3; choice++) {
        const int exe = free_laps_exe_laps(track, choice), d = abs(exe - laps);
        if (exe != 0 && d < best_distance) best = choice, best_distance = d;
    }
    if (!free_records_exe_rows(track, best, out)) {
        memset(out, 0, FREE_RECORDS_TABLE);
        return;
    }
    const int exe_laps = free_laps_exe_laps(track, best);
    for (int row = 0; row < FREE_RECORDS_ROWS; row++) {
        uint8_t *at = out + FREE_RECORDS_ROW * row + FREE_RECORDS_TIME;
        int32_t time;
        memcpy(&time, at, 4);
        time = scaled(time, laps, exe_laps);
        memcpy(at, &time, 4);
    }
}

void free_records_reset(void)
{
    for (int i = 0; i < count; i++) free(entries[i]);
    free(entries);
    entries = NULL;
    count = capacity = 0;
    loaded = 1;
}

bool free_records_save(void)
{
    const size_t size = HEADER + (size_t)count * ENTRY;
    uint8_t *out = malloc(size);
    if (out == NULL) return false;
    uint8_t *at = out;
    memcpy(at, MAGIC, MAGIC_SIZE), at += MAGIC_SIZE;
    const uint32_t n = (uint32_t)count;
    for (int k = 0; k < 4; k++) *at++ = (uint8_t)(n >> (8 * k));
    for (int i = 0; i < count; i++) {
        const Entry *e = entries[i];
        *at++ = e->track, *at++ = e->variant, *at++ = e->cheats, *at++ = e->laps;
        memcpy(at, e->table.rows, FREE_RECORDS_TABLE), at += FREE_RECORDS_TABLE;
        memcpy(at, e->table.marks, FREE_RECORDS_ROWS), at += FREE_RECORDS_ROWS;
    }
    const bool ok = platform_write_user_file("free_records.dat", out, size);
    free(out);
    return ok;
}

bool free_records_load(void)
{
    free_records_reset();   /* (the store is the file's from here, found or not) */
    size_t size = 0;
    uint8_t *data = platform_read_user_file("free_records.dat", &size);
    if (data == NULL) return false;
    bool ok = size >= HEADER && memcmp(data, MAGIC, MAGIC_SIZE) == 0;
    uint32_t n = 0;
    if (ok) {
        for (int k = 0; k < 4; k++) n |= (uint32_t)data[MAGIC_SIZE + k] << (8 * k);
        ok = n <= (size - HEADER) / ENTRY && size == HEADER + (size_t)n * ENTRY;
    }
    const uint8_t *at = data + HEADER;
    long previous = -1;
    for (uint32_t i = 0; ok && i < n; i++, at += ENTRY) {
        ok = key_ok(at[0], at[1], at[2], at[3]) && key_of(at[0], at[1], at[2], at[3]) > previous;   /* ascending, so no key twice */
        if (!ok) break;
        previous = key_of(at[0], at[1], at[2], at[3]);
        Entry *e = insert(at[0], at[1], at[2], at[3]);
        ok = e != NULL;
        if (ok) memcpy(&e->table, at + 4, sizeof e->table);
    }
    platform_free_file(data);
    if (!ok) {
        port_log("free_records: free_records.dat is damaged, ignored\n");
        free_records_reset();
    }
    return ok;
}
