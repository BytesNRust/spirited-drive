/* --drive's controls: the race's own input path (0043915c read, 004394dc
 * poll into the player's queue, 004397f4 taking the tick's word), with the
 * keys config.dat gives (00426ddc). */
#ifndef TNFS_DRIVE_INPUT_H
#define TNFS_DRIVE_INPUT_H

#include "game/race/input_queue.h"
#include "game/race/input_setup.h"
#include "game/race/race_state.h"

#include <stdint.h>

typedef struct {
    InputQueue queues[RACE_PLAYERS];  /* 004faa7c */
    int32_t codes[INPUT_KEY_CODES];   /* 004fb6dc */
    uint8_t keys[256];                /* 004c693c: a flag per key code */
    int16_t stand_in_block[1 + RACE_PLAYERS];  /* 004f829e: the kind word (0041019c), then 004f82a0's stand-ins */
    int32_t ticks;                    /* the tick's clock (004bb8e0's role): twice the ticks taken */
} DriveInput;

/* 00426ddc on config.dat's control words (its defaults when it was not loaded) for player 0; the player's queue on. */
void drive_input_start(DriveInput *in, RaceState *race);

/* Every key code's flag (004c693c) from the keys held now. */
void drive_input_read_keys(DriveInput *in);

/* One tick's word for player 0 from the keys last read, polled into the
 * queue and taken back for the tick. */
uint32_t drive_input_tick(DriveInput *in, RaceState *race);

#endif
