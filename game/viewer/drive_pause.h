/* --drive's deferred calls and their keys: 00410b04 and 004557f4 each frame
 * before 004381f8, the pause (00455600) when P schedules it. */
#ifndef TNFS_DRIVE_PAUSE_H
#define TNFS_DRIVE_PAUSE_H

#include "game/race/race_schedule.h"
#include "game/race/race_state.h"
#include "viewer/drive_hud.h"
#include "viewer/drive_input.h"

#include <stdint.h>

typedef struct {
    RaceScheduled sched[RACE_SCHEDULE_SLOTS];  /* 004f8100 */
    RaceMenuKeysState keys;                    /* 00513e50, 00513e54, 0052b314, 00509d70 */
    uint32_t paused_ms;     /* the time the last pause took: the timer stands still while 004fcdac is set (0043b970) */
    int quit;               /* the window closed during the pause */
    void (*menu)(void *user);  /* 004b90dc[0], the in-race menu (00447564); NULL none */
    void *menu_user;
    uint8_t detail;          /* 004fb785, the race's: set before each frame */
    int32_t detail_pending;  /* 004c5360: the draw distance the menu's Esc asks for, -1 none (the frame takes it) */
} DrivePause;

void drive_pause_start(DrivePause *p);

/* 00410b04 then 004557f4 at `frame` (0045597c's order), on in->keys. */
void drive_pause_frame(DrivePause *p, const DriveInput *in, RaceState *race, DriveHud *hud, int16_t frame);

#endif
