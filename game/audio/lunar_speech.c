/* port (dep59, Lunar Springs' entry): see lunar_speech.h */
#include "game/audio/lunar_speech.h"

#include <string.h>

const char LUNAR_SPEECH_NAME[] = "lunar.eas";
const char LUNAR_SPEECH_WELCOME[] = "lunarwel.eas";
const char LUNAR_SPEECH_GOLF[] = "lunargol.eas";

enum { AT_RATE = 4, AT_WIDTH = 8, AT_CHANNELS = 9, AT_PACKED = 0xa, AT_FRAMES = 0xc, AT_DATA = 0x18 };

static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }

/* the bytes of a plain EACS sample's data (frames x width x channels, inside the file); 0 for anything else */
static size_t data_bytes(const uint8_t *s, size_t size)
{
    if (s == NULL || size < EACS_HEADER_SIZE || memcmp(s, "EACS", 4) != 0 || s[AT_PACKED] != 0) return 0;
    if (get32(s + AT_DATA) != EACS_HEADER_SIZE || s[AT_WIDTH] == 0 || s[AT_CHANNELS] == 0) return 0;
    const uint64_t bytes = (uint64_t)get32(s + AT_FRAMES) * s[AT_WIDTH] * s[AT_CHANNELS];
    return bytes > 0 && bytes <= size - EACS_HEADER_SIZE ? (size_t)bytes : 0;
}

size_t lunar_speech_join(const uint8_t *a, size_t a_size, const uint8_t *b, size_t b_size, uint32_t gap_ms, uint8_t *out, size_t cap)
{
    const size_t an = data_bytes(a, a_size), bn = data_bytes(b, b_size);
    if (an == 0 || bn == 0 || get32(a + AT_RATE) != get32(b + AT_RATE) || a[AT_WIDTH] != b[AT_WIDTH] || a[AT_CHANNELS] != b[AT_CHANNELS])
        return 0;
    const size_t frame = (size_t)a[AT_WIDTH] * a[AT_CHANNELS];
    const uint64_t gap_frames = (uint64_t)get32(a + AT_RATE) * gap_ms / 1000u;
    const uint64_t frames = (uint64_t)get32(a + AT_FRAMES) + gap_frames + get32(b + AT_FRAMES);
    if (frames > UINT32_MAX) return 0;
    const size_t gap = (size_t)gap_frames * frame, total = EACS_HEADER_SIZE + an + gap + bn;
    if (out == NULL || cap < total) return total;
    memcpy(out, a, EACS_HEADER_SIZE);
    put32(out + AT_FRAMES, (uint32_t)frames);
    memcpy(out + EACS_HEADER_SIZE, a + EACS_HEADER_SIZE, an);
    memset(out + EACS_HEADER_SIZE + an, a[AT_WIDTH] == 1 ? 0x80 : 0, gap);   /* silence: 0 in signed 16-bit PCM, 0x80 in unsigned 8-bit */
    memcpy(out + EACS_HEADER_SIZE + an + gap, b + EACS_HEADER_SIZE, bn);
    return total;
}
