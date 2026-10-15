/* The race's recording for the replay: each human's control word, a byte
 * plane each for the low and the high byte, a slot every 4 ticks. */
#ifndef TNFS_RACE_REPLAY_H
#define TNFS_RACE_REPLAY_H

#include "game/race/car_struct.h"
#include "game/race/race_state.h"

#include <stddef.h>
#include <stdint.h>

enum {
    REPLAY_SLOTS = 0x1c20,           /* a human's slots: 0x7080 ticks of the clock / 4 */
    REPLAY_HIGH = 2 * REPLAY_SLOTS   /* the high bytes' plane, after two humans' low bytes */
};

typedef struct {
    int32_t w4fbb0c;                 /* 004fbb0c */
    int16_t playback;                /* 0052b312 */
    int32_t humans;                  /* 004fb7e0 */
    int32_t clock;                   /* 00512d90 */
} RaceReplayWords;

/* 0044db34 (): with 004fbb0c set and playback not 1, for each human i the
 * control word's low byte (004fb704 + 4i) to recording[i x 0x1c20 + clock / 4]
 * and its next byte 0x3840 further (clock / 4 toward zero). */
void race_replay_record(const RaceReplayWords *w, const uint32_t *inputs, uint8_t *recording, size_t size);

/* 0044dbc8 (; from 0044eced in 0044ecc4), always: for each human i the
 * control word (004fb704 + 4i) back from the recording, recording[i x
 * 0x1c20 + clock / 4] | recording[that + 0x3840] << 8. */
void race_replay_play(const RaceReplayWords *w, uint32_t *inputs, const uint8_t *recording, size_t size);

enum { REPLAY_CAR_SNAPSHOT = 0x168 };

/* 0044dc40 (eax car, edx snapshot): with 004fbb0c set and playback not 1,
 * the car's state the replay starts from: +0 controller (+0x50e), +4
 * physics (+0x512), +8 slot << 24 | the dword at +0x51a << 8, then +0..+0x23,
 * +0x24..+0x47 (the matrix), +0x48, +0x50, +0x54..+0x6b, +0x70..+0x87,
 * +0x8c..+0xbb, +0x174, +0xbc, the body (0x94 bytes where +0xc0 points:
 * +0xc4 in the car), then +0x15c and +0x158 (swapped), +0x33c, +0x4f2. */
void race_replay_save_car(int32_t w4fbb0c, int16_t playback, const Car *car, uint8_t *snapshot);

/* 0044dd9c (eax car, edx snapshot), always: 0044dc40's snapshot back into
 * the car (the slot = +8 >> 24, signed; the dword at +0x51a = +8 >> 8 &
 * 0xffff; the body where +0xc0 points), then +0x405 and +0x409 = 0 and the
 * progress (+0x4c) = (lap (+0x50) - 1) x segments (0050420c) x 4 + node
 * (+0x48), 32-bit. */
void race_replay_load_car(Car *car, const uint8_t *snapshot, int32_t segments);

enum {
    REPLAY_HUMANS_AT = 0x7080,       /* the humans' minutes after the two byte planes */
    REPLAY_HUMAN_MINUTE = 0x1a4,     /* a human's minute: the snapshot and 0x3c bytes more */
    REPLAY_HUMAN = 0x10 * REPLAY_HUMAN_MINUTE,
    REPLAY_OTHERS_AT = REPLAY_HUMANS_AT + 2 * REPLAY_HUMAN,
    REPLAY_OTHER = 0x172,            /* another car's minute: the snapshot and 10 bytes more */
    REPLAY_OTHER_MINUTE = 8 * REPLAY_OTHER,
    REPLAY_SIZE = REPLAY_OTHERS_AT + 0x10 * REPLAY_OTHER_MINUTE
};

/* 0044df20 (eax human, edx minute, ebx index; from 0044e354), always.
 * human != 0: the record at 0x7080 + index x 0x1a4 x 16 + minute x 0x1a4
 * goes into cars[index] through 0044dd9c, then the human's fields after
 * the snapshot (+0x168..+0x1a0: the gear bytes, the dwords, +0x16b - 100,
 * +0x185 - 2, +0x16c split in halves, +0x186's bits), 004c5f98 and
 * 004c5f9c = -1, +0x425 = 0. human 0: the record at 0xa500 + minute x
 * 0xb90 + index x 0x172 into cars[its +8 >> 24], then +0x160, +0x164,
 * +0x3fd (a byte) and the byte +0x340. */
