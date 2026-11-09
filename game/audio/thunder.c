#include "game/audio/thunder.h"

#include <stdlib.h>
#include <string.h>

#include "game/race/tick_rate.h"

enum { STEPS_PER_SECOND = RACE_TICKS_A_SECOND };   /* the delay is in race ticks (weather_thunder_delay) */

static uint32_t le16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return le16(p) | le16(p + 2) << 16; }

int thunder_load(Thunder *t, const uint8_t *wav, size_t size, uint32_t rate)
{
    if (t == NULL || wav == NULL || size < 12 || memcmp(wav, "RIFF", 4) != 0 || memcmp(wav + 8, "WAVE", 4) != 0) return 0;
    int ok = 0;
    const uint8_t *data = NULL;
    uint32_t data_size = 0;
    for (size_t at = 12; at + 8 <= size;) {
        const uint32_t n = le32(wav + at + 4);
        const uint8_t *body = wav + at + 8;
        if (n > size - at - 8) break;
        if (memcmp(wav + at, "fmt ", 4) == 0 && n >= 16)
            ok = le16(body) == 1 && le16(body + 2) == 1 && le32(body + 4) == rate && le16(body + 14) == 16;   /* PCM, mono, the mixer's rate, 16 bits */
        else if (memcmp(wav + at, "data", 4) == 0) data = body, data_size = n;
        at += 8 + (size_t)n + (n & 1u);
    }
    if (!ok || data == NULL || data_size < 2) return 0;
    const uint32_t frames = data_size / 2;
    int16_t *pcm = malloc((size_t)frames * sizeof *pcm);
    if (pcm == NULL) return 0;
    for (uint32_t i = 0; i < frames; i++) pcm[i] = (int16_t)le16(data + 2 * (size_t)i);
    free(t->pcm);
    memset(t, 0, sizeof *t);
    t->pcm = pcm, t->frames = frames, t->rate = rate;
    return 1;
}

void thunder_free(Thunder *t)
{
    if (t == NULL) return;
    free(t->pcm);
    memset(t, 0, sizeof *t);
}

void thunder_schedule(Thunder *t, int delay_ticks, int volume_percent)
{
    if (t == NULL || t->pcm == NULL || volume_percent <= 0) return;
    int pick = 0;
    for (int i = 0; i < THUNDER_VOICES; i++) {
        if (!t->voice[i].live) { pick = i; break; }
    }
    if (t->voice[pick].live) {   /* all in use: the one furthest into its sample goes */
        for (int i = 0; i < THUNDER_VOICES; i++)
            if (t->voice[i].at > t->voice[pick].at) pick = i;
    }
    t->voice[pick].wait = (uint32_t)((int64_t)(delay_ticks < 0 ? 0 : delay_ticks) * t->rate / STEPS_PER_SECOND);
    t->voice[pick].at = 0;
    t->voice[pick].volume = volume_percent > 100 ? 100 : volume_percent;
    t->voice[pick].live = 1;
}

void thunder_mix(Thunder *t, int16_t *out, uint32_t frames)
{
    if (t == NULL || t->pcm == NULL) return;
    for (int v = 0; v < THUNDER_VOICES; v++) {
        if (!t->voice[v].live) continue;
        uint32_t i = 0;
        const uint32_t skip = t->voice[v].wait < frames ? t->voice[v].wait : frames;
        t->voice[v].wait -= skip, i = skip;
        if (t->voice[v].wait > 0) continue;
        const int gain = t->voice[v].volume * THUNDER_GAIN_PERCENT / 100;   /* percent */
        for (; i < frames && t->voice[v].at < t->frames; i++, t->voice[v].at++) {
            const int s = t->pcm[t->voice[v].at] * gain / 100;
            for (int c = 0; c < 2; c++) {
                const int m = out[2 * i + (uint32_t)c] + s;
                out[2 * i + (uint32_t)c] = (int16_t)(m > 32767 ? 32767 : m < -32768 ? -32768 : m);
            }
        }
        if (t->voice[v].at >= t->frames) t->voice[v].live = 0;
    }
}

int thunder_active(const Thunder *t)
{
    int n = 0;
    for (int i = 0; t != NULL && i < THUNDER_VOICES; i++) n += t->voice[i].live != 0;
    return n;
}
