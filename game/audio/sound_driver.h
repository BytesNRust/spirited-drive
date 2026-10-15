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
    uint32_t mix_rate;              /* 004f38b4 */
    uint32_t rand_seed;             /* the C library's rand() seed, which nothing else draws during a race */
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

/* A sample's header as the driver keeps it in memory (+0 'EACS'), the
 * words 00497784 reads. */
typedef struct {
    uint32_t magic;           /* +0: 'EACS' */
    uint32_t rate;            /* +4 */
    uint8_t sample_size;      /* +8: 2 16-bit */
    uint8_t channels;         /* +9: 2 stereo */
    uint8_t compression;      /* +0xa: 2 compressed */
    int8_t type;              /* +0xb: >= 0 a fixed channel (a header) */
    uint32_t frames;          /* +0xc */
    uint32_t loop_start;      /* +0x10 */
    uint32_t loop_frames;     /* +0x14 */
    const uint8_t *data;      /* +0x18 */
    uint8_t channel;          /* +0x1c: the channel it plays on */
} SoundSample;

/* 00497784 (eax the header): its flags (8 for a sample size of 2, 4 for 2
 * channels, 2 for compression 2, 0x10 for a type >= 0, which also makes
 * the type its channel), the channel's mixed volume (00497750, inline),
 * then the voice started (00497d5b) on the header's channel with its data,
 * frames and loop, its rate, the channel's pan +0x23 and mixed volume
 * +0x22, and the cents 00497724 gives for the channel's pitch +0xe, range
 * +0xf and base +8 (all read from the channel the header named first). */
void sound_sample_start(SoundDriver *d, SoundSample *s);

/* A bank's sound record (the registry 0052eed4[128]), the words 0048eed8
 * reads. */
typedef struct {
    uint32_t mask;            /* +0: the channels it may take */
    SoundSample *sample;      /* +4 */
    int32_t count;            /* +8 */
    int32_t pitch_spread;     /* +0xc: cents */
    int32_t pitch_base;       /* +0x10: cents */
    uint8_t priority;         /* +0x14 */
    uint8_t range;            /* +0x17 */
    uint8_t pan;              /* +0x18 */
    uint8_t volume;           /* +0x19 */
    uint8_t volume_spread;    /* +0x1a */
    uint8_t sound_1c;         /* +0x1c */
    uint8_t pan_spread;       /* +0x1d */
} SoundBankSound;

enum { SOUND_NO_CHANNEL = -20, SOUND_NOT_EACS = -10, SOUND_PAN_OWN = -1 };

#define SOUND_EACS 0x53434145u /* 'EACS' */

/* 0048eed8 (eax the record, edx its number, ebx the pan, ecx the volume,
 * pushed the pitch): a channel for its mask and priority (00497640; -20
 * when none), its header checked ('EACS', else -10), the channel filled:
 * the sound, the pitch, its range and priority, the base cents (+0x10, plus
 * a spread draw of +0xc when either is set), the sound's volume (+0x19 plus
 * a draw of +0x1a, kept to 0..0x7f), the pan (-1: the sound's +0x18, or
 * with a spread +0x1d a draw + 0x40, kept to 0x7f; when the first draw is
 * below that the original draws again and takes the second, unclamped),
 * the volume; the mixed volume (00497750), a handle (004978a0), busy, the
 * sample started on it (00497784); the handle returned. */
int32_t sound_play_record(SoundDriver *d, const SoundBankSound *rec, int32_t id, int32_t pan, int32_t volume,
                          int32_t pitch);

#endif
