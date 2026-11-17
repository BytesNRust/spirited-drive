/* port (dep59 step 5, T5): see race_records_tracks.h. */
#include "game/sys/race_records_tracks.h"

#include "formats/mod_ini.h"
#include "game/platform/platform.h"
#include "game/sys/port_log.h"
#include "game/sys/race_records_io.h"

#include <stdlib.h>
#include <string.h>

enum {
    MAGIC_SIZE = 8, HEADER = MAGIC_SIZE + 12, KEY = 4, NAMES = RECORDS_TRACK_ID + RECORDS_TRACK_CAR_ID + RECORDS_TRACK_SHORT,
    TABLE_ENTRY = KEY + NAMES + RACE_RECORDS_TABLE_BYTES, LAP_ENTRY = KEY + NAMES + RACE_RECORDS_ROW_BYTES, LAP_KEY = 0,
    LAYOUTS = RACE_RECORDS_LAYOUTS, LAPS_MAX = RACE_RECORDS_LAPS_MAX
};

static const char MAGIC[MAGIC_SIZE] = { 'T', 'N', 'F', 'S', 'R', 'R', 'T', '1' };
static const char FILE_NAME[] = "race_records_tracks.dat";

typedef struct {
    uint8_t key[KEY];   /* layout, cheats, laps, 0 */
    char track[RECORDS_TRACK_ID], car[RECORDS_TRACK_CAR_ID], short_name[RECORDS_TRACK_SHORT];
    union {
        RecordTable table;
        RecordRow lap;
    } u;
} Entry;

/* sorted by (key, track, car); each entry allocated on its own, so a pointer handed out survives the ones made after it */
typedef struct {
    Entry **at;
    int count, capacity;
} List;

static List tables, laps_list;
static int held;
static char race_track[RECORDS_TRACK_ID];

static int compare(const uint8_t key[KEY], const char *track, const char *car, const Entry *e)
{
    int by = memcmp(key, e->key, KEY);
    if (by == 0) by = strcmp(track, e->track);
    return by != 0 ? by : strcmp(car, e->car);
}

