#include "game/race/water_mist.h"

#include <math.h>

void water_mist_shape(uint8_t out[WATER_MIST_BYTES])
{
    for (size_t i = 0; i < 16; i++) out[i] = 0;
    out[0] = 0x7b, out[4] = WATER_MIST_W & 0xff, out[5] = WATER_MIST_W >> 8, out[6] = WATER_MIST_H & 0xff, out[7] = WATER_MIST_H >> 8;
    for (int y = 0; y < WATER_MIST_H; y++)
        for (int x = 0; x < WATER_MIST_W; x++) {
            const float dx = ((float)x - (WATER_MIST_W - 1) / 2.0f) / (WATER_MIST_W / 2.0f), dy = ((float)y - (WATER_MIST_H - 1) / 2.0f) / (WATER_MIST_H / 2.0f);
            const float r2 = dx * dx + dy * dy, edge = r2 >= 1.0f ? 0.0f : powf(1.0f - r2, 1.7f);   /* no edge: it reaches 0 smoothly before the sprite's box */
            int level = (int)lrintf(edge * (float)WATER_MIST_LEVELS);
            if (level > WATER_MIST_LEVELS) level = WATER_MIST_LEVELS;
            out[16 + y * WATER_MIST_W + x] = (uint8_t)level;
        }
}

int water_mist_amount(int variant, int density)
{
    static const int TOP[WATER_MIST_VARIANTS][WATER_MIST_DENSITIES] = { { 8, 52, 96, 132 }, { 8, 52, 96, 132 }, { 6, 40, 74, 104 } };
    if (variant < 1 || variant > WATER_MIST_VARIANTS) return 0;
    if (density < 0) density = 0;
    if (density >= WATER_MIST_DENSITIES) density = WATER_MIST_DENSITIES - 1;
    return TOP[variant - 1][density];
}

void water_mist_colour(int variant, uint8_t rgb[3])
{
    static const uint8_t COLOUR[WATER_MIST_VARIANTS][3] = { { 236, 241, 248 }, { 236, 241, 248 }, { 214, 228, 244 } };
    const int v = variant < 1 || variant > WATER_MIST_VARIANTS ? 0 : variant - 1;
    for (int k = 0; k < 3; k++) rgb[k] = COLOUR[v][k];
}

int water_mist_width_pct(int variant) { return variant == 3 ? 260 : 200; }

int water_mist_height_pct(int variant) { return variant == 3 ? 24 : 36; }

int water_mist_streaks(int variant) { return variant >= 2; }
