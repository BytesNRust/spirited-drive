/* port (dep55): see race_records.h. */
#include "game/sys/race_records.h"

#include "game/platform/platform.h"
#include "game/sys/extras_marks.h"
#include "game/sys/port_log.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAGIC_SIZE = 8, HEADER = MAGIC_SIZE + 12, KEY = 4, ROW_BYTES = RACE_RECORDS_ROW + 4,
    TABLE_ROWS = RACE_RECORDS_ROWS + 1 + 2 * RACE_RECORDS_SEGMENTS,
    TABLE_BYTES = TABLE_ROWS * ROW_BYTES + 2 * RACE_RECORDS_SEGMENTS * 4,
    LAP_KEY = 0   /* a best lap's key's laps byte */
};

static const char MAGIC[MAGIC_SIZE] = { 'T', 'N', 'F', 'S', 'R', 'R', 'D', '1' };

typedef struct {
    uint8_t track, layout, cheats, laps;
    union {
        RecordTable table;
        RecordRow lap;
    } u;
} Entry;

/* a sorted list of entries by key, each allocated on its own so a pointer handed out survives the ones made after it */
typedef struct {
    Entry **at;
    int count, capacity;
} List;

static List tables, laps_list;
static uint8_t tournament;

static long key_of(int track, int layout, int cheats, int laps) { return ((track * RACE_RECORDS_LAYOUTS + layout) * 2L + cheats) * 128L + laps; }
static long entry_key(const Entry *e) { return key_of(e->track, e->layout, e->cheats, e->laps); }