static int lower_bound(const List *l, const uint8_t key[KEY], const char *track, const char *car)
{
    int lo = 0, hi = l->count;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (compare(key, track, car, l->at[mid]) > 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static Entry *find(const List *l, const uint8_t key[KEY], const char *track, const char *car)
{
    const int at = lower_bound(l, key, track, car);
    return at < l->count && compare(key, track, car, l->at[at]) == 0 ? l->at[at] : NULL;
}

static Entry *insert(List *l, const uint8_t key[KEY], const char *track, const char *car)
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
    memcpy(e->track, track, strlen(track) + 1);
    memcpy(e->car, car, strlen(car) + 1);
    const int at = lower_bound(l, key, track, car);
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

static int id_ok(const char *id, size_t cap) { return id != NULL && strlen(id) < cap && mod_ini_id_ok(id); }
static int ids_ok(const char *track, const char *car) { return id_ok(track, RECORDS_TRACK_ID) && id_ok(car, RECORDS_TRACK_CAR_ID); }

/* layout 0..7, cheats 0..1, laps 0..99 (a stage's 0); a lap's key has laps 0 */
static int key_ok(int layout, int cheats, int laps) { return layout >= 0 && layout < LAYOUTS && (cheats == 0 || cheats == 1) && laps >= 0 && laps <= LAPS_MAX; }

static void key_of(uint8_t key[KEY], int layout, int cheats, int laps)
{
    key[0] = (uint8_t)layout, key[1] = (uint8_t)cheats, key[2] = (uint8_t)laps, key[3] = 0;
}

static void keep_short(Entry *e, const char *short_name)
{
    if (short_name == NULL) return;
    memset(e->short_name, 0, sizeof e->short_name);
    strncpy(e->short_name, short_name, sizeof e->short_name - 1);
}

/* the entry of a key in `l`, made when `make` (a table empty, a lap empty); NULL out of range or without memory */
static Entry *entry(List *l, const char *track, int layout, int cheats, int laps, const char *car, int make, int lap)
{
    if (!key_ok(layout, cheats, laps) || !ids_ok(track, car)) return NULL;
    uint8_t key[KEY];
    key_of(key, layout, cheats, laps);
    Entry *e = find(l, key, track, car);
    if (e != NULL || !make) return e;
    e = insert(l, key, track, car);
    if (e == NULL) return NULL;
    if (lap) race_records_row_empty(&e->u.lap);
    else race_records_table_empty(&e->u.table);
    return e;
}

RecordTable *race_records_tracks_find(const char *track, int layout, int cheats, int laps, const char *car)
{
    Entry *e = entry(&tables, track, layout, cheats, laps, car, 0, 0);
    return e != NULL ? &e->u.table : NULL;
}

RecordTable *race_records_tracks_make(const char *track, int layout, int cheats, int laps, const char *car, const char *short_name)
{
    Entry *e = entry(&tables, track, layout, cheats, laps, car, 1, 0);
    if (e == NULL) return NULL;
    keep_short(e, short_name);
    return &e->u.table;
}

RecordRow *race_records_tracks_lap_find(const char *track, int layout, int cheats, const char *car)
{
    Entry *e = entry(&laps_list, track, layout, cheats, LAP_KEY, car, 0, 1);
    return e != NULL ? &e->u.lap : NULL;
}

RecordRow *race_records_tracks_lap_make(const char *track, int layout, int cheats, const char *car, const char *short_name)
{
    Entry *e = entry(&laps_list, track, layout, cheats, LAP_KEY, car, 1, 1);
    if (e == NULL) return NULL;
    keep_short(e, short_name);
    return &e->u.lap;
}

const char *race_records_tracks_short_name(const char *track, int layout, int cheats, int laps, const char *car)
{
    const Entry *e = entry(&tables, track, layout, cheats, laps, car, 0, 0);
    return e != NULL ? e->short_name : "";
}

/* `car` into `ids` (`n` there, ascending by id) unless it is there; the new count. Not past `max` */
static int car_add(const char *ids[], int n, int max, const char *car)
{
    int at = 0;
    while (at < n && strcmp(ids[at], car) < 0) at++;
    if (at < n && strcmp(ids[at], car) == 0) return n;
    if (n == max) return n;
    memmove(ids + at + 1, ids + at, sizeof ids[0] * (size_t)(n - at));
    ids[at] = car;
    return n + 1;
}

int race_records_tracks_cars(const char *track, const char *ids[], int max)
{
    int n = 0;
    if (track == NULL || ids == NULL || max <= 0) return 0;
    const List *const both[] = { &tables, &laps_list };
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < both[k]->count; i++)
            if (strcmp(both[k]->at[i]->track, track) == 0) n = car_add(ids, n, max, both[k]->at[i]->car);
    return n;
}

void race_records_tracks_reset(void)
{
    list_free(&tables);
    list_free(&laps_list);
}

static uint8_t *put_names(uint8_t *at, const Entry *e)
{
    memcpy(at, e->key, KEY), at += KEY;
    memset(at, 0, NAMES);
    memcpy(at, e->track, strlen(e->track));
    memcpy(at + RECORDS_TRACK_ID, e->car, strlen(e->car));
    memcpy(at + RECORDS_TRACK_ID + RECORDS_TRACK_CAR_ID, e->short_name, strlen(e->short_name));
    return at + NAMES;
}

uint8_t *race_records_tracks_image(size_t *size)
{
    *size = HEADER + (size_t)tables.count * TABLE_ENTRY + (size_t)laps_list.count * LAP_ENTRY;
    uint8_t *out = malloc(*size), *at = out;
    if (out == NULL) return NULL;
    memcpy(at, MAGIC, MAGIC_SIZE), at += MAGIC_SIZE;
    at = race_records_put32(at, 0);
    at = race_records_put32(at, (uint32_t)tables.count);
    at = race_records_put32(at, (uint32_t)laps_list.count);
    for (int i = 0; i < tables.count; i++) at = race_records_put_table(put_names(at, tables.at[i]), &tables.at[i]->u.table);
    for (int i = 0; i < laps_list.count; i++) at = race_records_put_row(put_names(at, laps_list.at[i]), &laps_list.at[i]->u.lap);
    return out;
}

/* a field of `cap` bytes at `at` into `out`: 0 when it has no NUL */
static int field(const uint8_t *at, size_t cap, char *out)
{
    if (memchr(at, 0, cap) == NULL) return 0;
    memcpy(out, at, cap);
    return 1;
}

/* the entries of `count` at `at` into `l`, each after the one before it; 0 at the first that is not, or out of range */
static int read_list(List *l, const uint8_t **at, uint32_t count, int laps)
{
    const Entry *previous = NULL;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *k = *at, *names = k + KEY;
        char track[RECORDS_TRACK_ID], car[RECORDS_TRACK_CAR_ID], short_name[RECORDS_TRACK_SHORT];
        const int good_key = key_ok(k[0], k[1], k[2]) && k[3] == 0 && (!laps || k[2] == LAP_KEY);
        if (!good_key || !field(names, RECORDS_TRACK_ID, track) || !field(names + RECORDS_TRACK_ID, RECORDS_TRACK_CAR_ID, car) ||
            !field(names + RECORDS_TRACK_ID + RECORDS_TRACK_CAR_ID, RECORDS_TRACK_SHORT, short_name) || !ids_ok(track, car) ||
            (previous != NULL && compare(k, track, car, previous) <= 0))
            return 0;
        Entry *e = insert(l, k, track, car);
        if (e == NULL) return 0;
        memcpy(e->short_name, short_name, sizeof e->short_name);
        *at = names + NAMES;
        *at = laps ? race_records_get_row(*at, &e->u.lap) : race_records_get_table(*at, &e->u.table);
        previous = e;
    }
    return 1;
}

