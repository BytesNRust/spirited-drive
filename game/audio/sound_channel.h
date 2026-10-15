/* The sound driver's channels (the EA library linked into NFS_WIN.EXE,
 * 0049xxxx): the 16 channels 0052f978 that sit above the mixer's voices,
 * and the 100 handles 004c7d94 the game holds for them. */
#ifndef TNFS_SOUND_CHANNEL_H
#define TNFS_SOUND_CHANNEL_H

#include <stdint.h>

/* A channel (0052f978[16], 0x24 bytes), the words the port has read. */
typedef struct {
    const void *sound;     /* +0: the bank's sound record */
    int32_t pitch_base;    /* +8: cents */
    uint8_t kind;          /* +0xd: 1 a bank sample, 2 a stream */
    uint8_t pitch;         /* +0xe: 0x40 the sound's own */
    uint8_t range;         /* +0xf: the pitch range (the sound's +0x17) */
    uint8_t priority;      /* +0x10: the sound's +0x14 */
    uint8_t handle;        /* +0x11 */
    uint8_t busy;          /* +0x12 */
    uint8_t volume;        /* +0x20: the caller's */
    uint8_t sound_volume;  /* +0x21: the sound's, with its spread */
    uint8_t mixed_volume;  /* +0x22 */
    uint8_t pan;           /* +0x23 */
} SoundChannel;

enum { SOUND_CHANNELS = 16, SOUND_HANDLES = 100, SOUND_HANDLE_FREE = -1 };

/* The channels and the handles that name them */
typedef struct {
    SoundChannel channel[SOUND_CHANNELS];  /* 0052f978 */
    int32_t handle[SOUND_HANDLES];         /* 004c7d94: the channel, -1 free */
    uint32_t handle_cursor;                /* 004c7f28 */
    uint32_t channel_cursor;               /* 004c7f24 */
    int32_t open;                          /* 004c7b54: the driver is open */
} SoundChannels;

/* 00487bf8, the C library's rand(): the seed (the thread block [004f3dd8]()
 * + 0xc) x 0x41c64e6d + 0x3039, its bits 16..30. */
int32_t sound_rand(uint32_t *seed);

/* 00497b64 (eax the spread): a draw in -spread..spread: the spread kept to
 * 0..0x10000 (below 0 is 0), x (rand() - 0x4000), / 0x4000 toward 0. */
int32_t sound_spread(uint32_t *seed, int32_t spread);

/* 00497750 (eax the channel): +0x22 = +0x21 x +0x20 / 0x7f. */
void sound_channel_mix_volume(SoundChannel *c);

/* 004978a0 (eax the channel): the first free handle from the cursor on,
 * then from 0 up to it, made the channel's (+0x11, its low byte) and the
 * cursor moved past it (100 -> 0); -1 for a channel outside 0..15 or no
 * free handle (the cursor kept). */
int32_t sound_handle_alloc(SoundChannels *s, int32_t channel);

/* 00497a34 (eax the handle): 0 when the driver is open and the handle
 * (0..99) names a channel (0..15) that is busy (+0x12), else -1. */
int32_t sound_handle_check(const SoundChannels *s, int32_t handle);

/* 00497af8 (eax the handle): the handle's channel when 00497a34 passes it,
 * else NULL. */
SoundChannel *sound_handle_channel(SoundChannels *s, int32_t handle);

/* 00497980 (eax the handle): a handle (0..99) naming a channel (0..15)
 * frees both (+0x12 = 0, the handle -1), 0; else -1. */
int32_t sound_handle_free(SoundChannels *s, int32_t handle);

/* 00497870 (eax the mixer slot; the mixer's stop hook 004f38ec): the
 * slot's channel's handle (+0x11; -1 for a slot outside 0..15) freed
 * (00497980). */
int32_t sound_channel_stopped(SoundChannels *s, int32_t slot);

#endif
