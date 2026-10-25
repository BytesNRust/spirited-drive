#include "game/race/car_paint.h"

#include <math.h>
#include <stddef.h>

enum { PALETTE_8BIT = 0x24, PALETTE_6BIT = 0x22 };

/* how much of the shading survives: an entry's lightness against the body's main tone, to this power */
static const float SHADE_POWER = 0.7f;
/* a dark paint's highlights are lifted by up to this many levels at the shading's top, so black still shows its curves */
static const float DARK_LIFT = 24.0f;

typedef struct {
    const char *name;
    int count;
    uint8_t own[3];
    CarPaint paints[CAR_PAINT_MAX];
} CarPaints;

/* by the race model (race_models.h's car_model_names): TSupra, LDiabl, P911, CZR1, F512TR, DVIPER, ANSX, MRX7 */
static const CarPaints TABLE[CAR_PAINT_MODELS] = {
    { "TSupra", 3, { 164, 24, 24 },
      { { "Super White II", { 244, 244, 242 } }, { "Platinum Silver Metallic", { 169, 173, 178 } },
        { "Baltic Blue Metallic", { 60, 107, 142 } } } },
    { "LDiabl", 1, { 32, 32, 32 }, { { "Rosso Diablo", { 179, 18, 30 } } } },
    { "P911", 4, { 16, 56, 72 },
      { { "Grand Prix White", { 241, 241, 238 } }, { "Black", { 14, 14, 16 } }, { "Guards Red", { 179, 18, 30 } },
        { "Speed Yellow", { 242, 197, 0 } } } },
    { "CZR1", 4, { 8, 32, 32 },
      { { "Arctic White", { 242, 242, 238 } }, { "Black", { 17, 17, 17 } }, { "Competition Yellow", { 242, 210, 27 } },
        { "Admiral Blue Metallic", { 31, 58, 110 } } } },
    { "F512TR", 4, { 188, 8, 16 },
      { { "Nero", { 10, 10, 10 } }, { "Bianco", { 244, 244, 240 } }, { "Blu Sera Metallizzato", { 31, 59, 107 } },
        { "Argento Metallizzato", { 185, 189, 194 } } } },
    { "DVIPER", 4, { 16, 16, 72 },
      { { "Viper Black", { 12, 12, 12 } }, { "Stone White", { 241, 240, 234 } }, { "Dandelion Yellow", { 242, 194, 0 } },
        { "Emerald Green", { 31, 92, 74 } } } },
    { "ANSX", 4, { 196, 196, 196 },
      { { "Berlina Black", { 14, 14, 16 } }, { "Grand Prix White", { 241, 241, 238 } }, { "Formula Red", { 179, 18, 30 } },
        { "Citrous Yellow Metallic", { 230, 217, 74 } } } },
    { "MRX7", 4, { 190, 198, 24 },
      { { "Brilliant Black", { 17, 17, 17 } }, { "Crystal White", { 242, 242, 238 } },
        { "Silver Stone Metallic", { 169, 173, 178 } }, { "Blaze Red", { 200, 40, 30 } } } },
};

int car_paint_count(int model)
{
    return model >= 0 && model < CAR_PAINT_MODELS ? TABLE[model].count : 0;
}

const CarPaint *car_paint(int model, int k)
{
    return k >= 0 && k < car_paint_count(model) ? &TABLE[model].paints[k] : NULL;
}

const uint8_t *car_paint_default(int model)
{
    return model >= 0 && model < CAR_PAINT_MODELS ? TABLE[model].own : NULL;
}

static float lightness(const float c[3]) { return 0.30f * c[0] + 0.59f * c[1] + 0.11f * c[2]; }

static float luma_of(const uint8_t rgb[3])
{
    const float c[3] = { (float)rgb[0], (float)rgb[1], (float)rgb[2] };
    const float y = lightness(c);
    return y < 1.0f ? 1.0f : y;
}

/* the lightness the heaviest half of the pixels ends on, counted from the darkest entry */
static float main_tone(const float *y, const uint32_t *weight, int n)
{
    int order[CAR_PAINT_ENTRIES];
    uint64_t total = 0;
    for (int i = 0; i < n; i++) {
        int k = i;
        while (k > 0 && y[order[k - 1]] > y[i]) order[k] = order[k - 1], k--;
        order[k] = i;
        total += weight[i];
    }
    if (total == 0) return (y[order[0]] + y[order[n - 1]]) / 2;   /* no pixels counted: the middle of the range */
    uint64_t before = 0;
    for (int k = 0; k < n; k++) {
        before += weight[order[k]];
        if (before * 2 >= total) return y[order[k]];
    }
    return y[order[n - 1]];
}

