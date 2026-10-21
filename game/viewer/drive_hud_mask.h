/* port: drive_hud.c's 00421c18 (the view presented through the cockpit mask onto the frame), split from it. */
#ifndef TNFS_DRIVE_HUD_MASK_H
#define TNFS_DRIVE_HUD_MASK_H

#include "viewer/drive_hud.h"

enum {
    DRIVE_HUD_HIGH_RES = 1,
    DRIVE_HUD_LIVE_RACE = 1   /* 004c5390: 00455a5c sets 1 once the race runs (0044e700 2 / 3 in replays) */
};

/* 0041e3f4's inputs for a width x height view in a live race at high resolution */
HudViewClipInputs drive_hud_view_clip_inputs(int width, int height);

/* 00421c18 onto `pixels` (width x height); the view's rows below the exe's when `changed` is 0 */
void drive_hud_mask(DriveHud *h, const RaceState *race, uint8_t *pixels, int width, int height, int32_t changed);

#endif
