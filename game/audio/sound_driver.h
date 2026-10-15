/* The sound driver (the EA library linked into NFS_WIN.EXE): the channels
 * and their handles over the mixer's voices, one voice per channel. */
#ifndef TNFS_SOUND_DRIVER_H
#define TNFS_SOUND_DRIVER_H

#include "game/audio/sound_channel.h"
#include "game/audio/sound_mix.h"

typedef struct {
    SoundChannels channels;
    SoundMixer mixer;               /* its stop hook 00497870 on `channels` */
    MixVoice voice[SOUND_VOICES];   /* 004f38f0[] */
} SoundDriver;

/* The mixer's voices and its stop hook (00497bb5 sets 004f38ec to
 * 00497870). */
void sound_driver_wire(SoundDriver *d);

/* 00473c68 (eax the handle): a handle that 00497af8 passes stops its
 * channel's voice (00497a1c the slot, 00497e6a, whose hook frees the
 * handle), 0; else -1. A stream's channel (+0xd == 2) also resets the
 * stream queue (0048e018), which the port does not have (no stream plays
 * during a race). */
int32_t sound_stop(SoundDriver *d, int32_t handle);

/* 00497640 (eax the channel mask, edx the priority): from the cursor
 * 004c7f24 round, the first channel in the mask that is free (+0x12 0);
 * else the first whose priority (+0x10) is at most `priority`, its sound
 * stopped (its handle +0x11 through 00473c68); the cursor moved past it
 * (16 -> 0) and the channel returned; -20 when none. */
int32_t sound_channel_pick(SoundDriver *d, uint32_t mask, int32_t priority);

#endif