int race_records_tracks_parse(const uint8_t *file, size_t size)
{
    race_records_tracks_reset();
    if (file == NULL || size < HEADER || memcmp(file, MAGIC, MAGIC_SIZE) != 0) return 0;
    const uint32_t flags = race_records_get32(file + MAGIC_SIZE);
    const uint32_t n_tables = race_records_get32(file + MAGIC_SIZE + 4), n_laps = race_records_get32(file + MAGIC_SIZE + 8);
    if (flags != 0 || HEADER + (uint64_t)n_tables * TABLE_ENTRY + (uint64_t)n_laps * LAP_ENTRY != size) return 0;
    const uint8_t *at = file + HEADER;
    if (read_list(&tables, &at, n_tables, 0) && read_list(&laps_list, &at, n_laps, 1)) return 1;
    race_records_tracks_reset();
    return 0;
}

RaceRecordsLoad race_records_tracks_load(void)
{
    race_records_tracks_reset();
    size_t size = 0;
    uint8_t *data = platform_read_user_file(FILE_NAME, &size);
    if (data == NULL && platform_user_file_exists(FILE_NAME)) {   /* there, but not read (locked, an I/O error): not made again over it */
        held = 1;
        port_log("records: %s could not be read; the records of Lunar Springs and the added tracks are not saved this session\n", FILE_NAME);
    }
    if (data == NULL) return RACE_RECORDS_NO_FILE;
    const int ok = race_records_tracks_parse(data, size);
    if (!ok && !race_records_keep_bad("race_records_tracks", data, size)) {
        held = 1;
        port_log("records: %s is damaged and no copy of it could be kept; its records are not saved this session\n", FILE_NAME);
    }
    platform_free_file(data);
    return ok ? RACE_RECORDS_LOADED : RACE_RECORDS_DAMAGED;
}

bool race_records_tracks_save(void)
{
    if (held) return false;
    size_t size = 0;
    uint8_t *out = race_records_tracks_image(&size);
    if (out == NULL) return false;
    const bool ok = platform_write_user_file(FILE_NAME, out, size);
    free(out);
    return ok;
}

void race_records_tracks_race_set(const char *track)
{
    if (id_ok(track, RECORDS_TRACK_ID)) memcpy(race_track, track, strlen(track) + 1);
    else race_track[0] = '\0';
}

const char *race_records_tracks_race(void) { return race_track[0] != '\0' ? race_track : NULL; }