static float shade_of(const CarPaintPlan *plan, float y)
{
    float f = powf(y / plan->main, SHADE_POWER);
    f = f < 0.1f ? 0.1f : f > 4.0f ? 4.0f : f;
    return f > 1.0f ? 1.0f + (f - 1.0f) * (plan->room < 1.0f ? plan->room : 1.0f) : f;
}

void car_paint_plan(CarPaintPlan *plan, const uint8_t (*rgb)[3], const uint32_t *weight, int n, const uint8_t target[3])
{
    float y[CAR_PAINT_ENTRIES];
    if (n > CAR_PAINT_ENTRIES) n = CAR_PAINT_ENTRIES;
    for (int i = 0; i < n; i++) y[i] = luma_of(rgb[i]);
    plan->main = n > 0 ? main_tone(y, weight, n) : 1.0f;
    float fmax = 1.0f;
    for (int i = 0; i < n; i++) {
        float f = powf(y[i] / plan->main, SHADE_POWER);
        f = f < 0.1f ? 0.1f : f > 4.0f ? 4.0f : f;
        fmax = f > fmax ? f : fmax;
    }
    const float brightest = (float)(target[0] > target[1] ? (target[0] > target[2] ? target[0] : target[2])
                                                          : (target[1] > target[2] ? target[1] : target[2]));
    const float headroom = brightest > 0 ? 255.0f / brightest : 1.0f;
    plan->room = headroom > 1.0f ? (headroom - 1.0f) / (fmax - 1.0f > 0 ? fmax - 1.0f : 1.0f) : 0.0f;
    plan->lift = brightest < 64.0f ? DARK_LIFT * (64.0f - brightest) / 64.0f : 0.0f;
    for (int c = 0; c < 3; c++) plan->rgb[c] = target[c];
}

void car_paint_shade(const CarPaintPlan *plan, const uint8_t in[3], uint8_t out[3])
{
    const float y = luma_of(in);
    const float g = shade_of(plan, y);
    const float up = y > plan->main ? (powf(y / plan->main, SHADE_POWER) - 1.0f) * plan->lift : 0.0f;
    for (int c = 0; c < 3; c++) {
        float v = (float)plan->rgb[c] * g + up;
        v = v < 0 ? 0 : v > 255 ? 255 : v;
        out[c] = (uint8_t)(int)(v + 0.5f);
    }
}

void car_paint_recolour(uint8_t *palette, int type, const uint8_t *entries, int n, const uint32_t weight[256],
                        const uint8_t rgb[3])
{
    if (palette == NULL || entries == NULL || n <= 0 || n > CAR_PAINT_ENTRIES || (type != PALETTE_8BIT && type != PALETTE_6BIT)) return;
    const int shift = type == PALETTE_6BIT ? 2 : 0;
    uint8_t colour[CAR_PAINT_ENTRIES][3];
    uint32_t w[CAR_PAINT_ENTRIES];
    for (int i = 0; i < n; i++) {
        const uint8_t *e = palette + 3 * entries[i];
        for (int c = 0; c < 3; c++) colour[i][c] = (uint8_t)(e[c] << shift);
        w[i] = weight[entries[i]];
    }
    CarPaintPlan plan;
    car_paint_plan(&plan, (const uint8_t (*)[3])colour, w, n, rgb);
    for (int i = 0; i < n; i++) {
        uint8_t out[3];
        car_paint_shade(&plan, colour[i], out);
        for (int c = 0; c < 3; c++) palette[3 * entries[i] + c] = (uint8_t)(out[c] >> shift);
    }
}

/* a small integer hash (xorshift and multiply) of the seed and the slot; the high bits are used, the low ones of a multiply only
 * depend on the low bits of its input */
static uint32_t mix(uint32_t seed, int slot)
{
    uint32_t h = seed * 0x9e3779b1u ^ (uint32_t)(slot + 1) * 0x85ebca6bu;
    h ^= h >> 15;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

int car_paint_pick(uint32_t seed, int slot, int car, int player_car, int player_paint, int catalogue, int ps1)
{
    int ids[2 + CAR_PAINT_MAX], n = 0;
    ids[n++] = 0;
    if (ps1) ids[n++] = 1;
    for (int k = 0; k < catalogue && k < CAR_PAINT_MAX; k++) ids[n++] = 2 + k;
    int count = 0, allowed[2 + CAR_PAINT_MAX];
    for (int i = 0; i < n; i++)
        if (!(car == player_car && ids[i] == player_paint)) allowed[count++] = ids[i];
    if (count == 0) return 0;
    return allowed[(mix(seed, slot) >> 8) % (uint32_t)count];
}
