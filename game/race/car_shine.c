/* port (body shine): see car_shine.h. */
#include "game/race/car_shine.h"
#include "game/race/car_envmap.h"
#include "game/sys/palette.h"

#include <math.h>
#include <stddef.h>

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

/* the level as -1 (darkest) .. 0 (neutral) .. 1 (lightest) */
static float level_t(int l)
{
    return l >= CAR_SHINE_NEUTRAL ? (float)(l - CAR_SHINE_NEUTRAL) / (CAR_SHINE_LEVELS - 1 - CAR_SHINE_NEUTRAL)
                                  : (float)(l - CAR_SHINE_NEUTRAL) / CAR_SHINE_NEUTRAL;
}

void car_shine_tables(const uint8_t palette[768], uint8_t out[CAR_SHINE_LEVELS][256])
{
    Family fam[256];
    const uint8_t *usable = palette_usable();   /* a shine never lands on an entry the race rewrites (the brake lamp, the police lights) */
    for (int i = 0; i < MARKERS; i++) fam[i] = family(palette + 3 * i);
    for (int l = 0; l < CAR_SHINE_LEVELS; l++) {
        const float t = level_t(l);
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
                if (!usable[j] || !same_family(&fam[i], &fam[j])) continue;
                const float dr = palette[3 * j] - want[0], dg = palette[3 * j + 1] - want[1], db = palette[3 * j + 2] - want[2];
                const float d = dr * dr + dg * dg + db * db;
                if (d < best_d) out[l][i] = (uint8_t)j, best_d = d;
            }
        }
    }
}

/* port (body_shine 4, envmap): a lift goes toward white (ENV_WHITE of the way at the lightest) and a shade toward black (ENV_SHADE at the darkest),
 * so the highlight takes the paint's hue and loses saturation rather than gaining it */
static const float ENV_WHITE = 0.6f, ENV_SHADE = 0.5f;
static const float ENV_LIGHT[3] = { 240.0f, 245.0f, 255.0f };   /* the highlight's colour: white with a subtle blue */
static const float ENV_NEAR_WHITE = 0.8f;   /* past this lift the strongest cores may take any palette entry: they reach near-white */

static void envmap_want(const uint8_t *rgb, float t, float want[3])
{
    for (int k = 0; k < 3; k++) {
        const float c = rgb[k];
        want[k] = t > 0 ? c + (ENV_LIGHT[k] - c) * ENV_WHITE * t : c * (1.0f + ENV_SHADE * t);
    }
}

/* the palette entry nearest `want`; `like` (NULL: any) limits it to the entries of its hue family */
static uint8_t nearest_entry(const uint8_t palette[768], const Family fam[256], const Family *like, const float want[3])
{
    uint8_t best = 0;
    float best_d = 1e30f;
    const uint8_t *usable = palette_usable();
    for (int j = 0; j < MARKERS; j++) {
        if (!usable[j] || (like != NULL && !same_family(like, &fam[j]))) continue;
        const float dr = palette[3 * j] - want[0], dg = palette[3 * j + 1] - want[1], db = palette[3 * j + 2] - want[2];
        const float d = dr * dr + dg * dg + db * db;
        if (d < best_d) best = (uint8_t)j, best_d = d;
    }
    return best;
}

void car_envmap_tables(const uint8_t palette[768], uint8_t paint[CAR_SHINE_LEVELS][256], uint8_t glass[CAR_SHINE_LEVELS][256])
{
    Family fam[256];
    for (int i = 0; i < MARKERS; i++) fam[i] = family(palette + 3 * i);
    for (int l = 0; l < CAR_SHINE_LEVELS; l++) {
        const float t = level_t(l);
        for (int i = 0; i < 256; i++) {
            paint[l][i] = glass[l][i] = (uint8_t)i;
            if (i >= MARKERS || l == CAR_SHINE_NEUTRAL) continue;
            float want[3];
            envmap_want(palette + 3 * i, t, want);
            /* the same intensity for the paint and for the glass and lamps (class 128), each from its own colour. A partial lift stays in the
             * colour's hue family (the pink blend the owner accepts); only the strongest cores may take any palette entry, so they reach
             * near-white. A shade stays in the family. The near-black trim and tyres stay as they are. */
            const Family *like = (t > 0 && t >= ENV_NEAR_WHITE) ? NULL : &fam[i];
            if (fam[i].top >= NEAR_BLACK) paint[l][i] = nearest_entry(palette, fam, like, want);
            glass[l][i] = nearest_entry(palette, fam, like, want);
        }
    }
}

void car_envmap_hard(const uint8_t palette[768], const uint8_t tables[16][256], uint8_t hard[16][256])
{
    for (int l = 0; l < CAR_SHINE_LEVELS; l++)
        for (int i = 0; i < 256; i++) {
            hard[l][i] = 0;
            if (l + 1 >= CAR_SHINE_LEVELS) continue;
            const uint8_t *a = palette + 3 * tables[l][i], *b = palette + 3 * tables[l + 1][i];
            const float dr = (float)a[0] - b[0], dg = (float)a[1] - b[1], db = (float)a[2] - b[2];
            hard[l][i] = dr * dr + dg * dg + db * db > CAR_ENVMAP_HARD_STEP * CAR_ENVMAP_HARD_STEP;
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
