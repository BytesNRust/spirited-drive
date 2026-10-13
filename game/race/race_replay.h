/* The race's recording for the replay: each human's control word, a byte
 * plane each for the low and the high byte, a slot every 4 ticks. */
#ifndef TNFS_RACE_REPLAY_H
#define TNFS_RACE_REPLAY_H

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

#endif
