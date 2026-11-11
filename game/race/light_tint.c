#include "game/race/light_tint.h"
#include "game/race/car_shine.h"
#include "game/sys/palette.h"

#include <string.h>

enum { NEUTRAL = 128 };

uint32_t light_tint_ring_node(int32_t node, uint32_t ring_nodes)
{
    if (ring_nodes == 0) return 0;
    const int64_t n = node % (int64_t)ring_nodes;
    return (uint32_t)(n < 0 ? n + ring_nodes : n);
}

int light_tint_curve(int k)
{
    k = k < 0 ? 0 : k > 511 ? 511 : k;
    const int v = 32 + 128 * k / 255;
    return v > 255 ? 255 : v;
}

void light_tint_script_light(const PsxLgs *lgs, int node, int lgt, float out[3])
{
    const PsxLgsContext *x = NULL;
    for (int c = 0; lgs != NULL && c < lgs->context_count; c++)
        if ((node >> 2) >= lgs->contexts[c].seg_ini && (node >> 2) <= lgs->contexts[c].seg_fin) {
            x = &lgs->contexts[c];
            break;
        }
    for (int k = 0; k < 3; k++) {
        int input = lgt;
        if (x != NULL) {
            const int *colour = (x->mode & 0x20) && (node & 1) ? x->c2 : x->c1;
            input = ((x->mode & 0x40) ? colour[k] : 0) + ((x->mode & 0x80) ? lgt : 0);
        }
        out[k] = (float)light_tint_curve(input) / (float)LIGHT_TINT_CURVE_FULL;
    }
}

void light_tint_car_ratio(const PsxLgs *lgs, uint32_t ring_nodes, int32_t node, int lgt, int step, float out[3])
{
    const int n = (int)light_tint_ring_node(node, ring_nodes);
    light_tint_script_light(lgs, n, lgt, out);
    for (int k = 0; k < 3; k++) {
        const int q = (int)(out[k] * (float)step + 0.5f);
        out[k] = (float)(q > 2 * step ? 2 * step : q) / (float)step;
    }
}

const float light_tint_night[3] = { 0.30f, 0.36f, 0.62f };

