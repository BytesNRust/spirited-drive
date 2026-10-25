#include "game/race/psx_lgs.h"

#include <string.h>

enum { NUMBERS = 14, CONTEXT_NUMBERS = 9, ALL_NUMBERS = NUMBERS + 1 + CONTEXT_NUMBERS * PSX_LGS_CONTEXTS, NEUTRAL = 128, CHANNEL_MAX = 255 };

/* The first `count` integers of the text outside comments, stopping at the first letter outside a comment (the trailer);
 * the number read. */
static int numbers(const uint8_t *p, size_t size, int *out, int count)
{
    int n = 0;
    size_t i = 0;
    while (i < size && n < count) {
        if (p[i] == '/' && i + 1 < size && p[i + 1] == '*') {
            for (i += 2; i + 1 < size && !(p[i] == '*' && p[i + 1] == '/'); i++) {}
            i += 2;
        } else if (p[i] >= '0' && p[i] <= '9') {
            long v = 0;
            for (; i < size && p[i] >= '0' && p[i] <= '9' && v < 100000; i++) v = v * 10 + (p[i] - '0');
            out[n++] = (int)v;
        } else if (p[i] == ',' || p[i] == ' ' || p[i] == '\t' || p[i] == '\r' || p[i] == '\n' || p[i] == '-') {
            i++;
        } else {
            break;
        }
    }
    return n;
}

int psx_lgs_parse(const uint8_t *file, size_t size, PsxLgs *out)
{
    int v[ALL_NUMBERS];
    if (file == NULL || out == NULL) return 0;
    const int got = numbers(file, size, v, ALL_NUMBERS);
    if (got < NUMBERS) return 0;
    for (int i = 8; i < NUMBERS; i++)
        if (v[i] > CHANNEL_MAX) return 0;
    for (int i = 0; i < 8; i++) out->depth[i] = v[i];
    for (int k = 0; k < 3; k++) out->start[k] = v[8 + k], out->end[k] = v[11 + k];
    out->context_count = 0;
    const int declared = got > NUMBERS ? v[NUMBERS] : 0;
    for (int c = 0; c < declared && c < PSX_LGS_CONTEXTS; c++) {
        const int *n = v + NUMBERS + 1 + CONTEXT_NUMBERS * c;
        if (NUMBERS + 1 + CONTEXT_NUMBERS * (c + 1) > got) break;
        PsxLgsContext *x = &out->contexts[out->context_count++];
        x->seg_ini = n[0], x->seg_fin = n[1], x->mode = n[2];
        for (int k = 0; k < 3; k++) x->c1[k] = n[3 + k] > CHANNEL_MAX ? CHANNEL_MAX : n[3 + k], x->c2[k] = n[6 + k] > CHANNEL_MAX ? CHANNEL_MAX : n[6 + k];
    }
    return 1;
}

static float mix(float a, float b, float t) { return a + (b - a) * t; }

void psx_lgs_ambient(const PsxLgs *lgs, int segment, int segment_count, float out[3])
{
    const float along = segment_count > 0 ? (float)segment / (float)segment_count : 0.0f;
    const float f = along < 0.0f ? 0.0f : along > 1.0f ? 1.0f : along;
    float base[3];
    for (int k = 0; k < 3; k++) base[k] = mix((float)lgs->start[k], (float)lgs->end[k], f);
    float colour[3] = { base[0], base[1], base[2] };
    for (int c = 0; c < lgs->context_count; c++) {
        const PsxLgsContext *x = &lgs->contexts[c];
        const int outside = segment < x->seg_ini ? x->seg_ini - segment : segment > x->seg_fin ? segment - x->seg_fin : 0;
        if (outside > PSX_LGS_BLEND) continue;
        const int span = x->seg_fin - x->seg_ini;
        const float t = x->mode == PSX_LGS_RAMP && span > 0 ? (float)(segment < x->seg_ini ? 0 : segment > x->seg_fin ? span : segment - x->seg_ini) / (float)span : 0.0f;
        const float weight = 1.0f - (float)outside / (float)(PSX_LGS_BLEND + 1);
        for (int k = 0; k < 3; k++) colour[k] = mix(base[k], mix((float)x->c1[k], (float)(x->mode == PSX_LGS_RAMP ? x->c2[k] : x->c1[k]), t), weight);
        break;
    }
    for (int k = 0; k < 3; k++) out[k] = colour[k] / (float)NEUTRAL;
}

int psx_lgs_switched(const PsxLgs *lgs, int segment)
{
    if (lgs == NULL) return 0;
    for (int c = 0; c < lgs->context_count; c++) {
        const PsxLgsContext *x = &lgs->contexts[c];
        if (x->mode == PSX_LGS_RAMP && segment >= x->seg_ini && segment <= x->seg_fin) return 1;
    }
    return 0;
}

void psx_lgs_tint(const PsxLgs *lgs, float tint[3])
{
    for (int k = 0; k < 3; k++) tint[k] = (float)(lgs->start[k] + lgs->end[k]) / (2.0f * NEUTRAL);
}

void psx_lgs_ratio(const PsxLgs *lgs, int segment, int segment_count, const float tint[3], int step, float out[3])
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

void psx_lit_table(const uint8_t palette[768], const float ratio[3], uint8_t out[256])
{
    enum { CHOICES = 0xfe };   /* 0xfe and 0xff mark shadow and see-through texels */
    for (int c = 0; c < 256; c++) {
        if (c >= CHOICES || (ratio[0] == 1.0f && ratio[1] == 1.0f && ratio[2] == 1.0f)) {
            out[c] = (uint8_t)c;
            continue;
        }
        float want[3];
        for (int k = 0; k < 3; k++) want[k] = (float)palette[3 * c + k] * ratio[k];
        int best = 0;
        float best_d = 1e30f;
        for (int i = 0; i < CHOICES; i++) {
            const float dr = (float)palette[3 * i] - want[0], dg = (float)palette[3 * i + 1] - want[1], db = (float)palette[3 * i + 2] - want[2];
            const float d = dr * dr + dg * dg + db * db;
            if (d < best_d) best = i, best_d = d;
        }
        out[c] = (uint8_t)best;
    }
}

void psx_tint_palette(const uint8_t palette[768], const float tint[3], uint8_t out[768])
{
    for (int i = 0; i < 256; i++)
        for (int k = 0; k < 3; k++) {
            const float v = (float)palette[3 * i + k] * tint[k] + 0.5f;
            out[3 * i + k] = (uint8_t)(v > 255.0f ? 255 : v);
        }
}

void psx_world_mask(const uint8_t *shown, const uint8_t *world, size_t count, uint8_t *mask)
{
    for (size_t i = 0; i < count; i++) mask[i] = shown[i] == world[i];
}

void psx_mask_rect(uint8_t *mask, int width, int height, int x, int y, int w, int h)
{
    const int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y, right = x + w > width ? width : x + w, bottom = y + h > height ? height : y + h;
    for (int row = top; row < bottom; row++)
        if (right > left) memset(mask + (size_t)row * (size_t)width + left, 1, (size_t)(right - left));
}
