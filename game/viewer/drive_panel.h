/* --drive's control panels on the race screen (0044d640's two): the
 * replay's dash controls (00450d60) outside a live race, the live panel
 * (00471364) while another car is shown. Glue, no exe function of its own:
 * each draw call the original makes onto the port's surface. No mouse in
 * --drive: no pointer drawn, and it is never inside a button (00463f80).
 * --drive draws the whole frame each time, so each panel is drawn whole
 * (00513898 / 0052b8f8 set) where the original redraws only what changed. */
#ifndef TNFS_DRIVE_PANEL_H
#define TNFS_DRIVE_PANEL_H

#include "game/gfx/quad.h"
#include "game/race/dash_art.h"
#include "game/race/hud.h"
#include "game/race/race_pointer.h"
#include "game/race/race_replay_seek.h"
#include "game/race/race_state.h"
#include "game/race/replay_panel.h"

#include <stdint.h>

typedef struct {
    ReplayPanel panel;          /* 0052b8d4..0052b8f4 (0052b8f8 is RaceState.view_taken) */
    int32_t dash_drawn_tick;    /* 00513dac */
    RacePointerKept pointer;    /* 00524794 / 96, 004bc6a0 */
} DrivePanel;

/* 0044edd8's panel words at the race's start: the cursor and the view 3,
 * 0052b8ec = 1, 0052b8e8 = 3, 0052b8e0 = 0. */
void drive_panel_start(DrivePanel *p);

/* What the panels draw with */
typedef struct {
    const QuadTarget *screen;   /* the frame (004c2da0 / the default surface) */
    int32_t surface_w, surface_h;
    DashArt *dash;
    const ReplayArt *replay;
    HudState *hud;              /* 0041e794's clock */
    RaceState *race;
    RaceReplayDirector *director;  /* 00513888 / 00513894 / 00513898, 005120c8; NULL with no recording */
    const uint8_t *car_colours;    /* 0050b7d4 */
    uint32_t played, rest;         /* 0050b79c, 0050b7b4 */
} DrivePanelDraw;

/* 00450d60 with DashArt's words kept equal to the director's (00513888,
 * 00513894, 00513898 are one word each in the original). */
void drive_panel_dash(DrivePanel *p, const DrivePanelDraw *d);

/* 00471364 */
void drive_panel_replay(DrivePanel *p, const DrivePanelDraw *d);

#endif
