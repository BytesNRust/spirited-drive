/* The race's recording for the replay: each human's control word, a byte
 * plane each for the low and the high byte, a slot every 4 ticks. */
#ifndef TNFS_RACE_REPLAY_H
#define TNFS_RACE_REPLAY_H

#include "game/race/car_struct.h"

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

#endif
