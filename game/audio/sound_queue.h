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

/* 004924f8 (eax the header, edx a percentage of its rate): -1 with the driver closed, the queue not set up or no
 * header; else its data set to the file's past its 0x20-byte header (+0x18 = +0x20), the header written at the
 * writer (which wraps at 0x10) with until = rate x percent / 100 (unsigned); 0. file is the bytes the header came
 * from. */
int32_t sound_queue_add(const SoundDriver *d, SoundQueue *q, SoundSample *s, const uint8_t *file, uint32_t percent);

/* 004925a8: playing (004c7c0c = 1), unchecked. */
void sound_queue_play(SoundQueue *q);

/* 00492620: with the driver open and the queue set up: not playing, the sounds before and now stopped (00473c68),
 * the queue emptied (004923f8: every entry's sample cleared, both indexes 0, the next channel the first), and the
 * queue no longer set up. (It checks the driver and the queue twice; nothing between changes them.) */
void sound_queue_stop(SoundDriver *d, SoundQueue *q);

/* 00492330 (004a4ac0's, while 004c7b60): while playing, once the sound now has no more frames left (00473cf8,
 * signed) than the entry at the reader asks: with nothing queued past it (reader == writer) playing ends when the
 * sound has stopped (00473d20); else the reader moves on (wrapping at 0x14), the channel turns to the other, the
 * sound now becomes the one before and the entry's sample starts on the channel at pan 0x40 and the queue's
 * volume (004a4bd8; its answer, a handle or below 0, kept). */
void sound_queue_tick(SoundDriver *d, SoundQueue *q);

#endif
