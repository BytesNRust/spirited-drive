#include "formats/wav.h"

#include <stdlib.h>
#include <string.h>

static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }
static uint32_t get16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t get32(const uint8_t *p) { return get16(p) | get16(p + 2) << 16; }

WavResult wav_write(const uint8_t *pcm, const WavInfo *info, uint8_t **out, size_t *out_len)
{
    const size_t frame = (size_t)info->bits / 8 * (size_t)info->channels;
    if ((info->bits != 8 && info->bits != 16) || (info->channels != 1 && info->channels != 2)) return WAV_ERR_UNSUPPORTED;
    const size_t data = info->frames * frame;
    if (data > 0x7FFFFFF0u) return WAV_ERR_UNSUPPORTED;
    const size_t total = 44 + data + (data & 1);
    uint8_t *f = malloc(total);
    if (f == NULL) return WAV_ERR_MEMORY;
    memcpy(f, "RIFF", 4);
    put32(f + 4, (uint32_t)(total - 8));
    memcpy(f + 8, "WAVEfmt ", 8);
    put32(f + 16, 16);
    put16(f + 20, 1);
    put16(f + 22, (uint32_t)info->channels);
    put32(f + 24, info->rate);
    put32(f + 28, info->rate * (uint32_t)frame);
    put16(f + 32, (uint32_t)frame);
    put16(f + 34, (uint32_t)info->bits);
    memcpy(f + 36, "data", 4);
    put32(f + 40, (uint32_t)data);
    if (info->bits == 8) for (size_t i = 0; i < data; ++i) f[44 + i] = (uint8_t)(pcm[i] ^ 0x80);
    else memcpy(f + 44, pcm, data);
    if (data & 1) f[44 + data] = 0;
    *out = f;
    *out_len = total;
    return WAV_OK;
}

WavResult wav_read(const uint8_t *d, size_t len, WavInfo *info, uint8_t **pcm)
{
    if (len < 12 || memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0) return WAV_ERR_FORMAT;
    int have_fmt = 0;
    size_t at = 12;
    while (at + 8 <= len) {
        const uint32_t n = get32(d + at + 4);
        const uint8_t *body = d + at + 8;
        if (memcmp(d + at, "fmt ", 4) == 0 && n >= 16 && at + 8 + 16 <= len) {
            if (get16(body) != 1) return WAV_ERR_UNSUPPORTED;
            info->channels = (int)get16(body + 2);
            info->rate = get32(body + 4);
            info->bits = (int)get16(body + 14);
            if ((info->bits != 8 && info->bits != 16) || (info->channels != 1 && info->channels != 2)) return WAV_ERR_UNSUPPORTED;
            have_fmt = 1;
        } else if (memcmp(d + at, "data", 4) == 0) {
            if (!have_fmt) return WAV_ERR_FORMAT;
            if (n > len - at - 8) return WAV_ERR_TRUNCATED;
            const size_t frame = (size_t)info->bits / 8 * (size_t)info->channels;
            info->frames = n / frame;
            const size_t bytes = info->frames * frame;
            uint8_t *out = malloc(bytes ? bytes : 1);
            if (out == NULL) return WAV_ERR_MEMORY;
            if (info->bits == 8) for (size_t i = 0; i < bytes; ++i) out[i] = (uint8_t)(body[i] ^ 0x80);
            else memcpy(out, body, bytes);
            *pcm = out;
            return WAV_OK;
        }
        if (n > len - at - 8) return WAV_ERR_TRUNCATED;
        at += 8 + n + (n & 1);
    }
    return WAV_ERR_FORMAT;
}

const char *wav_result_str(WavResult result)
{
    switch (result) {
    case WAV_OK: return "ok";
    case WAV_ERR_MEMORY: return "out of memory";
    case WAV_ERR_FORMAT: return "not a RIFF/WAVE file with fmt and data chunks";
    case WAV_ERR_UNSUPPORTED: return "only 8- or 16-bit PCM, mono or stereo, is supported";
    case WAV_ERR_TRUNCATED: return "chunk runs past the end of the file";
    }
    return "unknown error";
}
