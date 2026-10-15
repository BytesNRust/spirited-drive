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

#endif
