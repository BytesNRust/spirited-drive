/* The race's two deferred calls (004f8100, 004b90dc): the in-race menu
 * (00447564, Esc) and the pause (00455600, P), each due at a frame. */
#ifndef TNFS_RACE_SCHEDULE_H
#define TNFS_RACE_SCHEDULE_H

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

/* 00410b04 (watcall (frame), from 0045597c each frame): for the menu and
 * then the pause, when frame (a short) is not 0 and the entry's frame is not
 * 0 and at most frame (signed): its call, then its frame = 0. The 1 left in
 * eax is not read by 0045597c. */
void race_schedule_run(const RaceScheduleOps *ops, RaceScheduled sched[RACE_SCHEDULE_SLOTS], int16_t frame);

#endif
