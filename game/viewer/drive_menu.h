/* --drive's in-race menu: 00447564 (race_menu) when Esc schedules it
 * (004557f4, 004b90dc[0]), drawn with the frontend's own layer (video,
 * pointer, keys, the Yes/No box) on the --drive window. Glue. */
#ifndef TNFS_DRIVE_MENU_H
#define TNFS_DRIVE_MENU_H

#include "game/race/race_run.h"
#include "game/race/race_schedule.h"
#include "game/race/race_state.h"
#include "viewer/drive_hud.h"
#include "viewer/drive_replay.h"

#include <stdint.h>

typedef struct {
    RaceState *race;
    DriveReplay *replay;        /* 0044f534's request goes to its director */
    DriveHud *hud;              /* 0041e364 on the way back */
    RaceMenuKeysState *keys;    /* 00513e50 / 00513e54: 004557f4's pending call and its tick */
    RaceRunWords *words;        /* 004c5cca (music_started): the music's small ring */
    void (*music_start)(void *user);  /* 00456244: a race track drawn and played */
    void *music_user;
    int ready;                  /* the frontend's layer is up */
} DriveMenu;

/* After platform_init: the frontend's video, art paths, pointer and font
 * (as 00437540 sets them up). 0 when the menu cannot run (Esc then does
 * nothing). The map is the race's track and variant (004fb7c2 / 004fb7be). */
int drive_menu_open(DriveMenu *m, const char *data_root);

/* 00447564 on the race; the race's 0052b314 / 00509d70 written back. */
void drive_menu_run(void *menu);

void drive_menu_close(DriveMenu *m);

#endif
