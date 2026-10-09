/* The race globals the car simulation reads and writes, gathered in one
 * place. Each field names the original's address; tools/probe_car.c maps them
 * back for the oracle. */
#ifndef TNFS_RACE_STATE_H
#define TNFS_RACE_STATE_H

#include "formats/tri.h"

#include <stdint.h>

enum {
    RACE_NOTES = 0x7080 / 0x3c,   /* 00512f94: one per second of a 30 minute clock */
    RACE_TRACK_NAME = 16
};

typedef struct {
    void *user;
    /* 00444028 -> 00443eac: a sound effect (arguments as the original's) */
    void (*sound)(void *user, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f);
} RaceSounds;

typedef struct {
    const Tri *tri;                 /* 00505038 nodes, 00504208 mask (ring_nodes - 1) */
    char track_name[RACE_TRACK_NAME];  /* 004fb7c6, "tr6" */
    uint32_t track_flags;           /* 004fb7d8 */
    int32_t player;                 /* 004fbb08: the car the camera follows */
    int32_t phase;                  /* 004c5390: 1 while racing */
    int32_t tick;                   /* 00513e70 */
    int32_t clock;                  /* 00512d90: tick % 0x7080 */
    int32_t last_node;              /* 004fa18c */
    int32_t landing_shake;          /* 004f9b74: frames of camera shake after a jump */
    /* 0044d744: the loudest note of each second, (second << 8) | (0x80 + level) */
    int32_t note_second;            /* 00513714 */
    int32_t note_tick;              /* 004c53bc */
    int32_t note_level;             /* 004c53c0 */
    int32_t notes[RACE_NOTES];      /* 00512f94 */
    RaceSounds sounds;
} RaceState;

enum {
    /* 004fb7d8 bit 0x20: falling cars skip the extra half pull (00428bd8) and,
     * except on "tr6", come back from long jumps more slowly */
    TRACK_FLAG_LOW_GRAVITY = 0x20
};

/* 0044d744 */
void race_note(RaceState *race, int32_t level);

/* The road node under a node index, masked like the original (& 00504208). */
TriNode race_node(const RaceState *race, int32_t node);

#endif
