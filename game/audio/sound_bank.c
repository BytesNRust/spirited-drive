#include "game/audio/sound_bank.h"

#include <stdint.h>

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int fits(uint32_t offset, uint32_t length, size_t size)
{
    return offset <= size && length <= size - offset;
}

static int bank_slot(const SoundDriver *d)
{
    for (int i = 0; i < SOUND_BANK_SLOTS; i++)
        if (d->banks[i] == NULL) return i;
    return -1;
}

enum { SAMPLE_WIDE = 2, PACKED_BLOCK = 0x14 };   /* sound_driver.c's 2 (16-bit, stereo, compressed); sound_mix.c's BLOCK_DATA */

/* port: the sample's frames and loop cut to the `room` bytes from its data to the file's end, where the original reads on past a
 * damaged or hostile bank's (fuzz/sound_bank.c). 0 when there is not one frame to play (no frames: the mixer's end would wrap), or a
 * compressed sample's own block counts more than is there. A sample that fits is kept as it is. Five of the CD's run past their file
 * too, where the port read past its buffer: COLL_SB.BNK's 43, 45, 56 and 58 (their loop, by 405 frames), OSUPMB3D.BNK's 70 and
 * TESTBANK.BNK's 3 (a loop of 64930 frames on 3014); they now end at the file's end. */
static int sample_within(SoundSample *s, size_t room)
{
    const uint64_t channels = s->channels == SAMPLE_WIDE ? 2 : 1;
    uint64_t most;
    if (s->compression == SAMPLE_WIDE && s->type >= 0) {   /* a block: its count first (sound_mix.c voice_packed) */
        if (room < PACKED_BLOCK) return 0;
        const uint64_t count = u32(s->data);
        if (count == 0 || PACKED_BLOCK + (count * channels + 1) / 2 > room) return 0;
        if (s->loop_frames == 0) return 1;
    }
    /* a looped block is played as its bytes are, not decoded (sound_mix.c voice_span): its frames and loop are cut as a plain
     * sample's */
    if (s->compression == SAMPLE_WIDE && s->type < 0) most = room * 2 / channels;   /* 4 bits a sample */
    else most = room / (channels * (s->sample_size == SAMPLE_WIDE ? 2 : 1));
    if (most > UINT32_MAX) most = UINT32_MAX;
    if (s->frames > most) s->frames = (uint32_t)most;
    if (s->frames == 0) return 0;
    if (s->loop_start >= s->frames) s->loop_frames = 0;
    else if (s->loop_frames > s->frames - s->loop_start) s->loop_frames = s->frames - s->loop_start;
    return 1;
}

/* A record and its header, checked against the file's size */
static int read_sound(SoundBank *bank, int n, const uint8_t *file, size_t size, uint32_t at)
{
    if (!fits(at, SOUND_BANK_RECORD, size)) return 0;
    const uint8_t *r = file + at;
    const uint32_t header = u32(r + 4);
    if (!fits(header, SOUND_BANK_HEADER, size)) return 0;
    const uint8_t *h = file + header;
    const uint32_t data = u32(h + 0x18);
    if (data > size) return 0;
    bank->sample[n] = (SoundSample){ u32(h), u32(h + 4), h[8], h[9], h[0xa], (int8_t)h[0xb], u32(h + 0xc),
                                     u32(h + 0x10), u32(h + 0x14), file + data, h[0x1c] };
    if (!sample_within(&bank->sample[n], size - data)) return 0;
    bank->sound[n] = (SoundBankSound){ u32(r), &bank->sample[n], (int32_t)u32(r + 8), (int32_t)u32(r + 0xc),
                                       (int32_t)u32(r + 0x10), r[0x14], r[0x17], r[0x18], r[0x19], r[0x1a],
                                       r[0x1c], r[0x1d] };
    return 1;
}

static int bank_fits(const uint8_t *file, size_t size)
{
    if (size < 4 * SOUND_BANK_SOUNDS) return 0;
    for (int n = 0; n < SOUND_BANK_SOUNDS; n++) {
        const uint32_t at = u32(file + 4 * n);
        if (at == 0) continue;
        if (!fits(at, SOUND_BANK_RECORD, size)) return 0;
        const uint32_t header = u32(file + at + 4);
        if (!fits(header, SOUND_BANK_HEADER, size) || u32(file + header + 0x18) > size) return 0;
    }
    return 1;
}

/* 0048ed30 */
int32_t sound_bank_register(SoundDriver *d, SoundBank *bank, const uint8_t *file, size_t size)
{
    if (file == NULL || !bank_fits(file, size)) return SOUND_BANK_NONE;
    const int slot = bank_slot(d);
    if (slot < 0) return SOUND_BANK_TAKEN;
    for (int n = 0; n < SOUND_BANK_SOUNDS; n++) bank->has[n] = u32(file + 4 * n) != 0;
    for (int n = 0; n < SOUND_BANK_SOUNDS; n++) {
        const uint32_t at = u32(file + 4 * n);
        if (at == 0) continue;
        if (d->registry[n] != NULL) return SOUND_BANK_TAKEN;
        if (!read_sound(bank, n, file, size, at)) {   /* port: nothing there to play (sample_within): the number stays free */
            bank->has[n] = 0;
            continue;
        }
        d->registry[n] = &bank->sound[n];
    }
    d->banks[slot] = bank;
    return slot;
}

/* 0048edfc */
int32_t sound_bank_unregister(SoundDriver *d, int32_t slot)
{
    if (slot == SOUND_BANK_ALL) {
        for (int i = 0; i < SOUND_BANK_SLOTS; i++) sound_bank_unregister(d, i);
        return 0;
    }
    if (slot < 0 || slot >= SOUND_BANK_SLOTS || d->banks[slot] == NULL) return SOUND_BANK_NOT_HERE;
    const SoundBank *bank = d->banks[slot];
    for (int n = 0; n < SOUND_BANK_SOUNDS; n++)
        if (bank->has[n]) d->registry[n] = NULL;
    d->banks[slot] = NULL;
    return 0;
}
