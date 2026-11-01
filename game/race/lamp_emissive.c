#include "game/race/lamp_emissive.h"

#include "game/race/car_mask.h"

#include <stdlib.h>

static int kind_of(uint8_t cls)
{
    return cls == CAR_MASK_BRAKE ? LAMP_BRAKE : cls == CAR_MASK_POSITION ? LAMP_POSITION : cls == CAR_MASK_HEADLIGHT ? LAMP_HEADLIGHT : -1;
}

int lamp_texels_collect(LampTexels *l, uint32_t base, const uint8_t *mask, size_t count)
{
    if (l == NULL || mask == NULL) return 0;
    int added = 0;
    for (size_t i = 0; i < count; i++) {
        const int kind = kind_of(mask[i]);
        if (kind < 0) continue;
        if (l->count == l->capacity) {
            const int capacity = l->capacity != 0 ? 2 * l->capacity : 256;
            LampTexel *grown = realloc(l->texel, (size_t)capacity * sizeof *grown);
            if (grown == NULL) return -1;
            l->texel = grown, l->capacity = capacity;
        }
        l->texel[l->count++] = (LampTexel){ base + (uint32_t)i, (uint8_t)kind, 0 };
        added++;
    }
    return added;
}

void lamp_texels_free(LampTexels *l)
{
    if (l == NULL) return;
    free(l->texel);
    *l = (LampTexels){ 0 };
}

void lamp_texels_read(LampTexels *l, const uint8_t *file, size_t size)
{
    int kept = 0;
    for (int i = 0; i < l->count; i++) {
        if (l->texel[i].offset >= size) continue;
        l->texel[kept] = l->texel[i];
        l->texel[kept++].day = file[l->texel[i].offset];
    }
    l->count = kept;
}

int lamp_texels_write(const LampTexels *l, uint8_t *file, size_t size, LampState state, const LampEntries *e)
{
    int changed = 0;
    for (int i = 0; i < l->count; i++) {
        const LampTexel *t = &l->texel[i];
        if (t->offset >= size) continue;
        const uint8_t entry = t->kind == LAMP_BRAKE ? (state == LAMP_BRAKING ? e->brake_lit : e->position) : t->kind == LAMP_POSITION ? e->position : e->headlight;
        const uint8_t want = state == LAMP_DAY || entry == 0 ? t->day : entry;
        if (file[t->offset] != want) file[t->offset] = want, changed++;
    }
    return changed;
}

int lamp_emissive_low(const float tint[3], const float ratio[3])
{
    enum { EPSILON_PERCENT = 1 };   /* the ratios are rounded to steps: 0.995 is the day's */
    const float below = 1.0f - EPSILON_PERCENT / 100.0f;
    for (int k = 0; k < 3; k++)
        if ((tint != NULL && tint[k] < below) || (ratio != NULL && ratio[k] < below)) return 1;
    return 0;
}

static int gap(const uint8_t usable[256], const uint8_t rgb[768], int entry)
{
    int best = 1 << 30;
    for (int o = 0; o < 256; o++) {
        if (o == entry || !usable[o]) continue;
        int d = 0;
        for (int k = 0; k < 3; k++) d += abs((int)rgb[3 * entry + k] - (int)rgb[3 * o + k]);
        if (d < best) best = d;
    }
    return best;
}

int lamp_pick_entries(const uint8_t used[256], const uint8_t usable[256], const uint8_t rgb[768], int first, int last, int want, uint8_t *out)
{
    int found = 0, distance[256] = { 0 };
    for (int e = first < 0 ? 0 : first; e <= last && e < 256; e++) {
        if (used[e]) continue;
        const int d = gap(usable, rgb, e);
        int at = found;   /* the want nearest, kept in order: a later (higher) entry wins a tie */
        while (at > 0 && d <= distance[at - 1]) at--;
        if (at >= want) continue;
        for (int k = (found < want ? found : want - 1); k > at; k--) out[k] = out[k - 1], distance[k] = distance[k - 1];
        out[at] = (uint8_t)e, distance[at] = d;
        if (found < want) found++;
    }
    return found;
}