static int lower_bound(const List *l, long key)
{
    int lo = 0, hi = l->count;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (entry_key(l->at[mid]) < key) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static Entry *find(const List *l, int track, int layout, int cheats, int laps)
{
    const long key = key_of(track, layout, cheats, laps);
    const int at = lower_bound(l, key);
    return at < l->count && entry_key(l->at[at]) == key ? l->at[at] : NULL;
}

static Entry *insert(List *l, int track, int layout, int cheats, int laps)
{
    if (l->count == l->capacity) {
        const int bigger = l->capacity ? l->capacity * 2 : 16;
        Entry **grown = realloc(l->at, (size_t)bigger * sizeof *grown);
        if (grown == NULL) return NULL;
        l->at = grown, l->capacity = bigger;
    }
    Entry *e = calloc(1, sizeof *e);
    if (e == NULL) return NULL;
    e->track = (uint8_t)track, e->layout = (uint8_t)layout, e->cheats = (uint8_t)cheats, e->laps = (uint8_t)laps;
    const int at = lower_bound(l, entry_key(e));
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

static int base_ok(int track, int layout, int cheats)
{
    return track >= 0 && track < RACE_RECORDS_TRACKS && layout >= 0 && layout < RACE_RECORDS_LAYOUTS && (cheats == 0 || cheats == 1);
}

/* a stage has laps 0, a circuit 1..99 */
static int table_key_ok(int track, int layout, int cheats, int laps)
{
    if (!base_ok(track, layout, cheats)) return 0;
    return track < RACE_RECORDS_FIRST_CIRCUIT ? laps == 0 : laps >= 1 && laps <= RACE_RECORDS_LAPS_MAX;
}

static RecordMarks running;

RecordMarks race_records_conditions_of(int weather, int snow, int wet, int light_time, int rivals, int police, int traffic)
{
    RecordMarks m = { 0, 0, 0, 0 };
    m.weather = (uint8_t)(RECORD_MARK_KNOWN | (weather & RECORD_WEATHER_KIND) | (snow ? RECORD_WEATHER_SNOW : 0) | (wet ? RECORD_WEATHER_WET : 0));
    m.time = (uint8_t)(RECORD_MARK_KNOWN | (light_time & RECORD_TIME_KIND));
    m.difficulty = (uint8_t)(RECORD_MARK_KNOWN | (rivals & 3) | (police & 3) << 2 | (traffic & 3) << 4);
    return m;
}

int race_records_original_time(int time_of_day)
{
    enum { DAY = 0, DAWN = 1, DUSK = 2 };   /* port_settings.h's LIGHT_TIME_* */
    return time_of_day == 0 ? DAWN : time_of_day == 2 ? DUSK : DAY;
}

void race_records_conditions_set(RecordMarks m) { running = m, running.cheats = 0; }

static int lunar;
void race_records_lunar_set(int on) { lunar = on != 0; }
int race_records_lunar(void) { return lunar; }
RecordMarks race_records_conditions(void) { return running; }

void race_records_row_empty(RecordRow *r)
{
    memset(r, 0, sizeof *r);
    const int32_t longest = INT32_MAX;
    memcpy(r->row + RECORD_ROW_TIME, &longest, 4);
}

int race_records_row_set(const RecordRow *r)
{
    int32_t set;
    memcpy(&set, r->row + RECORD_ROW_SET, 4);
    return set != 0;
}

static void table_empty(RecordTable *t)
{
    memset(t, 0, sizeof *t);
    for (int i = 0; i < RACE_RECORDS_ROWS; i++) race_records_row_empty(&t->totals[i]);
    race_records_row_empty(&t->top);
    for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) race_records_row_empty(&t->segment_time[k]), race_records_row_empty(&t->segment_speed[k]);
}

RecordTable *race_records_find(int track, int layout, int cheats, int laps)
{
    if (!table_key_ok(track, layout, cheats, laps)) return NULL;
    Entry *e = find(&tables, track, layout, cheats, laps);
    return e != NULL ? &e->u.table : NULL;
}

RecordTable *race_records_make(int track, int layout, int cheats, int laps)
{
    RecordTable *have = race_records_find(track, layout, cheats, laps);
    if (have != NULL || !table_key_ok(track, layout, cheats, laps)) return have;
    Entry *e = insert(&tables, track, layout, cheats, laps);
    if (e == NULL) return NULL;
    table_empty(&e->u.table);
    return &e->u.table;
}

int race_records_laps(int track, int layout, int cheats, int *out, int max)
{
    if (!base_ok(track, layout, cheats)) return 0;
    int n = 0;
    for (int at = lower_bound(&tables, key_of(track, layout, cheats, 0)); at < tables.count && n < max; at++) {
        const Entry *e = tables.at[at];
        if (e->track != track || e->layout != layout || e->cheats != cheats) break;
        out[n++] = e->laps;
    }
    return n;
}

RecordRow *race_records_lap_find(int track, int layout, int cheats)
{
    if (!base_ok(track, layout, cheats)) return NULL;
    Entry *e = find(&laps_list, track, layout, cheats, LAP_KEY);
    return e != NULL ? &e->u.lap : NULL;
}

RecordRow *race_records_lap_make(int track, int layout, int cheats)
{
    RecordRow *have = race_records_lap_find(track, layout, cheats);
    if (have != NULL || !base_ok(track, layout, cheats)) return have;
    Entry *e = insert(&laps_list, track, layout, cheats, LAP_KEY);
    if (e == NULL) return NULL;
    race_records_row_empty(&e->u.lap);
    return &e->u.lap;
}

static const int TOURNAMENT_MARKS = EXTRAS_MARK_NITRO | EXTRAS_MARK_EXPLODE | EXTRAS_MARK_TURBO | EXTRAS_MARK_LUNAR;

int race_records_tournament(void) { return tournament; }
void race_records_set_tournament(int marks) { tournament = (uint8_t)(marks & TOURNAMENT_MARKS); }

void race_records_reset(void)
{
    list_free(&tables);
    list_free(&laps_list);
    tournament = 0;
}

/* the file: the magic, the tournament's marks, the tables' and the laps' counts (u32 each, little-endian), then each table (its key, its
 * rows, its stage words) and each lap (its key, its row), ascending by key */
static uint8_t *put32(uint8_t *at, uint32_t v)
{
    for (int k = 0; k < 4; k++) *at++ = (uint8_t)(v >> (8 * k));
    return at;
}

static uint32_t get32(const uint8_t *at) { return (uint32_t)at[0] | (uint32_t)at[1] << 8 | (uint32_t)at[2] << 16 | (uint32_t)at[3] << 24; }

static uint8_t *put_row(uint8_t *at, const RecordRow *r)
{
    memcpy(at, r->row, RACE_RECORDS_ROW), at += RACE_RECORDS_ROW;
    *at++ = r->marks.cheats, *at++ = r->marks.weather, *at++ = r->marks.time, *at++ = r->marks.difficulty;
    return at;
}

static const uint8_t *get_row(const uint8_t *at, RecordRow *r)
{
    memcpy(r->row, at, RACE_RECORDS_ROW), at += RACE_RECORDS_ROW;
    r->row[RECORD_ROW_NAME_MAX - 1] = 0;   /* the name ends inside its field whatever the file holds */
    r->marks.cheats = at[0], r->marks.weather = at[1], r->marks.time = at[2], r->marks.difficulty = at[3];
    return at + 4;
}

static uint8_t *put_key(uint8_t *at, const Entry *e)
{
    *at++ = e->track, *at++ = e->layout, *at++ = e->cheats, *at++ = e->laps;
    return at;
}

static uint8_t *put_table(uint8_t *at, const RecordTable *t)
{
    for (int i = 0; i < RACE_RECORDS_ROWS; i++) at = put_row(at, &t->totals[i]);
    at = put_row(at, &t->top);
    for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) at = put_row(at, &t->segment_time[k]);
    for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) at = put_row(at, &t->segment_speed[k]);
    for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) at = put32(at, (uint32_t)t->stage_speeds[k]);
    for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) at = put32(at, (uint32_t)t->stage_totals[k]);
    return at;
}

