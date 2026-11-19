/* See car_records.h. */
#include "game/frontend/car_records.h"

#include "game/sys/race_records_added.h"
#include "game/sys/race_records_tracks.h"

#include <string.h>

static int32_t d32(const uint8_t *p) { return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24); }

/* the car's table of a key: the stock tracks' store, or the tracks' store by the track's id */
static RecordTable *table_of(const char *car, int track, const char *track_id, int laps)
{
    return track_id != NULL ? race_records_tracks_find(track_id, LAYOUT_NORMAL, 0, laps, car) : race_records_added_find(track, LAYOUT_NORMAL, 0, laps, car);
}

static RecordRow *lap_of(const char *car, int track, const char *track_id)
{
    return track_id != NULL ? race_records_tracks_lap_find(track_id, LAYOUT_NORMAL, 0, car) : race_records_added_lap_find(track, LAYOUT_NORMAL, 0, car);
}

void car_records_find(const char *car, int track, const char *track_id, int stage, CarRecord *out)
{
    memset(out, 0, sizeof *out);
    out->stage = stage != 0;
    if (car == NULL || car[0] == '\0') return;
    int32_t total_best = 0;   /* a circuit: the best total of the table with the most laps */
    int total_laps = 0;
    const int first = stage ? 0 : 1, last = stage ? 0 : RACE_RECORDS_LAPS_MAX;   /* a stage's table has laps 0 */
    for (int laps = first; laps <= last; laps++) {
        const RecordTable *const t = table_of(car, track, track_id, laps);
        if (t == NULL) continue;
        if (race_records_row_set(&t->top) && d32(t->top.row + RECORD_ROW_SPEED) > out->speed) out->speed = d32(t->top.row + RECORD_ROW_SPEED);
        if (stage && race_records_row_set(&t->totals[0])) {   /* the whole course's time, the best of the ten */
            const int32_t total = d32(t->totals[0].row + RECORD_ROW_TIME);
            if (out->best == 0 || total < out->best) out->best = total;
        }
        if (!stage && race_records_row_set(&t->totals[0]) && d32(t->totals[0].row + RECORD_ROW_TIME) > 0 &&
            (laps > total_laps || (laps == total_laps && d32(t->totals[0].row + RECORD_ROW_TIME) < total_best)))
            total_best = d32(t->totals[0].row + RECORD_ROW_TIME), total_laps = laps;
    }
    const RecordRow *const lap = stage ? NULL : lap_of(car, track, track_id);
    if (lap != NULL && race_records_row_set(lap) && d32(lap->row + RECORD_ROW_TIME) > 0) out->best = d32(lap->row + RECORD_ROW_TIME), out->laps = 1;
    if (!stage && out->best == 0 && total_best > 0) out->best = total_best, out->laps = total_laps;   /* no lap kept: the longest race's total */
    out->found = out->best > 0 || out->speed > 0;
}
