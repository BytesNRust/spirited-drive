/* The sound driver's mixer (the EA library linked into NFS_WIN.EXE, 0049xxxx):
 * per voice, a resampling step and the left / right gains it mixes with. */
#ifndef TNFS_SOUND_MIX_H
#define TNFS_SOUND_MIX_H

#include "game/audio/sound_adpcm.h"
#include "game/audio/sound_stream.h"

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
typedef struct MixVoice MixVoice;

/* 004eb8ac..004f38ac: 0x8000 bytes */
enum { SOUND_DECODE_SAMPLES = 0x4000 };

struct MixVoice {
    uint8_t on;            /* +0: 1 while it plays */
    uint8_t looped;        /* +1 */
    uint8_t kind;          /* +2: 2 compressed (no CD bank is) */
    uint8_t has_header;    /* +3 */
    uint32_t flags;        /* +4: 8 16-bit, 4 stereo, 2 compressed, 0x10 a fixed channel */
    uint32_t shift;        /* +8: log2 of the bytes per frame */
    const uint8_t *data;   /* the sample's first byte */
    uint32_t rate;         /* +0xc: Hz */
    uint32_t end;          /* +0x10: the last frame */
    uint32_t pos;          /* +0x14 */
    uint32_t frac;         /* +0x18 */
    uint32_t loop_start;   /* +0x1c */
    uint32_t loop_end;     /* +0x20 */
    SoundStep step;        /* +0x24..+0x2c */
    uint32_t index;        /* +0x30: compressed: the decoder's step index(es) */
    uint32_t sample;       /* +0x34: the decoder's predictor(s) */
    uint32_t src_pos;      /* +0x38: the source's position, nibbles (mono) or bytes (stereo) */
    uint32_t src_frac;     /* +0x3c */
    const uint8_t *src;    /* +0x40: the compressed data */
    uint32_t src_loop_start; /* +0x44 */
    uint32_t src_loop_end; /* +0x48 */
    uint32_t src_end;      /* +0x4c */
    uint32_t saved_sample; /* +0x50: +0x34 at the loop's start */
    uint32_t saved_index;  /* +0x54 */
    SoundDecoder *decode;  /* +0x58 */
    int16_t *buffer;       /* 004eb8ac: what a compressed span decodes into, SOUND_DECODE_SAMPLES */
    SoundStream *stream;   /* a streamed voice's queue (the original's globals) */
    void (*mix)(MixVoice *v, int32_t *acc, uint32_t count);  /* +0x5c: the format's mixer */
    int32_t (*span)(MixVoice *v, int32_t left);              /* +0x60: frames to the segment's end */
    SoundGains gains;      /* +0x64, +0x68 */
};

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

/* 004983c5 (as 0049808a, a compressed one-shot voice): the frames before
 * the source's end (+0x4c, from +0x38), at most `left` (0 stops it); then
 * the source those frames step over - (n x step) >> 24, plus the carry of
 * the fraction +0x3c gathers and one more while it is not 0 - decoded
 * through +0x58 into the buffer (from nibble +0x38 of +0x40 mono, byte
 * +0x38 stereo; the predictors +0x34 and indices +0x30 carried), +0x38
 * past it and the voice's position the buffer's first frame (+0x18 kept). */
int32_t sound_span_packed(MixVoice *v, int32_t left);

/* 00498198 (as 004983c5, a compressed looped voice): at or past the loop's
 * start (+0x44) no limit (0xffff), less than a frame before it the decoder's
 * state saved (+0x34 / +0x30 into +0x50 / +0x54), further before it at most
 * the frames to it; at the loop's end (+0x48) back to its start with both
 * fractions 0 and the state restored (a loop of no frames: the original
 * spins, the port gives 0); the frames to the loop's end at most
 * `left` and the limit, decoded as 004983c5. */
int32_t sound_span_packed_loop(MixVoice *v, int32_t left);

