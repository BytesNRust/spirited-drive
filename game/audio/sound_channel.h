/* The sound driver's channels (the EA library linked into NFS_WIN.EXE,
 * 0049xxxx): the 16 channels 0052f978 that sit above the mixer's voices,
 * and the 100 handles 004c7d94 the game holds for them. */
#ifndef TNFS_SOUND_CHANNEL_H
#define TNFS_SOUND_CHANNEL_H

#include <stdint.h>

/* A channel (0052f978[16], 0x24 bytes), the words the port has read. */
typedef struct {
    const void *sound;     /* +0: the bank's sound record */
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

/* 00497750 (eax the channel): +0x22 = +0x21 x +0x20 / 0x7f. */
void sound_channel_mix_volume(SoundChannel *c);

#endif
