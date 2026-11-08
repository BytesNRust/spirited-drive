/* port (rally_spray): see spray_tint.h. */
#include "game/race/spray_tint.h"

static int luma_of(int r, int g, int b) { return (r * 77 + g * 150 + b * 29) >> 8; }

enum { SNOW_LUMA = 190, DIRTY_PERCENT = 62 };   /* a road lighter than this is snow; the others' mist is this much of the road's brightness */

void spray_tint_target(const uint8_t road[3], uint8_t out[3])
{
    const int l = luma_of(road[0], road[1], road[2]);
    for (int k = 0; k < 3; k++) {
        if (l >= SNOW_LUMA) out[k] = (uint8_t)(road[k] + (255 - road[k]) / 3);   /* white powder, a little lighter than the snow it flies from */
        else out[k] = (uint8_t)(road[k] * DIRTY_PERCENT / 100);
    }
}

void spray_tint_table(const uint8_t pal[768], const uint8_t tint[3], int percent, uint8_t table[256])
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    for (int i = 0; i < 256; i++) {
        table[i] = (uint8_t)i;
        if (i == 0xFF) continue;
        int want[3];
        for (int k = 0; k < 3; k++) want[k] = (pal[i * 3 + k] * (100 - percent) + tint[k] * percent + 50) / 100;
        int best = i, best_d = 1 << 30;
        for (int j = 0; j < 255; j++) {   /* the nearest palette entry; the lower index wins a tie but the entry itself wins its own colour */
            const int dr = pal[j * 3] - want[0], dg = pal[j * 3 + 1] - want[1], db = pal[j * 3 + 2] - want[2];
            const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;   /* the eye's weights, roughly */
            if (d < best_d || (d == best_d && j == i)) best_d = d, best = j;
        }
        table[i] = (uint8_t)best;
    }
}

uint32_t spray_tint_key(const uint8_t colour[3])
{
    return (uint32_t)(colour[0] >> 4) << 8 | (uint32_t)(colour[1] >> 4) << 4 | (uint32_t)(colour[2] >> 4);
}

int spray_mist_kind(const uint8_t road[3])
{
    const int l = luma_of(road[0], road[1], road[2]);
    if (l >= SNOW_LUMA) return SPRAY_MIST_SNOW;
    const int hi = road[0] > road[1] ? (road[0] > road[2] ? road[0] : road[2]) : (road[1] > road[2] ? road[1] : road[2]);
    const int lo = road[0] < road[1] ? (road[0] < road[2] ? road[0] : road[2]) : (road[1] < road[2] ? road[1] : road[2]);
    return hi > 0 && (hi - lo) * 100 >= hi * 14 ? SPRAY_MIST_MUD : SPRAY_MIST_WATER;   /* a hue: a dirt, a sand; grey: asphalt, gravel */
}

void spray_mist_colour(const uint8_t road[3], uint8_t out[3])
{
    switch (spray_mist_kind(road)) {
    case SPRAY_MIST_SNOW: out[0] = 244, out[1] = 247, out[2] = 252; break;
    case SPRAY_MIST_WATER: out[0] = 222, out[1] = 230, out[2] = 240; break;
    default:
        for (int k = 0; k < 3; k++) out[k] = (uint8_t)(road[k] + (255 - road[k]) * 40 / 100);   /* the road's hue, lifted: beige over brown */
    }
}

void spray_add_table(const uint8_t pal[768], const uint8_t colour[3], int amount, uint8_t table[256])
{
    if (amount < 0) amount = 0;
    if (amount > 255) amount = 255;
    for (int i = 0; i < 256; i++) {
        table[i] = (uint8_t)i;
        if (i == 0xFF || amount == 0) continue;
        int want[3];
        for (int k = 0; k < 3; k++) {
            want[k] = pal[i * 3 + k] + colour[k] * amount / 255;
            if (want[k] > 255) want[k] = 255;
        }
        int best = i, best_d = 1 << 30;
        const int floor_luma = pal[i * 3] + 2 * pal[i * 3 + 1] + pal[i * 3 + 2];   /* an additive never darkens: no colour of the palette that is darker than the one under */
        for (int j = 0; j < 255; j++) {
            if (j != i && pal[j * 3] + 2 * pal[j * 3 + 1] + pal[j * 3 + 2] < floor_luma) continue;
            const int dr = pal[j * 3] - want[0], dg = pal[j * 3 + 1] - want[1], db = pal[j * 3 + 2] - want[2];
            const int warm = (pal[j * 3] - pal[j * 3 + 2]) - (want[0] - want[2]);   /* the tint's warm or cool side is kept: a beige lift of a dark blue stays beige, not a grey-blue */
            const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2 + warm * warm * 3;
            if (d < best_d || (d == best_d && j == i)) best_d = d, best = j;
        }
        table[i] = (uint8_t)best;
    }
}

int spray_mist_shape(const uint8_t *shape, const uint8_t grey[256], uint8_t *out, size_t out_size)
{
    if (shape == NULL || out == NULL || shape[0] != 0x7b) return 0;
    const size_t w = (size_t)(shape[4] | shape[5] << 8), h = (size_t)(shape[6] | shape[7] << 8);
    if (out_size < 16 + w * h) return 0;
    for (size_t i = 0; i < 16; i++) out[i] = shape[i];
    int top = 0;   /* the sprite's whitest grey is the top level whatever the palette's range: the CD's blend palette is dark, its greys stay low */
    for (size_t i = 0; i < w * h; i++) {
        const uint8_t t = shape[16 + i];
        if (t != 0xFF && grey[t] > top) top = grey[t];
    }
    for (size_t i = 0; i < w * h; i++) {
        const uint8_t t = shape[16 + i];
        const int g = t == 0xFF || top == 0 || grey[t] * 255 / top < 24 ? 0 : grey[t] * 255 / top;   /* the faint fringe is nothing: it would show the sprite's box */
        int level = (g * SPRAY_MIST_LEVELS + 127) / 255;
        if (g > 0 && level < 1) level = 1;
        out[16 + i] = (uint8_t)level;
    }
    return 1;
}
