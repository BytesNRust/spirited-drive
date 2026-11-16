/* See race_records_merge.h. */
#include "game/sys/race_records_merge.h"

#include "formats/stock_content.h"
#include "game/race/car_registry.h"
#include "game/sys/edition_file.h"
#include "game/sys/race_records_added.h"

#include <string.h>

enum { ROWS = RACE_RECORDS_ROWS, SEGMENTS = RACE_RECORDS_SEGMENTS, JOINED = EDITION_LIST_MAX * RACE_RECORDS_ROWS };

static int32_t field(const RecordRow *r, int at)
{
    int32_t v;
    memcpy(&v, r->row + at, sizeof v);
    return v;
}

/* an added car's row with its number put in (the store keeps 0 for it); a stock car's as it is */
static RecordRow numbered(const RecordRow *r, int car)
{
    RecordRow out = *r;
    if (car >= 0 && race_records_row_set(&out)) {
        memcpy(out.row + RECORD_ROW_CAR, &car, sizeof car);
        memcpy(out.row + RECORD_ROW_CAR_B, &car, sizeof car);
    }
    return out;
}

/* the number an added car's rows are shown with; -1 for a stock car (its rows keep theirs) and an added car with no piece loaded */
static int added_number(const char *id) { return stock_car_of_id(id) >= 0 ? -1 : car_registry_find(id); }

static void table_empty(RecordTable *t)
{
    memset(t, 0, sizeof *t);
    for (int i = 0; i < ROWS; i++) race_records_row_empty(&t->totals[i]);
    race_records_row_empty(&t->top);
    for (int k = 0; k < SEGMENTS; k++) race_records_row_empty(&t->segment_time[k]), race_records_row_empty(&t->segment_speed[k]);
}

/* `r` into `best` when it is set and better: a time lower (`faster` 1) or a speed higher; an equal one leaves the first */
static void keep_best(RecordRow *best, const RecordRow *r, int faster, int car)
{
    if (!race_records_row_set(r)) return;
    const int better = !race_records_row_set(best) ? 1
                     : faster ? field(r, RECORD_ROW_TIME) < field(best, RECORD_ROW_TIME)
                              : field(r, RECORD_ROW_SPEED) > field(best, RECORD_ROW_SPEED);
    if (better) *best = numbered(r, car);
}

/* the set totals of `t` into `rows` after the `n` there, each before the first slower one already there (a stable insertion: equal
 * times keep the order they came in); the new count */
static int join_totals(RecordRow rows[], int n, const RecordTable *t, int car)
{
    for (int i = 0; i < ROWS && n < JOINED; i++) {
        if (!race_records_row_set(&t->totals[i])) continue;
        const int32_t time = field(&t->totals[i], RECORD_ROW_TIME);
        int at = n;
        while (at > 0 && field(&rows[at - 1], RECORD_ROW_TIME) > time) at--;
        memmove(&rows[at + 1], &rows[at], sizeof rows[0] * (size_t)(n - at));
        rows[at] = numbered(&t->totals[i], car);
        n++;
    }
    return n;
}

int race_records_cars_merge(int track, int layout, int cheats, int laps, const char *const ids[], int n, RecordTable *out)
{
    static RecordRow rows[JOINED + 1];
    table_empty(out);
    int found = 0, count = 0;
    const RecordTable *first = NULL;   /* the table the best total is from */
    for (int c = 0; c < n; c++) {
        const RecordTable *const t = race_records_added_find(track, layout, cheats, laps, ids[c]);
        if (t == NULL) continue;
        found = 1;
        const int car = added_number(ids[c]);
        if (race_records_row_set(&t->totals[0]) && (first == NULL || field(&t->totals[0], RECORD_ROW_TIME) < field(&rows[0], RECORD_ROW_TIME)))
            first = t;
        count = join_totals(rows, count, t, car);
        keep_best(&out->top, &t->top, 0, car);
        for (int k = 0; k < SEGMENTS; k++)
            keep_best(&out->segment_time[k], &t->segment_time[k], 1, car), keep_best(&out->segment_speed[k], &t->segment_speed[k], 0, car);
    }
    for (int i = 0; i < ROWS && i < count; i++) out->totals[i] = rows[i];
    if (first != NULL)
        for (int k = 0; k < SEGMENTS; k++) out->stage_speeds[k] = first->stage_speeds[k], out->stage_totals[k] = first->stage_totals[k];
    return found;
}

int race_records_cars_lap_merge(int track, int layout, int cheats, const char *const ids[], int n, RecordRow *out)
{
    race_records_row_empty(out);
    int found = 0;
    for (int c = 0; c < n; c++) {
        const RecordRow *const lap = race_records_added_lap_find(track, layout, cheats, ids[c]);
        if (lap == NULL) continue;
        found = 1;
        keep_best(out, lap, 1, added_number(ids[c]));
    }
    return found;
}

/* `a` and `b` the same row but for their car words */
static int same_but_car(const RecordRow *a, const RecordRow *b)
{
    RecordRow x = *a, y = *b;
    memset(x.row + RECORD_ROW_CAR, 0, 4), memset(x.row + RECORD_ROW_CAR_B, 0, 4);
    memset(y.row + RECORD_ROW_CAR, 0, 4), memset(y.row + RECORD_ROW_CAR_B, 0, 4);
    return memcmp(x.row, y.row, sizeof x.row) == 0 && memcmp(&x.marks, &y.marks, sizeof x.marks) == 0;
}

int race_records_row_index(const RecordRow rows[], int n, const RecordRow *row)
{
    for (int i = 0; i < n; i++)
        if (same_but_car(&rows[i], row)) return i;
    return -1;
}

int race_records_edition_cars(Edition *e, const char *ids[], int cap)
{
    if (edition_active(e) != 1) return 0;
    int n = 0;
    for (int i = 0; i < e->cars && n < cap; i++)
        if (edition_car_present(e->car[i])) ids[n++] = e->car[i];
    return n;
}

const RecordTable *race_records_shown(int track, int layout, int cheats, int laps)
{
    static Edition e;
    static RecordTable joined;
    const char *ids[EDITION_LIST_MAX];
    const int n = race_records_edition_cars(&e, ids, EDITION_LIST_MAX);
    if (n == 0) return race_records_find(track, layout, cheats, laps);
    return race_records_cars_merge(track, layout, cheats, laps, ids, n, &joined) ? &joined : NULL;
}

const RecordRow *race_records_shown_lap(int track, int layout, int cheats)
{
    static Edition e;
    static RecordRow joined;
    const char *ids[EDITION_LIST_MAX];
    const int n = race_records_edition_cars(&e, ids, EDITION_LIST_MAX);
    if (n == 0) return race_records_lap_find(track, layout, cheats);
    return race_records_cars_lap_merge(track, layout, cheats, ids, n, &joined) ? &joined : NULL;
}

int race_records_shown_laps(int track, int layout, int cheats, int *out, int max)
{
    static Edition e;
    const char *ids[EDITION_LIST_MAX];
    const int n = race_records_edition_cars(&e, ids, EDITION_LIST_MAX);
    if (n == 0) return race_records_laps(track, layout, cheats, out, max);
    int count = 0;
    for (int laps = 0; laps <= RACE_RECORDS_LAPS_MAX && count < max; laps++)
        for (int c = 0; c < n; c++)
            if (race_records_added_find(track, layout, cheats, laps, ids[c]) != NULL) {
                out[count++] = laps;
                break;
            }
    return count;
}
