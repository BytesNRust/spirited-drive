/* The sound driver's mixer (the EA library linked into NFS_WIN.EXE, 0049xxxx):
 * per voice, a resampling step and the left / right gains it mixes with. */
#ifndef TNFS_SOUND_MIX_H
#define TNFS_SOUND_MIX_H

#include <stdint.h>

/* 0049847d's three words in the voice's mixer record (+0x24, +0x28, +0x2c). */
typedef struct {
    uint32_t whole;     /* +0x24: output samples per input sample, the integer part, & 0xff */
    uint32_t fraction;  /* +0x28: the fraction, 24 bits << 8 */
    uint32_t step;      /* +0x2c: whole << 24 | fraction >> 8 (8.24) */
} SoundStep;

/* 0049847d (eax rate, ebx cents; esi the mixer record, 004f38b4 the mix
 * rate): the frequency rate x 2^(cents / 1200) (the table 004c7f2c, one
 * octave of 1200 steps from 0x10000, shifted right by 9 and one more per
 * octave below 0x40d0 - 0x2000 cents), stepped against the mix rate. Cents
 * past the table's top (0x4580 - 0x2000 and up) read the zeros that follow it
 * (frequency 0). A mix rate of 0 gives 0 (the original divides by 0). */
SoundStep sound_voice_step(uint32_t rate, int32_t cents, uint32_t mix_rate);

/* 00497724 (watcall pitch, range, base; from 00473c08 with the voice's
 * range +0xf and base +8): (pitch - 0x40) x range x 100 / 64, rounded
 * toward 0, plus base: the cents 0049847d takes. */
int32_t sound_pitch_cents(int32_t pitch, int32_t range, int32_t base);

/* 00497e8a (pushed: the mixer slot, pan, volume; from 00473b4c, 00473bb0 and
 * the voice start with the voice's +0x23 and +0x22): the mixer record's
 * +0x64 (left) and +0x68 (right) gains, << 10. Pan 0x40 both full (0x7f);
 * below it the right is pan x 2, above it the left (0x7f - pan) x 0x7f /
 * 0x3e (32-bit unsigned, so pans past 0x7f wrap); each x volume / 0x7f. */
typedef struct {
    uint32_t left;   /* +0x64 */
    uint32_t right;  /* +0x68 */
} SoundGains;

SoundGains sound_pan_gains(uint32_t pan, uint32_t volume);

/* A mixer voice record (004f38f0[slot], 0x70 bytes), the words the port
 * keeps. Positions are frames: the original keeps the sample's address
 * shifted right by `shift`, the port the frame from `data`. */
typedef struct {
    uint8_t on;            /* +0: 1 while it plays */
    uint8_t looped;        /* +1 */
    uint8_t kind;          /* +2: 2 compressed (no CD bank is) */
    uint8_t has_header;    /* +3 */
    uint32_t flags;        /* +4: 8 16-bit, 4 stereo, 2 compressed, 0x10 a fixed channel */
    uint32_t shift;        /* +8: log2 of the bytes per frame */
    const uint8_t *data;   /* the sample's first byte */
    uint32_t end;          /* +0x10: the last frame */
    uint32_t pos;          /* +0x14 */
    uint32_t frac;         /* +0x18 */
    uint32_t loop_start;   /* +0x1c */
    uint32_t loop_end;     /* +0x20 */
    SoundStep step;        /* +0x24..+0x2c */
    SoundGains gains;      /* +0x64, +0x68 */
} MixVoice;

/* 00498552 (esi the record; 004f38c8 the first frame of the accumulator
 * 004e98ac, 004f38cc the count): `count` frames of an 8-bit mono voice
 * added into the accumulator's left / right words through the volume table
 * 004c91ec (built by 004984f6: (int8) sample x 2 x row, the row the gain
 * >> 10), the position stepped by whole and fraction (the fraction's carry
 * into the frame); the position and fraction written back. */
void sound_mix_mono8(MixVoice *v, int32_t *acc, uint32_t count);

/* 00498896 (as 00498552, 16-bit mono): each frame's signed 16-bit sample x
 * (gain >> 10) >> 7 (arithmetic) into left and right. */
void sound_mix_mono16(MixVoice *v, int32_t *acc, uint32_t count);

/* 00498c44 (as 00498552, 8-bit stereo): the frame's first byte through the
 * left gain, its second through the right. */
void sound_mix_stereo8(MixVoice *v, int32_t *acc, uint32_t count);

/* 00499052 (as 00498896, 16-bit stereo; 004f38b0 is 0): the frame's first
 * sample through the left gain, its second through the right. */
void sound_mix_stereo16(MixVoice *v, int32_t *acc, uint32_t count);

/* 0049808a (esi the record, 004f38c4 the frames the mix still needs): the
 * frames a one-shot voice plays before its last frame (+0x10), at most
 * `left`; 0 stops it (00497e6a: +0 cleared, then the driver's stop hook
 * 004f38ec, which the caller runs). */
int32_t sound_span_once(MixVoice *v, int32_t left);

/* 004980d4 (as 0049808a, a looped voice): the frames before the loop's end
 * (+0x20), at most `left`; at it, back to the loop's start (+0x1c) with the
 * fraction 0 and again (a loop of no frames: the original spins, the port
 * gives 0). */
int32_t sound_span_loop(MixVoice *v, int32_t left);

/* 00499c08 (the mixer's output 004f38e8 for 16-bit stereo; 004f38bc the
 * buffer, 004f38b8 the frames): each accumulator word (004e98ac, left and
 * right) saturated to 16 bits. 0 frames: the original's loop counts from
 * -1 through the whole address space; the port writes nothing. */
void sound_out_s16(const int32_t *acc, int16_t *out, uint32_t frames);

#endif
