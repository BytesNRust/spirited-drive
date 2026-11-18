/* port (dep59 step 5, T5): see race_records_tracks.h. */
#include "game/sys/race_records_tracks.h"

#include "formats/mod_ini.h"
#include "game/platform/platform.h"
#include "game/sys/piece_print.h"
#include "game/sys/port_log.h"
#include "game/sys/race_records_io.h"

#include <stdlib.h>
#include <string.h>

enum {
    MAGIC_SIZE = 8, HEADER = MAGIC_SIZE + 12, KEY = 4, NAMES = RECORDS_TRACK_ID + RECORDS_TRACK_CAR_ID + RECORDS_TRACK_SHORT,
    PRINT = PIECE_PRINT_SIZE, PRINTS = 2 * PRINT,   /* the track's print and the car's, after the names, in a v2 file */
    TABLE_ENTRY = KEY + NAMES + PRINTS + RACE_RECORDS_TABLE_BYTES, LAP_ENTRY = KEY + NAMES + PRINTS + RACE_RECORDS_ROW_BYTES, LAP_KEY = 0,
    LAYOUTS = RACE_RECORDS_LAYOUTS, LAPS_MAX = RACE_RECORDS_LAPS_MAX
};

static const char MAGIC[MAGIC_SIZE] = { 'T', 'N', 'F', 'S', 'R', 'R', 'T', '2' };
static const char MAGIC_V1[MAGIC_SIZE] = { 'T', 'N', 'F', 'S', 'R', 'R', 'T', '1' };   /* before the prints: read, never written */
static const char FILE_NAME[] = "race_records_tracks2.dat", FILE_NAME_V1[] = "race_records_tracks.dat";