void race_replay_restore_car(RaceState *race, int32_t human, int32_t minute, int32_t index,
                             const uint8_t *recording, size_t size);

/* 0044e154 (eax car, edx index; from 0044eacf, each car of 00509e1c with its
 * index): with 004fbb0c set and playback not 1, the car into this minute
 * (clock / 0x708) of the recording: a human (slot +0x516 < humans) at
 * 0x7080 + slot x 0x1a40 + minute x 0x1a4 through 0044dc40 and the fields
 * 0044df20 reads back (the bytes and the dwords; +0x16c = +0x3c5 << 16 |
 * +0x3c1; +0x186 the four flag bytes as bits 3..0, | 0x20 when +0x426 has
 * bit 1; +0x185 = +0x3ed + 2, +0x16b = +0x3bd + 100, as bytes); another car
 * at 0xa500 + minute x 0xb90 + index x 0x172 through 0044dc40, then +0x160,
 * +0x164, the bytes +0x340 and +0x3fd. */
void race_replay_save_minute(const RaceState *race, const Car *car, int32_t index, uint8_t *recording, size_t size);

enum { REPLAY_RECORDING_SIZE = 0x15e00 };  /* 004c53b4's allocation */

/* 0044edd8's words; the recording's allocator is its one call (00474c8c("..." 004b06c8, 0x15e00, 0)). */
typedef struct {
    int32_t view_taken;              /* 0052b8f8 */
    int32_t split_views[2];          /* 0052b8d4, 0052b8d8 */
    int32_t unknown_52b8e0, unknown_52b8e4, unknown_52b8e8, unknown_52b8ec, unknown_52b8f0;
    int32_t w4fbb0c;                 /* 004fbb0c (read) */
    int16_t playback;                /* 0052b312 (read) */
    uint8_t *recording;              /* 004c53b4 */
    int32_t mark_countdown;          /* 005120d4 */
    int32_t unknown_512f78, unknown_513898, unknown_513718, unknown_5120c4, unknown_5120c8, unknown_512f7c;
    int32_t unknown_513728, unknown_51372c, unknown_51371c, unknown_513894;
    int32_t note_second;             /* 00513714 */
    int32_t unknown_513730, unknown_513888;
    int32_t *notes;                  /* 00512f94: RACE_NOTES words */
    int32_t camera_roll_add;         /* 004c53a8 */
    int32_t unknown_4c5388;
    uint8_t unknown_5137e4;
} RaceReplayStart;

/* 0044edd8 (watcall (), from 00455a5c): 0052b8f8 = 1, 0052b8d4 = 0052b8d8 =
 * 3, 0052b8ec = 1, 0052b8e8 = 3, 0052b8e4 = 1, 0052b8e0 = 0, 0052b8f0 = 4.
 * With 004fbb0c set and playback not 1: the recording allocated (0x15e00),
 * 005120d4 = 0x1c2, 00512f78 = -2, 00513898 = 00513718 = 1, 005120c4 =
 * 005120c8 = 00512f7c = 00513728 = 0051372c = 0051371c = 00513894 =
 * 00513714 = 0, 00513730 = 00513888 = 6, the 0x1e0 notes from 00512f94 0,
 * 004c53a8 = 004c5388 = 0051371c = 0, the byte 005137e4 = 0. */
void race_replay_start(RaceReplayStart *w, uint8_t *(*alloc)(void *ctx, int32_t size), void *ctx);

typedef struct {
    void *ctx;
    void (*save)(void *ctx);                 /* 0044d420: the replay written to its file */
    void (*free)(void *ctx, uint8_t *block); /* 00475260 */
} RaceReplayEndOps;

/* 0044eef8 (watcall (), from 00455a5c's teardown): with 004fbb0c set and
 * playback not 1: save unless the end state (00509d70) is 0x40 or 0x20,
 * then the recording (004c53b4) freed when held (not cleared). */
void race_replay_end(const RaceReplayEndOps *ops, int32_t w4fbb0c, int16_t playback, int32_t end_state,
                     uint8_t *recording);

#endif
