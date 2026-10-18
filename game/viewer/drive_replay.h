/* --drive's replay: the race recorded as the original records it (0044edd8,
 * 00450020 on each tick) and played back through the ported director. */
#ifndef TNFS_DRIVE_REPLAY_H
#define TNFS_DRIVE_REPLAY_H

#include "game/race/dash_art.h"
#include "game/race/hud.h"
#include "game/race/race_replay_seek.h"
#include "game/race/race_sim.h"
#include "game/race/race_view.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    RaceReplayDirector dir;
    uint8_t *recording;   /* 004c53b4: REPLAY_RECORDING_SIZE */
    RaceSim *sim;
    RaceViews *views;
    int key_down;         /* the development key's state the frame before */
    int32_t *pace;        /* race_run's copy of 004c53a0 (RaceRunWords), kept equal to the director's; NULL none */
    int32_t *timer;       /* race_run's 004bb8d8 and 004c5594, the director's words too (the controls' Esc waits on */
    int32_t *due;         /* the timer; a seek keeps and puts them back): read in before 00450020, out after; NULL none */
    HudState *hud;        /* 0041e358's 004f9a28 (hud->unknown_4f9a20[2]); NULL none */
    const uint8_t *key_flags;  /* 004c693c (drive_input's keys) for 0044f55c; NULL: the controls take no keys */
    DashArt *dash;        /* the controls' boxes, lit shapes and words 00513888.. (0044f55c); NULL none */
    int32_t last_key;     /* 004c53c4 */
    int32_t last_pressed; /* 004c53cc */
    const uint8_t *file;  /* a .RPL played back (REPLAY_FILE_SIZE bytes, the caller's): 0044d514 reads it; NULL none */
    size_t file_at;       /* 0044d514's reads so far */
    uint32_t seed;        /* 004fb7f4: the race's seed, for the setup 0044d420 writes */
} DriveReplay;

/* 0044edd8 on the sim's race, in a local game (004fbb0c 1): the recording
 * allocated and the replay's words set; the sim's tick then runs 00450020.
 * 0 when the recording could not be allocated (the race runs unrecorded). */
int drive_replay_start(DriveReplay *r, RaceSim *sim, RaceViews *views);

/* A development key standing in for the menus (the in-race menu's replay,
 * 00447564, and the view panel 00471460, are not wired; the controls,
 * 0044f55c, are, through key_flags and dash):
 * pressed while racing, the director's replay from the start (request 2,
 * 0044f188 with 2); pressed during a replay, back to the race where it
 * stood (request 7, 0044f090). `down` is the key's state this frame. */
void drive_replay_key(DriveReplay *r, int down);

/* 0044eef8 at the race's end: Replay.rpl written to the player's directory (0044d420), then the recording freed (the file 0044d420
 * writes is left to the menus that read it back). */
void drive_replay_end(DriveReplay *r);

#endif
