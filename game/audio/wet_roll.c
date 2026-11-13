#include "game/audio/wet_roll.h"

#include <string.h>

enum { EASE_SECONDS_DIV = 10, UNIT = 256, ONE_FRAME = 65536 };   /* a change takes a tenth of a second */

int wet_roll_load(WetRoll *w, const uint8_t *wav, size_t size, uint32_t rate)
{
    memset(w, 0, sizeof *w);
    w->pitch = 1000;
    return thunder_load(&w->sample, wav, size, rate);
}

void wet_roll_free(WetRoll *w)
{
    thunder_free(&w->sample);
    memset(w, 0, sizeof *w);
}

void wet_roll_set(WetRoll *w, int percent, int pitch_permille)
{
    w->target = (percent < 0 ? 0 : percent > WET_ROLL_MAX_PERCENT ? WET_ROLL_MAX_PERCENT : percent) * UNIT;
    w->idle = 0;
    w->pitch = pitch_permille < 100 ? 100 : pitch_permille > 4000 ? 4000 : pitch_permille;
}

void wet_roll_mix(WetRoll *w, int16_t *out, uint32_t frames)
{
    const Thunder *const t = &w->sample;
    if (t->pcm == NULL || t->frames < 2 || (w->level == 0 && w->target == 0)) return;
    const int step = 100 * UNIT / (int)(t->rate / EASE_SECONDS_DIV) + 1;
    const uint64_t length = (uint64_t)t->frames * ONE_FRAME;
    const uint32_t advance = (uint32_t)((uint64_t)w->pitch * ONE_FRAME / 1000);
    for (uint32_t i = 0; i < frames; i++) {
        if (w->idle < t->rate) w->idle++;
        if (w->idle > t->rate / 2) w->target = 0;
        w->level = w->level < w->target ? (w->level + step < w->target ? w->level + step : w->target) : (w->level - step > w->target ? w->level - step : w->target);
        const uint32_t at = (uint32_t)(w->position >> 16), next = at + 1 < t->frames ? at + 1 : 0, frac = (uint32_t)(w->position & 0xffff);
        const int32_t sample = t->pcm[at] + (int32_t)(((int64_t)(t->pcm[next] - t->pcm[at]) * frac) >> 16);
        const int32_t add = (int32_t)((int64_t)sample * w->level / (100 * UNIT));
        for (int c = 0; c < 2; c++) {
            const int32_t sum = out[2 * i + (uint32_t)c] + add;
            out[2 * i + (uint32_t)c] = (int16_t)(sum > 32767 ? 32767 : sum < -32768 ? -32768 : sum);
        }
        w->position = (w->position + advance) % length;
    }
}
