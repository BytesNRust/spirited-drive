#include "game/audio/splash.h"

#include "game/audio/wet_roll.h"

#include <string.h>

enum { ONE_FRAME = 65536, GAIN_FULL = 0x7f };

int splash_load(Splash *s, const uint8_t *wav, size_t size, uint32_t rate)
{
    if (s->clips >= SPLASH_CLIPS) return 0;
    if (!thunder_load(&s->clip[s->clips], wav, size, rate)) return 0;
    s->clips++;
    return 1;
}

void splash_free(Splash *s)
{
    for (int i = 0; i < s->clips; i++) thunder_free(&s->clip[i]);
    memset(s, 0, sizeof *s);
}

/* the frames a voice has left */
static uint64_t left(const Splash *s, int v) { return ((uint64_t)s->clip[s->voice[v].clip].frames << 16) - s->voice[v].position; }

void splash_play(Splash *s, int clip, int percent, int pitch_permille, int pan)
{
    if (clip < 0 || clip >= s->clips || percent <= 0 || s->clip[clip].frames < 2) return;
    int v = 0;
    for (int i = 0; i < SPLASH_VOICES; i++) {
        if (!s->voice[i].live) {
            v = i;
            break;
        }
        if (left(s, i) < left(s, v)) v = i;
    }
    const int p = percent > SPLASH_MAX_PERCENT ? SPLASH_MAX_PERCENT : percent, rate = pitch_permille < 100 ? 100 : pitch_permille > 4000 ? 4000 : pitch_permille;
    s->voice[v].clip = clip, s->voice[v].position = 0, s->voice[v].live = 1;
    s->voice[v].advance = (uint32_t)((uint64_t)rate * ONE_FRAME / 1000);
    for (int c = 0; c < 2; c++) s->voice[v].gain[c] = p * wet_roll_pan_gain(pan, c);
}

void splash_quiet(Splash *s)
{
    for (int v = 0; v < SPLASH_VOICES; v++) s->voice[v].live = 0;
}

void splash_mix(Splash *s, int16_t *out, uint32_t frames)
{
    int32_t bus[2 * 256];
    for (uint32_t done = 0; done < frames;) {
        const uint32_t n = frames - done < 256 ? frames - done : 256;
        memset(bus, 0, sizeof bus);
        splash_mix32(s, bus, n);
        for (uint32_t i = 0; i < 2 * n; i++) {
            const int32_t sum = out[2 * done + i] + bus[i];
            out[2 * done + i] = (int16_t)(sum > 32767 ? 32767 : sum < -32768 ? -32768 : sum);
        }
        done += n;
    }
}

void splash_mix32(Splash *s, int32_t *out, uint32_t frames)
{
    for (int v = 0; v < SPLASH_VOICES; v++) {
        if (!s->voice[v].live) continue;
        const Thunder *const t = &s->clip[s->voice[v].clip];
        for (uint32_t i = 0; i < frames; i++) {
            const uint32_t at = (uint32_t)(s->voice[v].position >> 16);
            if (at + 1 >= t->frames) {
                s->voice[v].live = 0;
                break;
            }
            const uint32_t frac = (uint32_t)(s->voice[v].position & 0xffff);
            const int32_t sample = t->pcm[at] + (int32_t)(((int64_t)(t->pcm[at + 1] - t->pcm[at]) * frac) >> 16);
            for (int c = 0; c < 2; c++) out[2 * i + (uint32_t)c] += (int32_t)((int64_t)sample * s->voice[v].gain[c] / (100 * GAIN_FULL));
            s->voice[v].position += s->voice[v].advance;
        }
    }
}

int splash_active(const Splash *s)
{
    int n = 0;
    for (int v = 0; v < SPLASH_VOICES; v++) n += s->voice[v].live;
    return n;
}