static const uint8_t *get_table(const uint8_t *at, RecordTable *t)
{
    for (int i = 0; i < RACE_RECORDS_ROWS; i++) at = get_row(at, &t->totals[i]);
    at = get_row(at, &t->top);
    for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) at = get_row(at, &t->segment_time[k]);
    for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) at = get_row(at, &t->segment_speed[k]);
    for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) t->stage_speeds[k] = (int32_t)get32(at), at += 4;
    for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) t->stage_totals[k] = (int32_t)get32(at), at += 4;
    return at;
}

static int held;
void race_records_hold(int on) { held = on != 0; }

bool race_records_save(void)
{
    if (held) return false;
    const size_t size = HEADER + (size_t)tables.count * (KEY + TABLE_BYTES) + (size_t)laps_list.count * (KEY + ROW_BYTES);
    uint8_t *out = malloc(size);
    if (out == NULL) return false;
    uint8_t *at = out;
    memcpy(at, MAGIC, MAGIC_SIZE), at += MAGIC_SIZE;
    at = put32(at, tournament);
    at = put32(at, (uint32_t)tables.count);
    at = put32(at, (uint32_t)laps_list.count);
    for (int i = 0; i < tables.count; i++) at = put_table(put_key(at, tables.at[i]), &tables.at[i]->u.table);
    for (int i = 0; i < laps_list.count; i++) at = put_row(put_key(at, laps_list.at[i]), &laps_list.at[i]->u.lap);
    const bool ok = platform_write_user_file("race_records.dat", out, size);
    free(out);
    return ok;
}

/* the entries of a file, ascending by key (so no key twice); 0 at the first that is not */
static int read_entries(const uint8_t *data, uint32_t n_tables, uint32_t n_laps)
{
    const uint8_t *at = data + HEADER;
    long previous = -1;
    for (uint32_t i = 0; i < n_tables; i++) {
        if (!table_key_ok(at[0], at[1], at[2], at[3]) || key_of(at[0], at[1], at[2], at[3]) <= previous) return 0;
        previous = key_of(at[0], at[1], at[2], at[3]);
        Entry *e = insert(&tables, at[0], at[1], at[2], at[3]);
        if (e == NULL) return 0;
        at = get_table(at + KEY, &e->u.table);
    }
    previous = -1;
    for (uint32_t i = 0; i < n_laps; i++) {
        if (!base_ok(at[0], at[1], at[2]) || at[3] != LAP_KEY || key_of(at[0], at[1], at[2], at[3]) <= previous) return 0;
        previous = key_of(at[0], at[1], at[2], at[3]);
        Entry *e = insert(&laps_list, at[0], at[1], at[2], at[3]);
        if (e == NULL) return 0;
        at = get_row(at + KEY, &e->u.lap);
    }
    return 1;
}

/* a damaged file's copy, under the first of race_records.bad, .bad2 .. .bad9 not taken; 0 when none could be written */
static int keep_bad(const uint8_t *data, size_t size)
{
    char name[32];
    for (int k = 1; k <= 9; k++) {
        if (k == 1) snprintf(name, sizeof name, "race_records.bad");
        else snprintf(name, sizeof name, "race_records.bad%d", k);
        if (platform_user_file_exists(name)) continue;
        if (!platform_write_user_file(name, data, size)) return 0;
        port_log("records: race_records.dat is damaged; kept as %s, the records start again\n", name);
        return 1;
    }
    return 0;
}

RaceRecordsLoad race_records_load(void)
{
    race_records_reset();
    size_t size = 0;
    uint8_t *data = platform_read_user_file("race_records.dat", &size);
    if (data == NULL) return RACE_RECORDS_NO_FILE;
    bool ok = size >= HEADER && memcmp(data, MAGIC, MAGIC_SIZE) == 0;
    uint32_t n_tables = 0, n_laps = 0;
    if (ok) {
        n_tables = get32(data + MAGIC_SIZE + 4), n_laps = get32(data + MAGIC_SIZE + 8);
        const uint64_t want = HEADER + (uint64_t)n_tables * (KEY + TABLE_BYTES) + (uint64_t)n_laps * (KEY + ROW_BYTES);
        ok = want == size;
    }
    if (ok) ok = read_entries(data, n_tables, n_laps);
    if (ok) race_records_set_tournament((int)get32(data + MAGIC_SIZE));
    if (!ok) {
        race_records_reset();
        if (!keep_bad(data, size)) {
            race_records_hold(1);
            port_log("records: race_records.dat is damaged and no copy of it could be kept; the records are not saved this session\n");
        }
    }
    platform_free_file(data);
    return ok ? RACE_RECORDS_LOADED : RACE_RECORDS_DAMAGED;
}
