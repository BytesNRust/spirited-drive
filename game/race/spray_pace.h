/* port (rain_fx, spray v4): the pace a car's spray goes by. A car's own `speed` (or, when that is 0, its world velocity: rain_spray_speed) is what the game keeps; a traffic car
 * that waits (held in its pool at the start, parked, or not yet driven) keeps the pace it will have too, so it threw spray every tick from the one spot, a cloud that stays on the road. The pace
 * is therefore capped by how far the car really goes: its place is noted once a SPRAY_PACE_WINDOW ticks and the speed it made over the last window is what it may have, with a little slack
 * (twice it and SPRAY_PACE_SLACK_MS). A car not seen for a window yet is taken as standing. A car that jumped farther than SPRAY_PACE_TELEPORT_M in a window (a traffic car recycled to
 * another place) counts as standing for that window: its trail begins where it lands, not with a burst. Pure: no globals. */
#ifndef TNFS_SPRAY_PACE_H
#define TNFS_SPRAY_PACE_H

#include <stdint.h>

enum {
    SPRAY_PACE_WINDOW = 15,        /* ticks (60 a second): a quarter of a second */
    SPRAY_PACE_TELEPORT_M = 100,   /* metres a window: farther is a jump, not driving */
    SPRAY_PACE_SLACK_MS = 2        /* metres a second the pace may be over twice the measured one */
};

typedef struct {
    int32_t x, z;          /* the place noted (16.16 m) */
    int32_t tick;          /* when */
    int32_t measured;      /* the speed over the last window (16.16 m/s), 0 until one has passed */
    int32_t last_tick;     /* the tick of the last answer, and the answer: the same tick again gives the same pace */
    int32_t last_pace;
    int has_anchor;
} SprayPace;

void spray_pace_clear(SprayPace *pace);

/* The pace (16.16 m/s, never negative) car at (x, z) (16.16 m) may spray with at `tick`, its own pace being `stored` (any sign). A tick that goes back (a new race, a replay) starts again. */
int32_t spray_pace(SprayPace *pace, int32_t tick, int32_t x, int32_t z, int32_t stored);

#endif
