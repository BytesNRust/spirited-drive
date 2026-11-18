/* port (dep59, 1b): see race_records_added.h. */
#include "game/sys/race_records_added.h"

#include "formats/mod_ini.h"
#include "formats/stock_content.h"
#include "game/platform/platform.h"
#include "game/sys/port_log.h"
#include "game/sys/race_records_io.h"

#include <stdlib.h>
#include <string.h>

enum {
    MAGIC_SIZE = 8, HEADER_V1 = MAGIC_SIZE + 8, HEADER = MAGIC_SIZE + 12, KEY = 4, FLAG_IMPORTED = 1, NAMES = RECORDS_ADDED_ID + RECORDS_ADDED_SHORT,
    PRINT = 32, PRINTS = 1 + PRINT,   /* after the names in a v3 file: 1 when the record waits for the print of its car (an older file's), then the print */
    TABLE_ENTRY = KEY + NAMES + PRINTS + RACE_RECORDS_TABLE_BYTES, LAP_ENTRY = KEY + NAMES + PRINTS + RACE_RECORDS_ROW_BYTES, LAP_KEY = 0
};

static const char MAGIC[MAGIC_SIZE] = { 'T', 'N', 'F', 'S', 'R', 'R', 'A', '3' };
static const char MAGIC_V2[MAGIC_SIZE] = { 'T', 'N', 'F', 'S', 'R', 'R', 'A', '2' };   /* before the prints: read, never written */
static const char MAGIC_V1[MAGIC_SIZE] = { 'T', 'N', 'F', 'S', 'R', 'R', 'A', '1' };   /* step 3's: no flags */
static const char FILE_NAME[] = "race_records_cars2.dat", FILE_NAME_V2[] = "race_records_cars.dat", FILE_NAME_V1[] = "race_records_added.dat";

