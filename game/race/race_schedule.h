/* The race's two deferred calls (004f8100, 004b90dc): the in-race menu
 * (00447564, Esc) and the pause (00455600, P), each due at a frame. */
#ifndef TNFS_RACE_SCHEDULE_H
#define TNFS_RACE_SCHEDULE_H

#include "game/race/input_queue.h"

#include <stdint.h>

enum {
    RACE_SCHEDULE_MENU = 0,     /* 004b90dc[0]: 00447564 */
    RACE_SCHEDULE_PAUSE = 1,    /* 004b90dc[1]: 00455600 */
    RACE_SCHEDULE_RUN = 2,      /* the entries 00410b04 looks at */
    RACE_SCHEDULE_SLOTS = 3     /* the entries 00410a70 sets (004b90dc[2] is 0) */
};

typedef struct {
    int16_t frame;              /* 004f8100 + 4 x i: due at this frame, 0 none */
    int16_t arg;                /* 004f8102 + 4 x i: set by 00410a70, not read by 00410b04 */
} RaceScheduled;

typedef struct {
    void *ctx;
    void (*call)(void *ctx, int which);  /* 004b90dc[which] (both keep every register) */
} RaceScheduleOps;

typedef struct {
    int32_t w4fb7d4;            /* 004fb7d4: 1 sends the entry to the other machine */
    int32_t w4fbb0c;            /* 004fbb0c */
    int16_t playback;           /* 0052b312 */
} RaceScheduleWords;

/* 00410a70 (watcall (frame, which, arg), from 004557f4): which 0..2 (signed),
 * else nothing: the entry's frame and arg set. With 004fb7d4 == 1: 00445e00
 * (0, {3, which, arg, frame + 8}, 8), then the entry's frame + 600 when
 * 004fbb0c is 0 and 0052b312 is 1, else frame + 8 (the message's word read
 * back). Shorts throughout. */
void race_schedule_set(const InputSendOps *send, RaceScheduled sched[RACE_SCHEDULE_SLOTS], const RaceScheduleWords *w,
                       int16_t frame, int32_t which, int16_t arg);

/* 00410b04 (watcall (frame), from 0045597c each frame): for the menu and
 * then the pause, when frame (a short) is not 0 and the entry's frame is not
 * 0 and at most frame (signed): its call, then its frame = 0. The 1 left in
 * eax is not read by 0045597c. */
void race_schedule_run(const RaceScheduleOps *ops, RaceScheduled sched[RACE_SCHEDULE_SLOTS], int16_t frame);

#endif
