/* The race's control panels each frame (0044d640, from 0042270c and the
 * outside view): the replay controls (00450d60, DashArt) outside a live race,
 * the live race's panel (00471364, ReplayArt) while a replay runs or another
 * car is followed. */
#ifndef TNFS_RACE_PANEL_H
#define TNFS_RACE_PANEL_H

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*dash)(void *ctx);    /* 00450d60 */
    void (*replay)(void *ctx);  /* 00471364 */
} RacePanelOps;

typedef struct {
    int32_t unknown_4c5390;     /* 1 once the race runs */
    int16_t replay;             /* 0052b316 */
    int32_t viewed, player;     /* 004fbb04 (the car shown), 004fbb08 (the car followed) */
    int32_t unknown_513e6c;
    int32_t chase_kind;         /* 004f801c */
} RacePanelInputs;

/* The camera each panel shows and its redraw flag. */
typedef struct {
    int32_t dash_view;          /* 00513890 (DashArt.unknown_513888[2]) */
    int32_t dash_redraw;        /* 00513898 (DashArt.unknown_513888[4]) */
    int32_t replay_view;        /* 0052b8dc (ReplayArt.ready) */
    int32_t replay_redraw;      /* 0052b8f4 */
} RacePanelViews;

/* 0044d640: with 004c5390 != 1, a camera kind 0..7 other than the shown one
 * is shown and redrawn, kind 13 shows 3 (redrawn), then 00450d60. With 1:
 * nothing while 0052b316 is 0, 004fbb04 is 004fbb08 and 00513e6c
 * is 0; else a kind 1..5 other than the shown one, or 13 as 3, or 1 when none
 * is shown yet, is shown and redrawn, then 00471364. */
void race_panel(const RacePanelOps *ops, RacePanelViews *v, const RacePanelInputs *in);

#endif