typedef struct {
    uint8_t key[KEY];   /* layout, cheats, laps, and 1 for a record an older file held that no print has been put to yet, else 0 */
    char track[RECORDS_TRACK_ID], car[RECORDS_TRACK_CAR_ID], short_name[RECORDS_TRACK_SHORT];
    uint8_t track_print[PRINT], car_print[PRINT];   /* the pieces it was made under, all zeros for none */
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

/* what names an entry: its key's layout, cheats and laps (not the older-file mark), the ids and the prints of the pieces */
typedef struct {
    uint8_t key[KEY];
    const char *track, *car;
    const uint8_t *track_print, *car_print;
} Ident;

static const uint8_t NO_PRINT[PRINT];
static RecordsPrintOf track_print_fn, car_print_fn;

void race_records_tracks_prints_set(RecordsPrintOf track, RecordsPrintOf car) { track_print_fn = track, car_print_fn = car; }

static void print_of(RecordsPrintOf fn, const char *id, uint8_t out[PRINT])
{
    memset(out, 0, PRINT);
    if (fn != NULL) fn(id, out);
}

static int compare(const Ident *a, const Entry *e)
{
    int by = memcmp(a->key, e->key, 3);
    if (by == 0) by = strcmp(a->track, e->track);
    if (by == 0) by = strcmp(a->car, e->car);
    if (by == 0) by = memcmp(a->track_print, e->track_print, PRINT);
    return by != 0 ? by : memcmp(a->car_print, e->car_print, PRINT);
}

static Ident ident_of(const Entry *e)
{
    const Ident id = { { e->key[0], e->key[1], e->key[2], 0 }, e->track, e->car, e->track_print, e->car_print };
    return id;
}

static int lower_bound(const List *l, const Ident *id)
{
    int lo = 0, hi = l->count;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (compare(id, l->at[mid]) > 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static Entry *find(const List *l, const Ident *id)
{
    const int at = lower_bound(l, id);
    return at < l->count && compare(id, l->at[at]) == 0 ? l->at[at] : NULL;
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
    const Ident id = ident_of(e);
    const int at = lower_bound(l, &id);
    memmove(l->at + at + 1, l->at + at, (size_t)(l->count - at) * sizeof *l->at);
    l->at[at] = e;
    l->count++;
    return 1;
}

static Entry *insert(List *l, const Ident *id)
{
    Entry *e = calloc(1, sizeof *e);
    if (e == NULL) return NULL;
    memcpy(e->key, id->key, KEY);
    memcpy(e->track, id->track, strlen(id->track) + 1);
    memcpy(e->car, id->car, strlen(id->car) + 1);
    memcpy(e->track_print, id->track_print, PRINT);
    memcpy(e->car_print, id->car_print, PRINT);
    if (put(l, e)) return e;
    free(e);
    return NULL;
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

/* the entry `old` (an older file's, no print put to it) made the one of the pieces as they are now, when that is not already there: 1 when moved */
static int adopt_at(List *l, int at)
{
    Entry *e = l->at[at];
    if (e->key[3] == 0) return 0;
    uint8_t track_print[PRINT], car_print[PRINT];
    print_of(track_print_fn, e->track, track_print), print_of(car_print_fn, e->car, car_print);
    Ident id = ident_of(e);
    id.track_print = track_print, id.car_print = car_print;
    if (compare(&id, e) == 0 || find(l, &id) != NULL) return 0;   /* no print yet (stays, to wait for the piece), or taken */
    memmove(l->at + at, l->at + at + 1, (size_t)(l->count - at - 1) * sizeof *l->at);
    l->count--;
    memcpy(e->track_print, track_print, PRINT), memcpy(e->car_print, car_print, PRINT), e->key[3] = 0;
    return put(l, e);   /* room was just made in it */
}

/* the entry of a key in `l` under the pieces as they are, made when `make` (a table empty, a lap empty); one an older file held is taken as
 * theirs at the first look; NULL out of range or without memory */
static Entry *entry(List *l, const char *track, int layout, int cheats, int laps, const char *car, int make, int lap)
{
    if (!key_ok(layout, cheats, laps) || !ids_ok(track, car)) return NULL;
    uint8_t track_print[PRINT], car_print[PRINT];
    print_of(track_print_fn, track, track_print), print_of(car_print_fn, car, car_print);
    Ident id = { { 0 }, track, car, track_print, car_print };
    key_of(id.key, layout, cheats, laps);
    Entry *e = find(l, &id);
    if (e == NULL) {
        const Ident old = { { id.key[0], id.key[1], id.key[2], 0 }, track, car, NO_PRINT, NO_PRINT };
        const int at = lower_bound(l, &old);
        if (at < l->count && compare(&old, l->at[at]) == 0 && adopt_at(l, at)) e = find(l, &id);
    }
    if (e != NULL || !make) return e;
    e = insert(l, &id);
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

/* the older file's entries of `track` made theirs under the pieces as they are, each once */
static void adopt_track(List *l, const char *track)
{
    for (int i = 0; i < l->count;)
        if (strcmp(l->at[i]->track, track) == 0 && adopt_at(l, i)) i = 0;
        else i++;
}

int race_records_tracks_cars(const char *track, const char *ids[], int max)
{
    int n = 0;
    if (track == NULL || ids == NULL || max <= 0) return 0;
    uint8_t track_print[PRINT], car_print[PRINT];
    print_of(track_print_fn, track, track_print);
    List *const both[] = { &tables, &laps_list };
    for (int k = 0; k < 2; k++) {
        adopt_track(both[k], track);
        for (int i = 0; i < both[k]->count; i++) {
            const Entry *e = both[k]->at[i];
            print_of(car_print_fn, e->car, car_print);
            if (strcmp(e->track, track) == 0 && memcmp(e->track_print, track_print, PRINT) == 0 && memcmp(e->car_print, car_print, PRINT) == 0)
                n = car_add(ids, n, max, e->car);
        }
    }
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
    memcpy(at + NAMES, e->track_print, PRINT), memcpy(at + NAMES + PRINT, e->car_print, PRINT);
    memcpy(at, e->track, strlen(e->track));
    memcpy(at + RECORDS_TRACK_ID, e->car, strlen(e->car));
    memcpy(at + RECORDS_TRACK_ID + RECORDS_TRACK_CAR_ID, e->short_name, strlen(e->short_name));
    return at + NAMES + PRINTS;
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

/* the entries of `count` at `at` into `l`, each after the one before it; 0 at the first that is not, or out of range. `v1`: a file before the
 * prints, whose entries are marked as waiting for one */
static int read_list(List *l, const uint8_t **at, uint32_t count, int laps, int v1)
{
    const Entry *previous = NULL;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *k = *at, *names = k + KEY, *prints = names + NAMES;
        char track[RECORDS_TRACK_ID], car[RECORDS_TRACK_CAR_ID], short_name[RECORDS_TRACK_SHORT];
        const int good_key = key_ok(k[0], k[1], k[2]) && k[3] == (v1 ? 0 : k[3] & 1) && (!laps || k[2] == LAP_KEY);
        if (!good_key || !field(names, RECORDS_TRACK_ID, track) || !field(names + RECORDS_TRACK_ID, RECORDS_TRACK_CAR_ID, car) ||
            !field(names + RECORDS_TRACK_ID + RECORDS_TRACK_CAR_ID, RECORDS_TRACK_SHORT, short_name) || !ids_ok(track, car))
            return 0;
        Ident id = { { k[0], k[1], k[2], 0 }, track, car, NO_PRINT, NO_PRINT };
        if (!v1) id.track_print = prints, id.car_print = prints + PRINT;
        if (previous != NULL && compare(&id, previous) <= 0) return 0;
        Entry *e = insert(l, &id);
        if (e == NULL) return 0;
        e->key[3] = v1 ? 1 : k[3];
        memcpy(e->short_name, short_name, sizeof e->short_name);
        *at = v1 ? prints : prints + PRINTS;
        *at = laps ? race_records_get_row(*at, &e->u.lap) : race_records_get_table(*at, &e->u.table);
        previous = e;
    }
    return 1;
}

int race_records_tracks_parse(const uint8_t *file, size_t size)
{
    race_records_tracks_reset();
    if (file == NULL || size < HEADER) return 0;
    const int v1 = memcmp(file, MAGIC_V1, MAGIC_SIZE) == 0;
    if (!v1 && memcmp(file, MAGIC, MAGIC_SIZE) != 0) return 0;
    const size_t table_entry = v1 ? TABLE_ENTRY - PRINTS : TABLE_ENTRY, lap_entry = v1 ? LAP_ENTRY - PRINTS : LAP_ENTRY;
    const uint32_t flags = race_records_get32(file + MAGIC_SIZE);
    const uint32_t n_tables = race_records_get32(file + MAGIC_SIZE + 4), n_laps = race_records_get32(file + MAGIC_SIZE + 8);
    if (flags != 0 || HEADER + (uint64_t)n_tables * table_entry + (uint64_t)n_laps * lap_entry != size) return 0;
    const uint8_t *at = file + HEADER;
    if (read_list(&tables, &at, n_tables, 0, v1) && read_list(&laps_list, &at, n_laps, 1, v1)) return 1;
    race_records_tracks_reset();
    return 0;
}

/* the store from `name` (a damaged one set aside as race_records_tracks.bad); NO_FILE when it is not there. `v1`: the file before the prints,
 * read when the new one is not there, never written, and its loss not worth holding saving for */
static RaceRecordsLoad load_file(const char *name, int v1)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(name, &size);
    if (data == NULL && platform_user_file_exists(name)) {   /* there, but not read (locked, an I/O error): not made again over it */
        if (!v1) held = 1;
        port_log("records: %s could not be read; the records of Lunar Springs and the added tracks are not %s\n", name,
                 v1 ? "taken from it" : "saved this session");
    }
    if (data == NULL) return RACE_RECORDS_NO_FILE;
    const int ok = race_records_tracks_parse(data, size);
    if (!ok && !race_records_keep_bad("race_records_tracks", data, size)) {
        if (!v1) held = 1;   /* the old file is never written over: nothing is lost by saving the new one */
        port_log("records: %s is damaged and no copy of it could be kept; its records are %s\n", name,
                 v1 ? "not taken from it" : "not saved this session");
    }
    platform_free_file(data);
    return ok ? RACE_RECORDS_LOADED : RACE_RECORDS_DAMAGED;
}

RaceRecordsLoad race_records_tracks_load(void)
{
    race_records_tracks_reset();
    const RaceRecordsLoad now = load_file(FILE_NAME, 0);
    if (now != RACE_RECORDS_NO_FILE || platform_user_file_exists(FILE_NAME)) return now;
    return load_file(FILE_NAME_V1, 1);   /* none of the new one's yet: the old one's records, each taken as its pieces' at its first look */
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
