/* See car_records.h. */
#include "game/frontend/car_records.h"

#include "game/sys/race_records_added.h"
#include "game/sys/race_records_tracks.h"

#include <string.h>

static int32_t d32(const uint8_t *p) { return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24); }

/* the car's table of a key: the stock tracks' store, or the tracks' store by the track's id */
static RecordTable *table_of(const char *car, int track, const char *track_id, int layout, int cheats, int laps)
{
    return track_id != NULL ? race_records_tracks_find(track_id, layout, cheats, laps, car) : race_records_added_find(track, layout, cheats, laps, car);
}

static RecordRow *lap_of(const char *car, int track, const char *track_id, int layout, int cheats)
{
    return track_id != NULL ? race_records_tracks_lap_find(track_id, layout, cheats, car) : race_records_added_lap_find(track, layout, cheats, car);
}

/* 1 when the key `a` (a candidate with total `ta`, `la` laps) is a better choice than the best so far `b`: the standard class first, then the
 * lower total, then the fewer laps */
static int better(int standard_a, int32_t ta, int la, int standard_b, int32_t tb, int lb)
{
    if (standard_a != standard_b) return standard_a;
    if (ta != tb) return ta < tb;
    return la < lb;
}

void car_records_find(const char *car, int track, const char *track_id, int stage, CarRecord *out)
{
    memset(out, 0, sizeof *out);
    out->stage = stage != 0;
    if (car == NULL || car[0] == '\0') return;
    int best_standard = 0, best_laps = 0;
    const RecordTable *best_table = NULL;
    for (int layout = 0; layout < RACE_RECORDS_LAYOUTS; layout++)
        for (int cheats = 0; cheats < 2; cheats++) {
            const int standard = layout == LAYOUT_NORMAL && cheats == 0;
            const int first = stage ? 0 : 1, last = stage ? 0 : RACE_RECORDS_LAPS_MAX;   /* a stage's table has laps 0 */
            for (int laps = first; laps <= last; laps++) {
                const RecordTable *t = table_of(car, track, track_id, layout, cheats, laps);
                if (t == NULL || !race_records_row_set(&t->totals[0])) continue;
                const int32_t total = d32(t->totals[0].row + RECORD_ROW_TIME);
                if (best_table != NULL && !better(standard, total, laps, best_standard, out->total, best_laps)) continue;
                best_table = t, best_standard = standard, best_laps = laps;
                out->layout = layout, out->cheats = cheats, out->laps = laps, out->total = total, out->marks = t->totals[0].marks;
            }
        }
    if (best_table == NULL) {   /* no total anywhere: a best lap or a speed alone is no row of the track's */
        return;
    }
    out->found = 1;
    if (race_records_row_set(&best_table->top)) out->speed = d32(best_table->top.row + RECORD_ROW_SPEED);
    if (stage) {
        for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++)
            if (race_records_row_set(&best_table->segment_time[k])) out->segment[k] = d32(best_table->segment_time[k].row + RECORD_ROW_TIME);
    } else {
        const RecordRow *lap = lap_of(car, track, track_id, out->layout, out->cheats);
        if (lap != NULL && race_records_row_set(lap)) out->lap = d32(lap->row + RECORD_ROW_TIME);
    }
}
