/* The replay's seek to a minute (0044e354): the race's state as it was at
 * that minute's mark, then the cars from the recording. */
#ifndef TNFS_RACE_REPLAY_SEEK_H
#define TNFS_RACE_REPLAY_SEEK_H

#include "game/race/race_state.h"

#include <stdint.h>

enum {
    REPLAY_MARK = 0xa2,              /* a mark's bytes (005120d8) */
    REPLAY_MARK_FIRST = 0x10,        /* the marks after the sixteen minutes */
    REPLAY_MARK_COUNT = 4,           /* 00512f84's ticks */
    REPLAY_MARKS = REPLAY_MARK_FIRST + REPLAY_MARK_COUNT
};

typedef struct {
    void *ctx;
    void (*restore_car)(void *ctx, int32_t human, int32_t minute, int32_t index);  /* 0044df20 */
} RaceReplaySeekOps;

/* 0044e354 (eax clock, edx mark; both callers, 0044eb0f and 0044ece8, pass
 * 00512d90 and -1). mark >= 0: the tick from 00512f84[mark], the tick
 * flags from it (1 on even ticks, 4 every 4th, 0x10 every 8th), the
 * minute 0x10 + mark; else the minute clock / 0x708. Then 00450250,
 * 004501a8(tick / 0x708); from the minute's mark: by_node and by_progress
 * for the cars (004fb7e8), the other cars' count (+0x50), 004b9628,
 * 0052b694, 0052b66c (byte +0x99), 004c5fa0, 004c5cd8, 004c5d40,
 * 0052b330, 0052b334, 0052b680, 00509e10, 004b9624 (+0x98 & 1), 0052b478
 * (24 bytes); each car's +0x4ee and +0x4ea zeroed; 004f80fc, 0051c970,
 * 0051c974. Each human's car back (0044df20(1, minute, i); 0051372c = 1
 * first with a mark), the other cars' (0044df20(0, minute, i)); 0051372c
 * = 0. With a mark: racing (004c5390 1): 004501fc; 004c5390 = 2, 00513728
 * = 1. Not racing: the camera follows the player's car in the record's
 * next kind (+4), 7 only while 0043f8f8(0, 5) (slot 2's car, else the
 * player's in kind 5) and 6 while 0043f948(0, 5) (the other of slots 0
 * and 1, else as 7). 00513734 set: cleared, 00443820, then 00444394 when
 * the player's car is in physics 1 or 2, else 0044435c. */
void race_replay_seek(RaceState *race, const RaceReplaySeekOps *ops, int32_t clock, int32_t mark);

typedef struct {
    void *ctx;
    void (*input_tick)(void *ctx, int32_t clock);               /* 004397f4 */
    void (*record)(void *ctx);                                  /* 0044db34 */
    void (*save_car)(void *ctx, int32_t slot, int32_t index);   /* 0044e154 on cars[slot] */
    void (*seek)(void *ctx, int32_t clock, int32_t mark);       /* 0044e354 */
} RaceReplayTickOps;

enum { REPLAY_MARK_EVERY = 0x1c2, REPLAY_INPUT_WAIT = 0x7a0, REPLAY_INPUT_TICKS = 0x78 };

/* 0044e8f8 (no arguments; from 0045009f): 004397f4(004c5590). Then with
 * 004fbb0c set, playback not 1 and (playback 0 or 0052b314 0): before tick
 * 0x78 each human's control word 0x7a0. The countdown (005120d4) 0: it is
 * 0x1c2, 004501fc, and this minute's mark (00512d90 / 0x708) takes what
 * 0044e354 restores (+0x98 and +0x99 whole bytes), then 0044db34, 0044e154
 * on each car with the count of those not human so far, that count to +0x50
 * and 0044e354(00512d90, -1). Else 0044db34. Either way the countdown - 1. */
void race_replay_tick(RaceState *race, const RaceReplayTickOps *ops);

#endif
