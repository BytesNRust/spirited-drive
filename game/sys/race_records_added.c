/* port (dep59, 1b): see race_records_added.h. */
#include "game/sys/race_records_added.h"

#include "formats/mod_ini.h"
#include "game/platform/platform.h"
#include "game/sys/port_log.h"
#include "game/sys/race_records_io.h"

#include <stdlib.h>
#include <string.h>

enum {
    MAGIC_SIZE = 8, HEADER = MAGIC_SIZE + 8, KEY = 4, NAMES = RECORDS_ADDED_ID + RECORDS_ADDED_SHORT,
    TABLE_ENTRY = KEY + NAMES + RACE_RECORDS_TABLE_BYTES, LAP_ENTRY = KEY + NAMES + RACE_RECORDS_ROW_BYTES, LAP_KEY = 0
};

static const char MAGIC[MAGIC_SIZE] = { 'T', 'N', 'F', 'S', 'R', 'R', 'A', '1' };
static const char FILE_NAME[] = "race_records_added.dat";

typedef struct {
    uint8_t key[KEY];   /* track, layout, cheats, laps */
    char id[RECORDS_ADDED_ID], short_name[RECORDS_ADDED_SHORT];
    union {
        RecordTable table;
        RecordRow lap;
    } u;
} Entry;

/* sorted by (key, id); each entry allocated on its own, so a pointer handed out survives the ones made after it */
typedef struct {
    Entry **at;
    int count, capacity;
} List;

static List tables, laps_list;
static int held;

static int compare(const uint8_t key[KEY], const char *id, const Entry *e)
{
    const int by_key = memcmp(key, e->key, KEY);
    return by_key != 0 ? by_key : strcmp(id, e->id);
}

