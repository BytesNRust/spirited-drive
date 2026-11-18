/* port (dep59 showcase, owner 2026-11-18): what a car's RECORDS screen shows for each track: the records of one car on one track, taken from the
 * per-car store (race_records_added.h: the stock tracks 0..8) or the tracks' store (race_records_tracks.h: Lunar Springs and the added tracks, by
 * id). A track has many tables for a car, one for each key (layout, cheats, laps); the screen shows one row for the track, so one key is chosen:
 * the standard one (the normal layout, no cheats) when the car has a record there, else the best of what it has; within a class the best total
 * (a lower time; the fewer laps on a tie). The row says which conditions it was, by the marks of the record. Pure data: no screen. */
#ifndef TNFS_CAR_RECORDS_H
#define TNFS_CAR_RECORDS_H

#include "game/sys/race_records.h"

#include <stdint.h>

typedef struct {
    int found;                          /* 1 when the car has any record on the track */
    int layout, cheats, laps;           /* the key the figures are from (laps 0 on a stage) */
    int stage;                          /* 1 for a stage (segments), 0 for a circuit (a best lap) */
    RecordMarks marks;                  /* the conditions of the best total */
    int32_t total;                      /* the best total, in the game's ticks; 0 none */
    int32_t lap;                        /* a circuit's best lap; 0 none */
    int32_t segment[RACE_RECORDS_SEGMENTS];   /* a stage's best time of each segment; 0 none */
    int32_t speed;                      /* the top speed, as the row keeps it; 0 none */
} CarRecord;

/* `car` (the car's id) on the track: `track` 0..8 for a stock one (race_records.h's numbers), else NULL there and `track_id` the added track's or
 * Lunar Springs' id ("lunar"). `stage` is whether the track is a stage. `out` is filled whatever the answer (found 0 for no record) */
void car_records_find(const char *car, int track, const char *track_id, int stage, CarRecord *out);

#endif
