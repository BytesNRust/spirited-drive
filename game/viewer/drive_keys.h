/* --drive's key commands: 004381f8 each frame on the keys last read, its
 * calls the ported functions (F5's music pair aside). */
#ifndef TNFS_DRIVE_KEYS_H
#define TNFS_DRIVE_KEYS_H

#include "game/race/race_keys.h"
#include "game/race/race_state.h"
#include "game/race/race_view.h"
#include "viewer/drive_input.h"

typedef struct {
    RaceKeysState state;
    int32_t detail_pending;   /* 004c5360 (0044c1bc), -1 none: the frame takes it (0044cc74's detail step) */
    int32_t w51371c;          /* 0051371c (0044f9ac) */
    int loaded;               /* the kept words hold the exe's first values (drive_keys_start) */
} DriveKeys;

void drive_keys_start(DriveKeys *k);

/* port: the F3 rotation's Ultra step (race_keys.h port_f3_ultra), and the menu's Ultra (port.cfg view_ultra or
 * TNFS_VIEW_ULTRA) that the race starts on at View Far. Set before the race (track_viewer.c drive_wide_setup). */
void drive_keys_port(int f3_ultra, int view_ultra);
/* the race's start, once its distance is read from the options: Ultra when the menu has it and the View is Far */
void drive_keys_race_begins(DriveKeys *k);

/* 004381f8 on in->keys, with race and the views the follow camera uses. */
void drive_keys_frame(DriveKeys *k, DriveInput *in, RaceState *race, RaceViews *views);

#endif
