#include "game/race/psx_lgs.h"

#include <string.h>

enum { NUMBERS = 14, NEUTRAL = 128, CHANNEL_MAX = 255 };

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
    int v[NUMBERS];
    if (file == NULL || out == NULL || numbers(file, size, v, NUMBERS) != NUMBERS) return 0;
    for (int i = 8; i < NUMBERS; i++)
        if (v[i] > CHANNEL_MAX) return 0;
    for (int i = 0; i < 8; i++) out->depth[i] = v[i];
    for (int k = 0; k < 3; k++) out->start[k] = v[8 + k], out->end[k] = v[11 + k];
    return 1;
}

void psx_lgs_tint(const PsxLgs *lgs, float tint[3])
{
    for (int k = 0; k < 3; k++) tint[k] = (float)(lgs->start[k] + lgs->end[k]) / (2.0f * NEUTRAL);
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
