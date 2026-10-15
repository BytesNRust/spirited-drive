#include "game/audio/sound_bank.h"

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
        read_sound(bank, n, file, size, at);
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
