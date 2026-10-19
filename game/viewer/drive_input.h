/* --drive's controls: the race's own input path (0043915c read, 004394dc
 * poll into the player's queue, 004397f4 taking the tick's word), with the
 * keys config.dat gives (00426ddc). */
#ifndef TNFS_DRIVE_INPUT_H
#define TNFS_DRIVE_INPUT_H

#include "game/race/car_specs.h"
#include "game/race/input_queue.h"
#include "game/race/input_setup.h"
#include "game/race/hud.h"
#include "game/race/race_state.h"

#include <stdint.h>

struct TrackViewerLink;

typedef struct {
    InputQueue queues[RACE_PLAYERS];  /* 004faa7c */
    int32_t codes[INPUT_KEY_CODES];   /* 004fb6dc */
    uint8_t keys[256];                /* 004c693c: a flag per key code */
    int16_t own_stand_ins[1 + RACE_PLAYERS];   /* 004f829e racing alone */
    int16_t *stand_in_block;          /* 004f829e: the kind word (0041019c), then 004f82a0's stand-ins; a link race's are the link's */
    int32_t ticks;                    /* the tick's clock (004bb8e0's role): twice the ticks taken */
    const struct TrackViewerLink *link;  /* a link race's (track_viewer.h), NULL racing alone */
    int32_t *store_clock;             /* 004fb7a0, the run's: the host's store steps it */
    int32_t *late;                    /* 004fb790, the run's: a client's polls sent */
    int32_t *timer;                   /* 004bb8d8, the run's */
    HudState *hud;                    /* 004f9a8c / 004f9a94 / 004f9a98: the network notices */
    int32_t notice_on, notice_until;  /* 004fb7a8, 004fb7a4 */
    const CarSpecs *own_specs;        /* port: test harness only (drive_autopilot): this machine's car's specs */
} DriveInput;

/* 00426ddc on config.dat's control words (its defaults when it was not loaded) for player 0; the player's queue on. */
void drive_input_start(DriveInput *in, RaceState *race);

/* Every key code's flag (004c693c) from the keys held now. */
void drive_input_read_keys(DriveInput *in);

/* One tick's word for player 0 from the keys last read, polled into the
 * queue and taken back for the tick. In a link race (in->link) the word goes
 * to the host, or on the host into its queue and round to the others with
 * every human's (00439b24); each human's word is taken, waiting on the
 * transport for one not in yet, and the others' go to race->inputs. */
uint32_t drive_input_tick(DriveInput *in, RaceState *race);

#endif
