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

#endif
