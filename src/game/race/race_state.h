/* The race globals the car simulation reads and writes, gathered in one
 * place. Each field names the original's address; tools/probe_car.c maps them
 * back for the oracle. */
#ifndef TNFS_RACE_STATE_H
#define TNFS_RACE_STATE_H

#include "formats/tri.h"

#include <stdint.h>

enum {
    RACE_NOTES = 0x7080 / 0x3c,   /* 00512f94: one per second of a 30 minute clock */
    RACE_TRACK_NAME = 16,
    RACE_PLAYERS = 8,
    RACE_LAP_MARKS = 115,         /* what fits in a 0x1d2-byte player record */
    RACE_SURFACES = 3
};

/* 004fa9b0 + 16 * surface (car +0x4a2), set per track by 0042f734. */
typedef struct {
    int32_t grip;       /* 8.8 */
    int32_t rolling;    /* 8.8, times the car's drag */
    int32_t drag;       /* constant resistance */
    int32_t off_road;   /* copied to car +0x4a6 (4 when set) */
} Surface;

/* 0051cc54 + 0x1d2 * player: the start tick, then the tick each lap ended.
 * Only the lap marks are known so far. */
#pragma pack(push, 1)
typedef struct {
    int32_t unknown_000;
    int32_t lap_marks[RACE_LAP_MARKS];  /* +4: [0] start, [k] end of lap k */
    uint8_t unknown_1d0[2];
} PlayerRecord;
#pragma pack(pop)

typedef struct {
    void *user;
    /* 00444028 -> 00443eac: a sound effect (arguments as the original's) */
    void (*sound)(void *user, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f);
    /* 004646cc + 00464850: a voice sample, "besttime.eas" and the like */
    void (*speech)(void *user, const char *name);
} RaceSounds;

typedef struct {
    const Tri *tri;                 /* 00505038 nodes, 00504208 mask (ring_nodes - 1) */
    char track_name[RACE_TRACK_NAME];  /* 004fb7c6, "tr6" */
    int32_t track;                  /* 004fb7c2: 0..7 */
    int32_t track_variant;          /* 004fb7be */
    int32_t arcade;                 /* 004fbadc */
    Surface surfaces[RACE_SURFACES];  /* 004fa9b0 */
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
    int32_t circuit;                /* 00504200: laps are counted */
    int32_t segments;               /* 0050420c */
    int32_t loop_nodes;             /* 00509d58: nodes in one lap */
    int32_t laps;                   /* 00509d54 */
    int32_t best_lap;               /* 00509e04: ticks */
    int32_t best_lap_set;           /* 00509dfc */
    int32_t viewed;                 /* 004fbb04: the car shown in a replay */
    int16_t replay;                 /* 0052b316 */
    PlayerRecord players[RACE_PLAYERS];  /* 0051cc54 */
    RaceSounds sounds;
} RaceState;

enum {
    /* 004fb7d8 bit 0x20: falling cars skip the extra half pull (00428bd8) and,
     * except on "tr6", come back from long jumps more slowly */
    TRACK_FLAG_LOW_GRAVITY = 0x20
};

/* 0044d744 */
void race_note(RaceState *race, int32_t level);

/* 0042f734 */
void race_surfaces_init(RaceState *race);

/* The road node under a node index, masked like the original (& 00504208). */
TriNode race_node(const RaceState *race, int32_t node);

#endif
