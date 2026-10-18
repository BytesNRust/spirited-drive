/* The menu between a race's segments (check/chkmenu.qfs): continue, the replay, the highlights, the options, the
 * results, the chat, quit (0040b648). */
#ifndef TNFS_NEXT_SEGMENT_H
#define TNFS_NEXT_SEGMENT_H

#include "game/frontend/after_race.h"

typedef struct NextSegmentOps {
    AfterRaceOps menu;                                         /* its ctx is the one for these too */
    int32_t (*yes_no)(void *ctx, const char *text);            /* 00465f4c: eax; ax */
    void (*banner)(void *ctx, const char *text);               /* 0046667c: eax */
    void (*deadline)(void *ctx, int32_t ticks);                /* 00410b7c: eax */
    int32_t (*wait_others)(void *ctx, int32_t stage);          /* 00410c18: eax; nonzero when all came */
} NextSegmentOps;

typedef struct NextSegmentState {
    AfterRaceState menu;   /* its link_over (0050a5b6) is cleared through over */
    int16_t *over;         /* the short 0050a5b6, written */
    int16_t *aborted;      /* the short 0052b314: the link game left (quit here, or the others lost) */
    int16_t german;        /* the short 0052b31c */
} NextSegmentState;

/* 0040b648 (all registers kept): eax the result, 1 to go on (Continue, Escape, a link game's tournament over), 0
 * to stop (Quit answered yes, a link game aborted). Seven items over the pointer's bands (Continue; the replay and
 * the highlights not in a link game; the options; the results; the chat only in one; Quit). No deadline leaves it.
 * A link game going on waits for the others ("Waiting for other players"); none coming, 0052b314 is set and the
 * result 0. */
int32_t next_segment_menu(NextSegmentState *st, const NextSegmentOps *ops);

#endif