static int lower_bound(const List *l, const uint8_t key[KEY], const char *id)
{
    int lo = 0, hi = l->count;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (compare(key, id, l->at[mid]) > 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static Entry *find(const List *l, const uint8_t key[KEY], const char *id)
{
    const int at = lower_bound(l, key, id);
    return at < l->count && compare(key, id, l->at[at]) == 0 ? l->at[at] : NULL;
}

static Entry *insert(List *l, const uint8_t key[KEY], const char *id)
{
    if (l->count == l->capacity) {
        const int bigger = l->capacity ? l->capacity * 2 : 16;
        Entry **grown = realloc(l->at, (size_t)bigger * sizeof *grown);
        if (grown == NULL) return NULL;
        l->at = grown, l->capacity = bigger;
    }
    Entry *e = calloc(1, sizeof *e);
    if (e == NULL) return NULL;
    memcpy(e->key, key, KEY);
    memcpy(e->id, id, strlen(id) + 1);
    const int at = lower_bound(l, key, id);
    memmove(l->at + at + 1, l->at + at, (size_t)(l->count - at) * sizeof *l->at);
    l->at[at] = e;
    l->count++;
    return e;
}

static void list_free(List *l)
{
    for (int i = 0; i < l->count; i++) free(l->at[i]);
    free(l->at);
    l->at = NULL, l->count = l->capacity = 0;
}

static int id_ok(const char *id) { return id != NULL && strlen(id) < RECORDS_ADDED_ID && mod_ini_id_ok(id); }

static void key_of(uint8_t key[KEY], int track, int layout, int cheats, int laps)
{
    key[0] = (uint8_t)track, key[1] = (uint8_t)layout, key[2] = (uint8_t)cheats, key[3] = (uint8_t)laps;
}

static void keep_short(Entry *e, const char *short_name)
{
    if (short_name == NULL) return;
    memset(e->short_name, 0, sizeof e->short_name);
    strncpy(e->short_name, short_name, sizeof e->short_name - 1);
}

RecordTable *race_records_added_find(int track, int layout, int cheats, int laps, const char *id)
{
    if (!race_records_table_key_ok(track, layout, cheats, laps) || !id_ok(id)) return NULL;
    uint8_t key[KEY];
    key_of(key, track, layout, cheats, laps);
    Entry *e = find(&tables, key, id);
    return e != NULL ? &e->u.table : NULL;
}

RecordTable *race_records_added_make(int track, int layout, int cheats, int laps, const char *id, const char *short_name)
{
    if (!race_records_table_key_ok(track, layout, cheats, laps) || !id_ok(id)) return NULL;
    uint8_t key[KEY];
    key_of(key, track, layout, cheats, laps);
    Entry *e = find(&tables, key, id);
    if (e == NULL) {
        e = insert(&tables, key, id);
        if (e == NULL) return NULL;
        race_records_table_empty(&e->u.table);
    }
    keep_short(e, short_name);
    return &e->u.table;
}

RecordRow *race_records_added_lap_find(int track, int layout, int cheats, const char *id)
{
    if (!race_records_base_ok(track, layout, cheats) || !id_ok(id)) return NULL;
    uint8_t key[KEY];
    key_of(key, track, layout, cheats, LAP_KEY);
    Entry *e = find(&laps_list, key, id);
    return e != NULL ? &e->u.lap : NULL;
}

RecordRow *race_records_added_lap_make(int track, int layout, int cheats, const char *id, const char *short_name)
{
    if (!race_records_base_ok(track, layout, cheats) || !id_ok(id)) return NULL;
    uint8_t key[KEY];
    key_of(key, track, layout, cheats, LAP_KEY);
    Entry *e = find(&laps_list, key, id);
    if (e == NULL) {
        e = insert(&laps_list, key, id);
        if (e == NULL) return NULL;
        race_records_row_empty(&e->u.lap);
    }
    keep_short(e, short_name);
    return &e->u.lap;
}

const char *race_records_added_short_name(int track, int layout, int cheats, int laps, const char *id)
{
    if (!race_records_table_key_ok(track, layout, cheats, laps) || !id_ok(id)) return "";
    uint8_t key[KEY];
    key_of(key, track, layout, cheats, laps);
    const Entry *e = find(&tables, key, id);
    return e != NULL ? e->short_name : "";
}

void race_records_added_reset(void)
{
    list_free(&tables);
    list_free(&laps_list);
}

static uint8_t *put_names(uint8_t *at, const Entry *e)
{
    memcpy(at, e->key, KEY), at += KEY;
    memset(at, 0, NAMES);
    memcpy(at, e->id, strlen(e->id));
    memcpy(at + RECORDS_ADDED_ID, e->short_name, strlen(e->short_name));
    return at + NAMES;
}

uint8_t *race_records_added_image(size_t *size)
{
    *size = HEADER + (size_t)tables.count * TABLE_ENTRY + (size_t)laps_list.count * LAP_ENTRY;
    uint8_t *out = malloc(*size), *at = out;
    if (out == NULL) return NULL;
    memcpy(at, MAGIC, MAGIC_SIZE), at += MAGIC_SIZE;
    at = race_records_put32(at, (uint32_t)tables.count);
    at = race_records_put32(at, (uint32_t)laps_list.count);
    for (int i = 0; i < tables.count; i++) at = race_records_put_table(put_names(at, tables.at[i]), &tables.at[i]->u.table);
    for (int i = 0; i < laps_list.count; i++) at = race_records_put_row(put_names(at, laps_list.at[i]), &laps_list.at[i]->u.lap);
    return out;
}

/* an entry's key, id and short name at `at`: 0 when the id or the short name has no NUL in its field, or the id is not a mod's */
static int names_ok(const uint8_t *at, char id[RECORDS_ADDED_ID], char short_name[RECORDS_ADDED_SHORT])
{
    const uint8_t *name = at + KEY, *shorter = name + RECORDS_ADDED_ID;
    if (memchr(name, 0, RECORDS_ADDED_ID) == NULL || memchr(shorter, 0, RECORDS_ADDED_SHORT) == NULL) return 0;
    memcpy(id, name, RECORDS_ADDED_ID), memcpy(short_name, shorter, RECORDS_ADDED_SHORT);
    return id_ok(id);
}

/* the entries of `count` at `at` into `l`, each after the one before it; 0 at the first that is not, or out of range */
static int read_list(List *l, const uint8_t **at, uint32_t count, int laps)
{
    const Entry *previous = NULL;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *k = *at;
        char id[RECORDS_ADDED_ID], short_name[RECORDS_ADDED_SHORT];
        const int key_ok = laps ? race_records_base_ok(k[0], k[1], k[2]) && k[3] == LAP_KEY : race_records_table_key_ok(k[0], k[1], k[2], k[3]);
        if (!key_ok || !names_ok(k, id, short_name) || (previous != NULL && compare(k, id, previous) <= 0)) return 0;
        Entry *e = insert(l, k, id);
        if (e == NULL) return 0;
        memcpy(e->short_name, short_name, sizeof e->short_name);
        *at = k + KEY + NAMES;
        *at = laps ? race_records_get_row(*at, &e->u.lap) : race_records_get_table(*at, &e->u.table);
        previous = e;
    }
    return 1;
}

int race_records_added_parse(const uint8_t *file, size_t size)
{
    race_records_added_reset();
    if (file == NULL || size < HEADER || memcmp(file, MAGIC, MAGIC_SIZE) != 0) return 0;
    const uint32_t n_tables = race_records_get32(file + MAGIC_SIZE), n_laps = race_records_get32(file + MAGIC_SIZE + 4);
    if (HEADER + (uint64_t)n_tables * TABLE_ENTRY + (uint64_t)n_laps * LAP_ENTRY != size) return 0;
    const uint8_t *at = file + HEADER;
    if (read_list(&tables, &at, n_tables, 0) && read_list(&laps_list, &at, n_laps, 1)) return 1;
    race_records_added_reset();
    return 0;
}

RaceRecordsLoad race_records_added_load(void)
{
    race_records_added_reset();
    size_t size = 0;
    uint8_t *data = platform_read_user_file(FILE_NAME, &size);
    if (data == NULL) return RACE_RECORDS_NO_FILE;
    const int ok = race_records_added_parse(data, size);
    if (!ok && !race_records_keep_bad("race_records_added", data, size)) {
        held = 1;
        port_log("records: %s is damaged and no copy of it could be kept; the added cars' records are not saved this session\n", FILE_NAME);
    }
    platform_free_file(data);
    return ok ? RACE_RECORDS_LOADED : RACE_RECORDS_DAMAGED;
}

bool race_records_added_save(void)
{
    if (held) return false;
    size_t size = 0;
    uint8_t *out = race_records_added_image(&size);
    if (out == NULL) return false;
    const bool ok = platform_write_user_file(FILE_NAME, out, size);
    free(out);
    return ok;
}