/* 004982a5 (as 004983c5, a compressed streamed voice): at the source's
 * end, the stream's next block (0048dda8 into +0x38 / +0x4c): its five
 * words - samples (+0x4c), left / right step index (+0x30 = l << 6 |
 * r << 22) and predictors (+0x34 = l & 0xffff | r << 16) - and its data at
 * +0x14 (+0x40, +0x38 0), and again; none (the stream's end): the voice
 * stops, 0. A block of no samples that the queue hands back again (the
 * original spins) gives 0. */
int32_t sound_span_packed_stream(MixVoice *v, int32_t left);

/* 00498119 (as 0049808a, a plain streamed voice): at its last frame (+0x10)
 * the stream's next block (0048dda8 into +0x14 / +0x10): its data the
 * voice's (+0x14 >>= the shift), its last frame the frames given - 1 on
 * from there (+0x18 kept), and again; none (the stream's end): the voice
 * stops, 0. A block of no frames handed back again (the original spins)
 * gives 0. The blocks' data is frame-aligned (the original keeps the
 * address >> shift). */
int32_t sound_span_stream(MixVoice *v, int32_t left);

/* 00499c08 (the mixer's output 004f38e8 for 16-bit stereo; 004f38bc the
 * buffer, 004f38b8 the frames): each accumulator word (004e98ac, left and
 * right) saturated to 16 bits. 0 frames: the original's loop counts from
 * -1 through the whole address space; the port writes nothing. */
void sound_out_s16(const int32_t *acc, int16_t *out, uint32_t frames);

enum { SOUND_VOICES = 16, SOUND_MIX_FRAMES = 0x400 };

/* The mixer's state: the 16 records 004f38f0[], the accumulator 004e98ac
 * and the driver's stop hook 004f38ec (the slot of a one-shot voice that
 * reached its end). */
typedef struct {
    MixVoice *voice[SOUND_VOICES];
    void (*stopped)(void *ctx, uint32_t slot);
    void *ctx;
    int32_t acc[2 * SOUND_MIX_FRAMES];
} SoundMixer;

/* 00497cb2 (cdecl buffer, frames): more than 0x400 frames does nothing (0);
 * else the accumulator cleared (00499d13), each playing voice (+0 == 1) in
 * slot order mixed segment by segment (its span, then its mixer on the
 * span's frames from the first not yet mixed) until the frames are filled
 * or the span gives less than 1, then the output 004f38e8 (00499c08 for
 * 16-bit stereo) into `out`. 0 or fewer frames: the original's clear and
 * output run past their buffers; the port does nothing. */
int sound_mix_frames(SoundMixer *m, int16_t *out, int32_t frames);

/* 00497e6a (cdecl slot): the slot's voice off (+0 = 0), then the stop hook
 * 004f38ec with the slot. */
void sound_voice_stop(SoundMixer *m, uint32_t slot);

/* 00497d5b's arguments (cdecl slot, then these in order) */
typedef struct {
    const uint8_t *data;   /* the first sample byte */
    uint32_t frames;       /* the sample's length */
    uint32_t loop_start;   /* frames from the start */
    uint32_t loop_frames;  /* 0: one-shot */
    uint32_t rate;         /* Hz, +0xc */
    uint32_t pan;
    uint32_t volume;
    uint32_t flags;        /* +4: 8 16-bit, 4 stereo, 2 compressed, 0x10 a header */
    int32_t cents;         /* 0049847d's */
} SoundStart;

/* 00497d5b (cdecl slot, data, frames, loop start, loop frames, rate, pan,
 * volume, flags, cents; 004f38b4 the mix rate): the slot's record reset and
 * filled: the shift from the format, the position the data's first frame,
 * the end frames - 1, the loop from its start for its length (looped when
 * that is not 0), the step (0049847d), the format's mixer and the span
 * (00497f9e), the gains (00497e8a), then +0 = 1. The compressed decoder's
 * words (+0x30..+0x4c, +0x58) and its spans and those of a one-shot with a
 * header (00498119, a stream that asks 0048dda8 for more) are not ported:
 * no CD bank sound is compressed or streamed; such a voice gets no span
 * (NULL) and stays off. */
void sound_voice_start(MixVoice *v, const SoundStart *s, uint32_t mix_rate);

#endif