typedef struct {
    uint8_t key[KEY];   /* track, layout, cheats, laps */
    char id[RECORDS_ADDED_ID], short_name[RECORDS_ADDED_SHORT];
    uint8_t waits, print[PRINT];   /* the car's print it was made under (all zeros for none); `waits`: an older file's, no print put to it yet */
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
static int held, imported;

static const uint8_t NO_PRINT[PRINT];
static RecordsPrintOf print_fn;

void race_records_added_prints_set(RecordsPrintOf car) { print_fn = car; }

static void print_of(const char *id, uint8_t out[PRINT])
{
    memset(out, 0, PRINT);
    if (print_fn != NULL) print_fn(id, out);
}

static int compare(const uint8_t key[KEY], const char *id, const uint8_t print[PRINT], const Entry *e)
{
    int by = memcmp(key, e->key, KEY);
    if (by == 0) by = strcmp(id, e->id);
    return by != 0 ? by : memcmp(print, e->print, PRINT);
}

static int lower_bound(const List *l, const uint8_t key[KEY], const char *id, const uint8_t print[PRINT])
{
    int lo = 0, hi = l->count;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (compare(key, id, print, l->at[mid]) > 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static Entry *find(const List *l, const uint8_t key[KEY], const char *id, const uint8_t print[PRINT])
{
    const int at = lower_bound(l, key, id, print);
    return at < l->count && compare(key, id, print, l->at[at]) == 0 ? l->at[at] : NULL;
}

/* `e` into its place in `l` (room made first) */
static int put(List *l, Entry *e)
{
    if (l->count == l->capacity) {
        const int bigger = l->capacity ? l->capacity * 2 : 16;
        Entry **grown = realloc(l->at, (size_t)bigger * sizeof *grown);
        if (grown == NULL) return 0;
        l->at = grown, l->capacity = bigger;
    }
    const int at = lower_bound(l, e->key, e->id, e->print);
    memmove(l->at + at + 1, l->at + at, (size_t)(l->count - at) * sizeof *l->at);
    l->at[at] = e;
    l->count++;
    return 1;
}

static Entry *insert(List *l, const uint8_t key[KEY], const char *id, const uint8_t print[PRINT])
{
    Entry *e = calloc(1, sizeof *e);
    if (e == NULL) return NULL;
    memcpy(e->key, key, KEY);
    memcpy(e->id, id, strlen(id) + 1);
    memcpy(e->print, print, PRINT);
    if (put(l, e)) return e;
    free(e);
    return NULL;
}

/* the entry at `at` of `l` (an older file's, no print put to it) made the one of the car as it is now, when that is not there already: 1 when moved */
static int adopt_at(List *l, int at)
{
    Entry *e = l->at[at];
    uint8_t print[PRINT];
    print_of(e->id, print);
    if (!e->waits || memcmp(print, e->print, PRINT) == 0 || find(l, e->key, e->id, print) != NULL) return 0;   /* no print yet: waits for the car */
    memmove(l->at + at, l->at + at + 1, (size_t)(l->count - at - 1) * sizeof *l->at);
    l->count--;
    memcpy(e->print, print, PRINT), e->waits = 0;
    return put(l, e);   /* room was just made in it */
}

/* the entry of a key for the car `id` as it is now: an older file's one is taken as its at the first look */
static Entry *entry(List *l, const uint8_t key[KEY], const char *id)
{
    uint8_t print[PRINT];
    print_of(id, print);
    Entry *e = find(l, key, id, print);
    if (e == NULL) {
        const int at = lower_bound(l, key, id, NO_PRINT);
        if (at < l->count && compare(key, id, NO_PRINT, l->at[at]) == 0 && adopt_at(l, at)) e = find(l, key, id, print);
    }
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
    Entry *e = entry(&tables, key, id);
    return e != NULL ? &e->u.table : NULL;
}

RecordTable *race_records_added_make(int track, int layout, int cheats, int laps, const char *id, const char *short_name)
{
    if (!race_records_table_key_ok(track, layout, cheats, laps) || !id_ok(id)) return NULL;
    uint8_t key[KEY];
    key_of(key, track, layout, cheats, laps);
    Entry *e = entry(&tables, key, id);
    if (e == NULL) {
        uint8_t print[PRINT];
        print_of(id, print);
        e = insert(&tables, key, id, print);
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
    Entry *e = entry(&laps_list, key, id);
    return e != NULL ? &e->u.lap : NULL;
}

RecordRow *race_records_added_lap_make(int track, int layout, int cheats, const char *id, const char *short_name)
{
    if (!race_records_base_ok(track, layout, cheats) || !id_ok(id)) return NULL;
    uint8_t key[KEY];
    key_of(key, track, layout, cheats, LAP_KEY);
    Entry *e = entry(&laps_list, key, id);
    if (e == NULL) {
        uint8_t print[PRINT];
        print_of(id, print);
        e = insert(&laps_list, key, id, print);
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
    const Entry *e = entry(&tables, key, id);
    return e != NULL ? e->short_name : "";
}

void race_records_added_reset(void)
{
    list_free(&tables);
    list_free(&laps_list);
    imported = 0;
}

int race_records_cars_imported(void) { return imported; }
void race_records_cars_set_imported(int on) { imported = on != 0; }

static uint8_t *put_names(uint8_t *at, const Entry *e)
{
    memcpy(at, e->key, KEY), at += KEY;
    memset(at, 0, NAMES);
    memcpy(at, e->id, strlen(e->id));
    memcpy(at + RECORDS_ADDED_ID, e->short_name, strlen(e->short_name));
    at[NAMES] = e->waits, memcpy(at + NAMES + 1, e->print, PRINT);
    return at + NAMES + PRINTS;
}

uint8_t *race_records_added_image(size_t *size)
{
    *size = HEADER + (size_t)tables.count * TABLE_ENTRY + (size_t)laps_list.count * LAP_ENTRY;
    uint8_t *out = malloc(*size), *at = out;
    if (out == NULL) return NULL;
    memcpy(at, MAGIC, MAGIC_SIZE), at += MAGIC_SIZE;
    at = race_records_put32(at, imported ? FLAG_IMPORTED : 0);
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

/* the entries of `count` at `at` into `l`, each after the one before it; 0 at the first that is not, or out of range. `v3`: a file with the
 * prints; the entries of an older one wait for the print of their car */
static int read_list(List *l, const uint8_t **at, uint32_t count, int laps, int v3)
{
    const Entry *previous = NULL;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *k = *at, *prints = k + KEY + NAMES;
        char id[RECORDS_ADDED_ID], short_name[RECORDS_ADDED_SHORT];
        const int key_ok = laps ? race_records_base_ok(k[0], k[1], k[2]) && k[3] == LAP_KEY : race_records_table_key_ok(k[0], k[1], k[2], k[3]);
        if (!key_ok || !names_ok(k, id, short_name) || (v3 && prints[0] > 1)) return 0;
        const uint8_t *print = v3 ? prints + 1 : NO_PRINT;
        if (previous != NULL && compare(k, id, print, previous) <= 0) return 0;
        Entry *e = insert(l, k, id, print);
        if (e == NULL) return 0;
        e->waits = v3 ? prints[0] : 1;
        memcpy(e->short_name, short_name, sizeof e->short_name);
        *at = v3 ? prints + PRINTS : prints;
        *at = laps ? race_records_get_row(*at, &e->u.lap) : race_records_get_table(*at, &e->u.table);
        previous = e;
    }
    return 1;
}

int race_records_added_parse(const uint8_t *file, size_t size)
{
    race_records_added_reset();
    if (file == NULL || size < HEADER_V1) return 0;
    const int v1 = memcmp(file, MAGIC_V1, MAGIC_SIZE) == 0, v2 = memcmp(file, MAGIC_V2, MAGIC_SIZE) == 0;
    const int v3 = !v1 && !v2 && memcmp(file, MAGIC, MAGIC_SIZE) == 0;
    if (!v1 && !v2 && !v3) return 0;
    if (!v1 && size < HEADER) return 0;
    const uint32_t flags = v1 ? 0 : race_records_get32(file + MAGIC_SIZE);
    const uint8_t *const counts = file + (v1 ? MAGIC_SIZE : MAGIC_SIZE + 4);
    const uint32_t n_tables = race_records_get32(counts), n_laps = race_records_get32(counts + 4);
    const size_t header = v1 ? HEADER_V1 : HEADER, table_entry = v3 ? TABLE_ENTRY : TABLE_ENTRY - PRINTS, lap_entry = v3 ? LAP_ENTRY : LAP_ENTRY - PRINTS;
    if ((flags & ~(uint32_t)FLAG_IMPORTED) != 0 || header + (uint64_t)n_tables * table_entry + (uint64_t)n_laps * lap_entry != size) return 0;
    const uint8_t *at = file + header;
    if (read_list(&tables, &at, n_tables, 0, v3) && read_list(&laps_list, &at, n_laps, 1, v3)) {
        imported = (flags & FLAG_IMPORTED) != 0;
        return 1;
    }
    race_records_added_reset();
    return 0;
}

RaceRecordsLoad race_records_added_load(void)
{
    race_records_added_reset();
    /* the file, or the one before the prints, or step 3's, when there is none newer (read once: the next save makes the new one, the older left as it was) */
    const char *name = FILE_NAME, *stem = "race_records_cars2";
    if (!platform_user_file_exists(FILE_NAME) && platform_user_file_exists(FILE_NAME_V2)) name = FILE_NAME_V2, stem = "race_records_cars";
    else if (!platform_user_file_exists(FILE_NAME) && platform_user_file_exists(FILE_NAME_V1)) name = FILE_NAME_V1, stem = "race_records_added";
    const int old = name != FILE_NAME;   /* an older file is never written over: losing its records is no reason to hold the new one's saving */
    size_t size = 0;
    uint8_t *data = platform_read_user_file(name, &size);
    if (data == NULL && platform_user_file_exists(name)) {   /* there, but not read (locked, an I/O error): not made again over it */
        if (!old) held = 1;
        port_log("records: %s could not be read; the records by car are %s\n", name, old ? "not taken from it" : "not saved this session");
    }
    if (data == NULL) return RACE_RECORDS_NO_FILE;
    const int ok = race_records_added_parse(data, size);
    if (ok)   /* what was read: whether the standard rows are in it (race_records_cars_import) */
        port_log("records: %s read: %d tables, %d laps, imported %d\n", name, tables.count, laps_list.count, imported);
    if (!ok && !race_records_keep_bad(stem, data, size)) {
        if (!old) held = 1;
        port_log("records: %s is damaged and no copy of it could be kept; the records by car are %s\n", name, old ? "not taken from it" : "not saved this session");
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

/* port (dep59 step 4): a standard row into its car's table, after the rows there (a table's rows come best first, so each car's stay in
 * order); 0 when the row's car has no id (8 PACK, 9 CLOCK, 10 CLASS) or the table cannot be made */
static int32_t row_car(const RecordRow *r)
{
    int32_t car;
    memcpy(&car, r->row + RECORD_ROW_CAR, 4);
    return car;
}

static RecordTable *car_table(int track, int layout, int cheats, int laps, const RecordRow *r)
{
    const char *const id = stock_car_id(row_car(r));
    return id != NULL && race_records_row_set(r) ? race_records_added_make(track, layout, cheats, laps, id, NULL) : NULL;
}

static void import_table(int track, int layout, int cheats, int laps, const RecordTable *from)
{
    for (int i = 0; i < RACE_RECORDS_ROWS; i++) {
        RecordTable *const to = car_table(track, layout, cheats, laps, &from->totals[i]);
        if (to == NULL) continue;
        int at = 0;
        while (at < RACE_RECORDS_ROWS && race_records_row_set(&to->totals[at])) at++;
        if (at == RACE_RECORDS_ROWS) continue;
        to->totals[at] = from->totals[i];
        if (i == 0)   /* the best run's figures go with its row */
            for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) to->stage_speeds[k] = from->stage_speeds[k], to->stage_totals[k] = from->stage_totals[k];
    }
    RecordTable *const top = car_table(track, layout, cheats, laps, &from->top);
    if (top != NULL) top->top = from->top;
    for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) {
        RecordTable *const time = car_table(track, layout, cheats, laps, &from->segment_time[k]);
        if (time != NULL) time->segment_time[k] = from->segment_time[k];
        RecordTable *const speed = car_table(track, layout, cheats, laps, &from->segment_speed[k]);
        if (speed != NULL) speed->segment_speed[k] = from->segment_speed[k];
    }
}

int race_records_cars_import(void)
{
    if (imported) return 0;
    for (int track = 0; track < RACE_RECORDS_TRACKS; track++)
        for (int layout = 0; layout < RACE_RECORDS_LAYOUTS; layout++)
            for (int cheats = 0; cheats < 2; cheats++) {
                int laps[RACE_RECORDS_LAPS_MAX + 1] = { 0 };
                const int n = track < RACE_RECORDS_FIRST_CIRCUIT ? 1 : race_records_laps(track, layout, cheats, laps, RACE_RECORDS_LAPS_MAX);
                for (int i = 0; i < n; i++) {
                    const RecordTable *const t = race_records_find(track, layout, cheats, laps[i]);
                    if (t != NULL) import_table(track, layout, cheats, laps[i], t);
                }
                const RecordRow *const lap = track < RACE_RECORDS_FIRST_CIRCUIT ? NULL : race_records_lap_find(track, layout, cheats);
                const char *const id = lap != NULL && race_records_row_set(lap) ? stock_car_id(row_car(lap)) : NULL;
                RecordRow *const to = id != NULL ? race_records_added_lap_make(track, layout, cheats, id, NULL) : NULL;
                if (to != NULL) *to = *lap;
            }
    imported = 1;
    return 1;
}
