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
    static const int TOP[WATER_MIST_VARIANTS][WATER_MIST_DENSITIES] = { { 6, 38, 72, 100 }, { 6, 38, 72, 100 }, { 5, 30, 56, 80 } };
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

int water_mist_height_pct(int variant) { return variant == 3 ? 40 : 55; }

int water_mist_streaks(int variant) { return variant >= 2; }

void water_mist_streak_shape(uint8_t out[WATER_STREAK_BYTES], int mirror)
{
    for (int i = 0; i < 16; i++) out[i] = 0;
    out[0] = 0x7b, out[4] = WATER_STREAK_SIDE, out[6] = WATER_STREAK_SIDE;
    for (int i = 0; i < WATER_STREAK_SIDE * WATER_STREAK_SIDE; i++) out[16 + i] = 0;
    static const int START[5][3] = { { 14, 28, -1 }, { 17, 30, 1 }, { 11, 25, -2 }, { 20, 26, 2 }, { 15, 22, 0 } };   /* x, y and the lean of each: up the box, out to the sides */
    for (int n = 0; n < 3; n++) {
        const int lean = mirror ? -START[n][2] : START[n][2];
        const int x0 = mirror ? WATER_STREAK_SIDE - 1 - START[n][0] : START[n][0];
        for (int s = 0; s < 9; s++) {
            const int x = x0 + lean * s / 3, y = START[n][1] - s * 2 + (s * s) / 14;   /* a little arc: up and then flattening */
            if (x < 0 || x >= WATER_STREAK_SIDE || y < 0 || y >= WATER_STREAK_SIDE) continue;
            out[16 + y * WATER_STREAK_SIDE + x] = (uint8_t)(WATER_MIST_LEVELS - s / 3);   /* bright at the start, thinning along */
            if (y + 1 < WATER_STREAK_SIDE && s < 6) out[16 + (y + 1) * WATER_STREAK_SIDE + x] = (uint8_t)(WATER_MIST_LEVELS - 2 - s / 3);
        }
    }
}

int water_mist_streak_amount(void) { return 150; }
