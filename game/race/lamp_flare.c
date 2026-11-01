/* The tail light halo (lamp_flare.h). */
#include "game/race/lamp_flare.h"

#include <stdlib.h>
#include <string.h>

enum { STACK = 4096 };

static int by_area(const void *a, const void *b)
{
    return ((const LampBlob *)b)->area - ((const LampBlob *)a)->area;
}

int lamp_blobs(const uint8_t *pixels, int pitch, int x0, int y0, int x1, int y1, const uint8_t member[256], LampBlob *out, int max)
{
    if (pixels == NULL || out == NULL || member == NULL || max <= 0 || x1 < x0 || y1 < y0) return 0;
    const int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if ((long)w * (long)h > 1 << 20) return 0;   /* a car's rectangle is never this big; a bad one would only cost time */
    uint8_t *seen = calloc((size_t)w * (size_t)h, 1);
    int *stack = malloc(sizeof(int) * STACK);
    LampBlob found[LAMP_FLARE_BLOBS * 4];
    int count = 0;
    if (seen == NULL || stack == NULL) {
        free(seen), free(stack);
        return 0;
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const int at = y * w + x;
            const uint8_t seed = pixels[(size_t)(y0 + y) * (size_t)pitch + (size_t)(x0 + x)];
            if (seen[at] || !member[seed]) continue;
            long sx = 0, sy = 0;
            int area = 0, top = 0;
            seen[at] = 1, stack[top++] = at;
            while (top > 0) {
                const int p = stack[--top], px = p % w, py = p / w;
                sx += px, sy += py, area++;
                const int nx[4] = { px - 1, px + 1, px, px }, ny[4] = { py, py, py - 1, py + 1 };
                for (int k = 0; k < 4; k++) {
                    if (nx[k] < 0 || nx[k] >= w || ny[k] < 0 || ny[k] >= h) continue;
                    const int q = ny[k] * w + nx[k];
                    if (seen[q] || !member[pixels[(size_t)(y0 + ny[k]) * (size_t)pitch + (size_t)(x0 + nx[k])]]) continue;
                    seen[q] = 1;
                    if (top < STACK) stack[top++] = q;
                }
            }
            if (count < (int)(sizeof found / sizeof found[0])) {
                found[count].cx = x0 + (int)(sx / area), found[count].cy = y0 + (int)(sy / area), found[count].area = area, found[count].value = seed;
                count++;
            }
        }
    qsort(found, (size_t)count, sizeof found[0], by_area);
    if (count > max) count = max;
    memcpy(out, found, sizeof found[0] * (size_t)count);
    free(seen), free(stack);
    return count;
}

int lamp_flare_radius(int area)
{
    int r = 0;
    while ((r + 1) * (r + 1) * 3 <= area) r++;   /* the blob's radius: sqrt(area / 3), a little under sqrt(area / pi) */
    r = r * 22 / 10;
    return r < 3 ? 3 : r > LAMP_FLARE_MAX_RADIUS ? LAMP_FLARE_MAX_RADIUS : r;
}

int lamp_halo_level(int dx, int dy, int radius)
{
    static const int BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    const long d2 = (long)dx * dx + (long)dy * dy, r2 = (long)radius * radius;
    if (radius <= 0 || d2 >= r2) return 0;
    /* intensity (1 - d2 / r2)^2 in 1/4096, times the levels in 1/16 of a level, plus the ordered threshold (0..15 of 16) */
    const long k = 4096 - d2 * 4096 / r2;
    const long i = k * k / 4096;
    const long v = i * LAMP_FLARE_LEVELS * 16 / 4096 + BAYER[dy & 3][dx & 3];
    const long level = v / 16;
    return level < 0 ? 0 : level > LAMP_FLARE_LEVELS ? LAMP_FLARE_LEVELS : (int)level;
}

void lamp_halo_draw(uint8_t *pixels, int pitch, int x0, int y0, int x1, int y1, int cx, int cy, int radius,
                    const uint8_t *const map[LAMP_FLARE_LEVELS])
{
    if (pixels == NULL || map == NULL || radius <= 0) return;
    for (int y = cy - radius; y <= cy + radius; y++) {
        if (y < y0 || y > y1) continue;
        for (int x = cx - radius; x <= cx + radius; x++) {
            if (x < x0 || x > x1) continue;
            const int level = lamp_halo_level(x - cx, y - cy, radius);
            if (level == 0) continue;
            uint8_t *const p = pixels + (size_t)y * (size_t)pitch + (size_t)x;
            *p = map[level - 1][*p];
        }
    }
}
