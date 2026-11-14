/* port (rain_tyre_roll, rain_v2, snow): the weather's sounds (the storm's thunder, the rain's bus, the roll on snow) hushed by the pause and the in-race menu, as the engine's voices
 * go quiet there: a gain that falls from whole to silent in HUSH_MS (a fade, no click), holds silent while the race stands still, and rises back in as long once the race's clock moves
 * again. The mixer takes one gain a frame (hush_gains) and gives it to each of those sounds where it meets the mix; silent, the caller drops what they held (hush_silent). */
#ifndef TNFS_HUSH_H
#define TNFS_HUSH_H

#include <stdint.h>

enum { HUSH_MS = 20, HUSH_UNITY = 65536 };

typedef struct {
    int32_t quiet;   /* 0 heard whole .. HUSH_UNITY silent, eased toward the hush */
    int hushed;
    int32_t tick;    /* the race's tick it was hushed at */
} Hush;   /* all 0: heard, not hushed */

/* The race stands still at `tick` (the pause, the in-race menu): silent within HUSH_MS */
void hush_at(Hush *h, int32_t tick);
/* The race's tick now: hushed and the clock moved (on or back), it wakes (1, the caller drops what its sounds held); 0 otherwise */
int hush_wake(Hush *h, int32_t tick);
/* 1 once hushed and silent */
int hush_silent(const Hush *h);
/* The next `frames` frames' gains (0..HUSH_UNITY) at `rate` frames a second, the ease moved on by them */
void hush_gains(Hush *h, int32_t *gain, uint32_t frames, uint32_t rate);

#endif
