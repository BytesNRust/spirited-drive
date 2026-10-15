#include "game/audio/sound_adpcm.h"

enum { STEPS = 89, ROW = 64 };

/* The IMA steps the delta table 004f48a0 is built from */
static const int32_t steps[STEPS] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,    25,    28,
    31,    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,   337,   371,   408,   449,   494,
    544,   598,   658,   724,   796,   876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
    2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,
    9493,  10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

/* 004f5ee0: the index's move by the nibble's low 3 bits, x 64 */
static const int32_t index_move[8] = { -ROW, -ROW, -ROW, -ROW, 2 * ROW, 4 * ROW, 6 * ROW, 8 * ROW };

/* 004f48a0[index / 64][nibble] */
static int32_t delta(uint32_t index, uint32_t nibble)
{
    const int32_t step = steps[index / ROW];
    int32_t d = step >> 3;
    if (nibble & 4) d += step;
    if (nibble & 2) d += step >> 1;
    if (nibble & 1) d += step >> 2;
    return nibble & 8 ? -d : d;
}

static int32_t clamp16(int32_t v) { return v > 0x7fff ? 0x7fff : v < -0x8000 ? -0x8000 : v; }

static uint32_t next_index(uint32_t index, uint32_t nibble)
{
    const int32_t i = (int32_t)index + index_move[nibble & 7];
    return i > SOUND_ADPCM_INDEX_MAX ? SOUND_ADPCM_INDEX_MAX : i < 0 ? 0 : (uint32_t)i;
}

/* 004a553c */
void sound_adpcm_mono(uint32_t first, const uint8_t *src, int16_t *dst, int32_t count, int32_t *sample, uint32_t *index)
{
    const int32_t end = count + (int32_t)first;
    uint32_t at = first, idx = *index;
    int32_t s = (int16_t)*sample;
    do {
        const uint32_t nibble = at & 1 ? *src++ & 0xf : *src >> 4;
        s = clamp16(s + delta(idx, nibble));
        *dst++ = (int16_t)s;
        at++;
        idx = next_index(idx, nibble);
    } while ((int32_t)at < end);
    *sample = s;
    *index = idx;
}

/* 004a55e4 */
void sound_adpcm_stereo(const uint8_t *src, int16_t *dst, int32_t count, uint32_t *sample, uint32_t *index)
{
    int32_t left = (int16_t)*sample, right = (int16_t)(*sample >> 16);
    uint32_t li = *index & 0xffff, ri = *index >> 16;
    int32_t frame = 0;
    do {
        const uint32_t hi = *src >> 4, lo = *src & 0xf;
        left = clamp16(left + delta(li, hi));
        dst[2 * frame] = (int16_t)left;
        li = next_index(li, hi);
        right = clamp16(right + delta(ri, lo));
        dst[2 * frame + 1] = (int16_t)right;
        ri = next_index(ri, lo);
        frame++;
        src++;
    } while (frame < count);
    *sample = (uint32_t)right << 16 | (uint16_t)left;
    *index = ri << 16 | (uint16_t)li;
}