static int scaled(int value, float mul)
{
    const int v = (int)((float)value * mul + 0.5f);
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

void light_tint_scale(PsxLgs *lgs, const float mul[3], int contexts)
{
    for (int k = 0; k < 3; k++) {
        lgs->start[k] = scaled(lgs->start[k], mul[k]);
        lgs->end[k] = scaled(lgs->end[k], mul[k]);
        for (int c = 0; contexts && c < lgs->context_count && c < PSX_LGS_CONTEXTS; c++) {
            lgs->contexts[c].c1[k] = scaled(lgs->contexts[c].c1[k], mul[k]);
            lgs->contexts[c].c2[k] = scaled(lgs->contexts[c].c2[k], mul[k]);
        }
    }
}

void light_tint_script_tint(const PsxLgs *lgs, float tint[3])
{
    for (int k = 0; k < 3; k++) tint[k] = (float)(lgs->start[k] + lgs->end[k]) / (2.0f * NEUTRAL);
}

void light_tint_ratio(const PsxLgs *lgs, int segment, int segment_count, const float tint[3], int step, float out[3])
{
    float a[3];
    psx_lgs_ambient(lgs, segment, segment_count, a);
    const int max = 2 * step;
    for (int k = 0; k < 3; k++) {
        const float world = tint[k] > 0.01f ? tint[k] : 0.01f;
        int q = (int)(a[k] / world * (float)step + 0.5f);
        out[k] = (float)(q > max ? max : q) / (float)step;
    }
}

void light_tint_lit_table_floor(const uint8_t palette[768], const float ratio[3], int any_family, const uint8_t add[3], uint8_t out[256])
{
    enum { CHOICES = 0xfe };   /* 0xfe and 0xff mark shadow and see-through texels */
    const uint8_t *usable = palette_usable();   /* not the entries the race rewrites (the brake lamp, the police lights); the cars' own slots are here */
    const uint8_t *emissive = palette_emissive();   /* port (emissive lamps): a lamp is lit whatever the light */
    for (int c = 0; c < 256; c++) {
        if (c >= CHOICES || emissive[c] || (add == NULL && ratio[0] == 1.0f && ratio[1] == 1.0f && ratio[2] == 1.0f)) {
            out[c] = (uint8_t)c;
            continue;
        }
        const int dark_source = add != NULL && palette[3 * c] < 48 && palette[3 * c + 1] < 48 && palette[3 * c + 2] < 48;   /* port (car_night_lift): a dark entry (a tyre, a shade of a dark paint) has no lighter neighbour in its family: the floor lifts it to the nearest of any */
        float want[3];
        for (int k = 0; k < 3; k++) want[k] = (float)palette[3 * c + k] * ratio[k] + (add != NULL ? (float)add[k] : 0.0f);   /* port (car_night_lift): a floor of light, which a multiplier on a near-black paint cannot give */
        /* a colour stays in its hue family under a darker or lighter light (a yellow in shadow went to olive, orange and pink); where the palette has
         * none of the family (a tone with no neighbour) the nearest of all */
        int best = 0, best_any = 0;
        float best_d = 1e30f, best_any_d = 1e30f;
        for (int i = 0; i < CHOICES; i++) {
            if (!usable[i]) continue;
            const float dr = (float)palette[3 * i] - want[0], dg = (float)palette[3 * i + 1] - want[1], db = (float)palette[3 * i + 2] - want[2];
            const float d = dr * dr + dg * dg + db * db;
            if (d < best_any_d) best_any = i, best_any_d = d;
            if (d < best_d && (any_family || dark_source || car_shine_same_family(palette + 3 * c, palette + 3 * i))) best = i, best_d = d;
        }
        out[c] = (uint8_t)(best_d < 1e29f ? best : best_any);
    }
}

void light_tint_lit_table_ex(const uint8_t palette[768], const float ratio[3], int any_family, uint8_t out[256]) { light_tint_lit_table_floor(palette, ratio, any_family, NULL, out); }

void light_tint_lit_table(const uint8_t palette[768], const float ratio[3], uint8_t out[256]) { light_tint_lit_table_ex(palette, ratio, 0, out); }

void light_tint_car_tone_table(const uint8_t palette[768], const uint8_t *base, const uint8_t rgb[3], int percent, uint8_t out[256])
{
    enum { CHOICES = 0xfe };
    const uint8_t *usable = palette_usable();
    const uint8_t *emissive = palette_emissive();
    const float p = (float)(percent < 0 ? 0 : percent > 100 ? 100 : percent) / 100.0f;
    const float tone_lum = 0.30f * rgb[0] + 0.59f * rgb[1] + 0.11f * rgb[2];
    for (int c = 0; c < 256; c++) {
        const int from = base != NULL ? base[c] : c;
        if (c >= CHOICES || emissive[c] || from >= CHOICES || p <= 0.0f || tone_lum < 1.0f) {
            out[c] = (uint8_t)from;
            continue;
        }
        const float lum = 0.30f * palette[3 * from] + 0.59f * palette[3 * from + 1] + 0.11f * palette[3 * from + 2];
        float want[3];
        for (int k = 0; k < 3; k++) {
            const float target = (float)rgb[k] * lum / tone_lum;
            want[k] = (float)palette[3 * from + k] * (1.0f - p) + target * p;
        }
        int best = from;
        float best_d = 1e30f;
        for (int i = 0; i < CHOICES; i++) {
            if (!usable[i]) continue;
            const float dr = (float)palette[3 * i] - want[0], dg = (float)palette[3 * i + 1] - want[1], db = (float)palette[3 * i + 2] - want[2];
            const float d = dr * dr + dg * dg + db * db;
            if (d < best_d) best = i, best_d = d;
        }
        out[c] = (uint8_t)best;
    }
}

void light_tint_palette(const uint8_t palette[768], const float tint[3], uint8_t out[768])
{
    const uint8_t *emissive = palette_emissive();
    for (int i = 0; i < 256; i++)
        for (int k = 0; k < 3; k++) {
            if (emissive[i]) {
                out[3 * i + k] = palette[3 * i + k];
                continue;
            }
            const float v = (float)palette[3 * i + k] * tint[k] + 0.5f;
            out[3 * i + k] = (uint8_t)(v > 255.0f ? 255 : v);
        }
}

void light_tint_world_mask(const uint8_t *shown, const uint8_t *world, size_t count, uint8_t *mask)
{
    for (size_t i = 0; i < count; i++) mask[i] = shown[i] == world[i];
}

void light_tint_mask_rect(uint8_t *mask, int width, int height, int x, int y, int w, int h)
{
    const int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y, right = x + w > width ? width : x + w, bottom = y + h > height ? height : y + h;
    for (int row = top; row < bottom; row++)
        if (right > left) memset(mask + (size_t)row * (size_t)width + left, 1, (size_t)(right - left));
}

void light_tint_dash_palette(const uint8_t palette[768], const float tint[3], int num, int den, uint8_t out[768])
{
    uint8_t full[768];
    light_tint_palette(palette, tint, full);
    for (int i = 0; i < 768; i++) out[i] = (uint8_t)((palette[i] * (den - num) + full[i] * num + den / 2) / den);
}

float light_tint_night_ceiling_scale(const float tint[3], float ceiling)
{
    const float mean = (tint[0] + tint[1] + tint[2]) / 3.0f;
    return mean > ceiling && mean > 0.0f ? ceiling / mean : 1.0f;
}
