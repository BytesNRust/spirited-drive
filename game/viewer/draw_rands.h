/* port: race_rand taken while drawing (the tyre smoke), counted once a tick. The window draws two frames a tick: the
 * first after a tick draws and logs, the others replay the log. A replaying frame interpolates the cars, so its calls
 * can come in another order or number than the logged ones: a value logged for another n is taken modulo this n, and
 * past the log it gives 0, so a value is always below n and only the logging frame takes from the race. */
#ifndef TNFS_DRAW_RANDS_H
#define TNFS_DRAW_RANDS_H

#include <stdint.h>

enum { DRAW_RANDS = 256 };

typedef struct {
    int32_t value[DRAW_RANDS];
    int32_t n[DRAW_RANDS];
    int count, at;
    int logging;
} DrawRands;

/* a frame starts: `ticked` when a tick ran since the last frame drawn (a new log) */
void draw_rands_frame(DrawRands *d, int ticked);
/* a value in [0, n): logged from `take` on the logging frame, replayed on the others */
int32_t draw_rands_take(DrawRands *d, int32_t n, int32_t (*take)(void *user, int32_t n), void *user);

#endif
