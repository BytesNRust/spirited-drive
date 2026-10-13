/* --drive's controls: the race's own input path (0043915c read, 004394dc
 * poll into the player's queue, 004397f4 taking the tick's word), with the
 * keys config.dat gives by default (00426ddc). */
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
    int16_t stand_in[RACE_PLAYERS];   /* 004f82a0 */
    int32_t ticks;                    /* the tick's clock (004bb8e0's role): twice the ticks taken */
} DriveInput;

/* 00426ddc on config.dat's default words for player 0; the player's queue on. */
void drive_input_start(DriveInput *in, RaceState *race);

/* One tick's word for player 0: the keys read, polled into the queue and
 * taken back for the tick. */
uint32_t drive_input_tick(DriveInput *in, RaceState *race);

#endif
