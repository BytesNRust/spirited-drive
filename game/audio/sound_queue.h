/* EA's speech queue (004c7b60..004c7c27): samples played one after another on two channels taken in turn, each
 * started when the one before has a given number of frames left; the menus' voices and fanfares (00464788). */
#ifndef TNFS_SOUND_QUEUE_H
#define TNFS_SOUND_QUEUE_H

#include "game/audio/sound_driver.h"

#include <stdint.h>

/* 0x14 entries are cleared and read, the writer wraps at 0x10 */
enum { SOUND_QUEUE_ENTRIES = 0x14, SOUND_QUEUE_WRITTEN = 0x10 };

typedef struct {
    SoundSample *sample;  /* +0: its header ('EACS'), NULL when cleared */
    uint32_t until;       /* +4: the frames left of the one before when it starts */
} SoundQueueEntry;

typedef struct {
    int32_t ready;                              /* 004c7b60: 00492438 set it up */
    SoundQueueEntry entry[SOUND_QUEUE_ENTRIES]; /* 004c7b64 */
    int32_t write;                              /* 004c7c04 */
    int32_t read;                               /* 004c7c08 */
    int32_t playing;                            /* 004c7c0c */
    int32_t volume;                             /* 004c7c10 */
    int32_t channel_a, channel_b;               /* 004c7c14, 004c7c18 */
    int32_t channel;                            /* 004c7c1c: the next one's */
    int32_t last, handle;                       /* 004c7c20, 004c7c24: the sounds before and now */
} SoundQueue;

/* 00492438 (eax and edx the two channels, ebx the volume): -1 with the driver closed (00492324: 004c7b54) or a
 * channel out of 0..15; else not playing, every entry's sample cleared (004923f0), the channels kept (the next the
 * first), both indexes 0, the volume & 0x7f, ready; 0. */
int32_t sound_queue_setup(const SoundDriver *d, SoundQueue *q, int32_t channel_a, int32_t channel_b, uint32_t volume);

#endif
