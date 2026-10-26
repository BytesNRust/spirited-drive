/* port (body shine): see car_shine.h. */
#include "game/race/car_shine.h"

#include <math.h>

enum { MARKERS = 0xfe, NEAR_BLACK = 30, CHROMA_MIN = 20 };
static const float HUE_TOLERANCE = 8.0f;   /* degrees */
static const float LIFT = 0.54f, WHITE = 42.0f, SHADOW = 0.35f;

typedef struct {
    float chroma, hue;
    int top;
} Family;

static Family family(const uint8_t *rgb)
{
    const int r = rgb[0], g = rgb[1], b = rgb[2];
    const int hi = r > g ? (r > b ? r : b) : (g > b ? g : b), lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
    Family f = { (float)(hi - lo), 0, hi };
    if (hi == lo) return f;
    const float c = (float)(hi - lo);
    float h;
    if (hi == r) h = fmodf((g - b) / c, 6.0f);
    else if (hi == g) h = (b - r) / c + 2.0f;
    else h = (r - g) / c + 4.0f;
    f.hue = h * 60.0f;
    if (f.hue < 0) f.hue += 360.0f;
    return f;
}

static int same_family(const Family *a, const Family *b)
{
    if (a->chroma < CHROMA_MIN) return b->chroma < CHROMA_MIN * 1.5f;   /* a grey takes a grey */
    if (b->chroma < CHROMA_MIN * 0.5f) return 0;
    float d = fabsf(a->hue - b->hue);
    if (d > 180.0f) d = 360.0f - d;
    return d <= HUE_TOLERANCE;
}

void car_shine_tables(const uint8_t palette[768], uint8_t out[CAR_SHINE_LEVELS][256])
{
    Family fam[256];
    for (int i = 0; i < MARKERS; i++) fam[i] = family(palette + 3 * i);
    for (int l = 0; l < CAR_SHINE_LEVELS; l++) {
        const float t = l >= CAR_SHINE_NEUTRAL ? (float)(l - CAR_SHINE_NEUTRAL) / (CAR_SHINE_LEVELS - 1 - CAR_SHINE_NEUTRAL)
                                               : (float)(l - CAR_SHINE_NEUTRAL) / CAR_SHINE_NEUTRAL;
        for (int i = 0; i < 256; i++) {
            out[l][i] = (uint8_t)i;
            if (i >= MARKERS || l == CAR_SHINE_NEUTRAL || fam[i].top < NEAR_BLACK) continue;
            float want[3];
            for (int k = 0; k < 3; k++) {
                const float c = palette[3 * i + k];
                want[k] = t > 0 ? fminf(c * (1.0f + LIFT * t) + WHITE * t, 255.0f) : c * (1.0f + SHADOW * t);
            }
            float best_d = 1e30f;
            for (int j = 0; j < MARKERS; j++) {
                if (!same_family(&fam[i], &fam[j])) continue;
                const float dr = palette[3 * j] - want[0], dg = palette[3 * j + 1] - want[1], db = palette[3 * j + 2] - want[2];
                const float d = dr * dr + dg * dg + db * db;
                if (d < best_d) out[l][i] = (uint8_t)j, best_d = d;
            }
        }
    }
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

typedef struct {
    float sky;    /* how far the bright band above the horizon climbs toward the lightest level */
    float dark;   /* how far the band below sinks toward the darkest */
} Gains;

static const Gains GAINS[3] = { { 0.80f, 0.60f }, { 0.95f, 0.85f }, { 1.05f, 1.00f } };
static const float HORIZON = 0.12f;   /* the reflection's elevation over which the line between the bands passes: a thin soft line */

static const Gains *gains(int strength) { return &GAINS[strength < 1 ? 0 : strength > 3 ? 2 : strength - 1]; }

static float smooth(float x) { return x >= 1.0f ? 1.0f : x <= 0.0f ? 0.0f : x * x * (3.0f - 2.0f * x); }

float car_shine_level(const float normal[3], const float view[3], const float up[3], int strength)
{
    const Gains *g = gains(strength);
    const float d = view[0] * normal[0] + view[1] * normal[1] + view[2] * normal[2];
    float e = 0;   /* the reflection's elevation over the world's horizon */
    for (int k = 0; k < 3; k++) e += (view[k] - 2.0f * d * normal[k]) * up[k];
    e = clampf(e, -1.0f, 1.0f);
    const float top = (float)(CAR_SHINE_LEVELS - 1 - CAR_SHINE_NEUTRAL);
    const float level = e >= 0 ? CAR_SHINE_NEUTRAL + top * g->sky * smooth(e / HORIZON)
                               : CAR_SHINE_NEUTRAL - CAR_SHINE_NEUTRAL * g->dark * smooth(-e / HORIZON) * (0.8f + 0.2f * -e);
    return clampf(level, 0.0f, (float)(CAR_SHINE_LEVELS - 1));
}
