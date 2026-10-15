/* --drive's replay: the race recorded as the original records it (0044edd8,
 * 00450020 on each tick) and played back through the ported director. */
#ifndef TNFS_DRIVE_REPLAY_H
#define TNFS_DRIVE_REPLAY_H

#include "game/race/race_replay_seek.h"
#include "game/race/race_sim.h"
#include "game/race/race_view.h"

#include <stdint.h>

typedef struct {
    RaceReplayDirector dir;
    uint8_t *recording;   /* 004c53b4: REPLAY_RECORDING_SIZE */
    RaceSim *sim;
    RaceViews *views;
    int key_down;         /* the development key's state the frame before */
    int32_t *pace;        /* race_run's copy of 004c53a0 (RaceRunWords), kept equal to the director's; NULL none */
} DriveReplay;

/* 0044edd8 on the sim's race, in a local game (004fbb0c 1): the recording
 * allocated and the replay's words set; the sim's tick then runs 00450020.
 * 0 when the recording could not be allocated (the race runs unrecorded). */
int drive_replay_start(DriveReplay *r, RaceSim *sim, RaceViews *views);

/* A development key standing in for the menus (the in-race menu's replay,
 * 00447564, and the replay panel 00471460 / 0044f55c, are the frontend's):
 * pressed while racing, the director's replay from the start (request 2,
 * 0044f188 with 2); pressed during a replay, back to the race where it
 * stood (request 7, 0044f090). `down` is the key's state this frame. */
void drive_replay_key(DriveReplay *r, int down);

/* 0044eef8 at the race's end: the recording freed (the file 0044d420
 * writes is left to the menus that read it back). */
void drive_replay_end(DriveReplay *r);

#endif
