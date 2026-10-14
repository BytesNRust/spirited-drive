/* One frame of the race loop (0045597c, from 00455a5c): the services, the
 * deferred calls, the keys, and the tick while the race runs. */
#ifndef TNFS_RACE_FRAME_H
#define TNFS_RACE_FRAME_H

#include "game/race/race_state.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*music)(void *ctx);                    /* 00464240 */
    void (*service)(void *ctx);                  /* 00475c14(0) */
    void (*schedule)(void *ctx, int16_t frame);  /* 00410b04 */
    void (*menu_keys)(void *ctx, int16_t frame); /* 004557f4 */
    void (*keys)(void *ctx);                     /* 004381f8 */
    void (*tick)(void *ctx);                     /* 00440ef0 */
} RaceFrameOps;

/* 0045597c (watcall (frame)): music (00524e63 set) or service; schedule,
 * menu_keys, keys with the frame. Then, with 00513e64 set and 00513e60 0:
 * tick flags (004fcdb0) 1 on an even tick, | 4 every 4th, | 0x10 every 8th
 * (else 0); tick; then, unless phase is 4, or phase 2 with 004c5398 0 and
 * 004c539c at most 0: 004c539c - 1 when above 0, tick + 2. */
void race_frame(const RaceFrameOps *ops, RaceState *race, int16_t music_on, int16_t frame);

#endif
