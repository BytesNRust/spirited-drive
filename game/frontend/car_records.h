/* port (dep59 showcase, owner 2026-11-19): what a car's RECORDS screen shows for each track, from the per-car store (race_records_added.h: the stock
 * tracks 0..8) or the tracks' store (race_records_tracks.h: Lunar Springs and the added tracks, by id). Two figures, read as a magazine would give
 * them, from the normal layout without cheats only (the weather, the hour, the nitro are no key: any race counts):
 *   best:  a circuit's best lap, and where the car has none (the records made before the lap rows: the standard store keeps one lap for the whole track) the
 *          car's best total of its longest race, `laps` saying how long; a stage's total of the whole course (the stage's totals table, kept only once the course is finished: a segment's
 *          own record, kept as each segment ends, is never read);
 *   speed: the top speed, the greatest of the top speeds of every table of that key (a circuit has one table for each lap count).
 * Pure data: no screen. */
#ifndef TNFS_CAR_RECORDS_H
#define TNFS_CAR_RECORDS_H

#include "game/sys/race_records.h"

#include <stdint.h>

typedef struct {
    int found;         /* 1 when the car has a best time or a speed on the track */
    int stage;         /* the track is a stage */
    int32_t best;      /* the game's ticks; 0 none */
    int32_t speed;     /* as the row keeps it; 0 none */
    int laps;          /* a circuit's best that is a race's total, not a lap (no lap row kept): that race's laps; 0 for a lap and for a stage */
} CarRecord;

/* `car` (the car's id) on the track: `track` 0..8 for a stock one (race_records.h's numbers), else NULL there and `track_id` the added track's or
 * Lunar Springs' id ("lunar"). `stage` is whether the track is a stage. `out` is filled whatever the answer (found 0 for no record) */
void car_records_find(const char *car, int track, const char *track_id, int stage, CarRecord *out);

#endif
