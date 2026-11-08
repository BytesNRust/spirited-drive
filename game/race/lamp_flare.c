/* The tail light halo (lamp_flare.h). */
#include "game/race/lamp_flare.h"

#include <stdlib.h>
#include <string.h>

enum { STACK = 4096 };

static int by_area(const void *a, const void *b)
{
    return ((const LampBlob *)b)->area - ((const LampBlob *)a)->area;
}

/* the scan both entry points share: `seen` is w*h stamps, a pixel is seen when its stamp is `stamp` (so the caller need not clear it between scans), `found` has room for `cap` blobs; returns how many were found, unsorted */
static int scan_blobs(const uint8_t *pixels, int pitch, int x0, int y0, int w, int h, const uint8_t member[256], uint16_t *seen, uint16_t stamp,
                      int *stack, LampBlob *found, int cap)
{
    int count = 0, lo = 255, hi = 0;   /* the members' range: most pixels are out of it, a compare without a table load */
    for (int e = 0; e < 256; e++)
        if (member[e]) lo = e < lo ? e : lo, hi = e > hi ? e : hi;
    if (lo > hi) return 0;
    const unsigned span = (unsigned)(hi - lo);
    for (int y = 0; y < h; y++) {
        const uint8_t *const row = pixels + (size_t)(y0 + y) * (size_t)pitch + (size_t)x0;
        for (int x = 0; x < w; x++) {
            if ((x & 31) == 0) {   /* a chunk of 32 with none in range is skipped by one branchless pass the compiler vectorizes */
                const int n = w - x < 32 ? w - x : 32;
                unsigned any = 0;
                for (int c = 0; c < n; c++) any |= (unsigned)((uint8_t)(row[x + c] - lo) <= span);
                if (!any) {
                    x += n - 1;
                    continue;
                }
            }
            const int at = y * w + x;
            const uint8_t seed = row[x];
            if ((unsigned)(seed - lo) > span || !member[seed] || seen[at] == stamp) continue;
            long sx = 0, sy = 0;
            int area = 0, top = 0;
            seen[at] = stamp, stack[top++] = at;
            while (top > 0) {
                const int p = stack[--top], px = p % w, py = p / w;
                sx += px, sy += py, area++;
                const int nx[4] = { px - 1, px + 1, px, px }, ny[4] = { py, py, py - 1, py + 1 };
                for (int k = 0; k < 4; k++) {
                    if (nx[k] < 0 || nx[k] >= w || ny[k] < 0 || ny[k] >= h) continue;
                    const int q = ny[k] * w + nx[k];
                    if (!member[pixels[(size_t)(y0 + ny[k]) * (size_t)pitch + (size_t)(x0 + nx[k])]] || seen[q] == stamp) continue;
                    seen[q] = stamp;
                    if (top < STACK) stack[top++] = q;
                }
            }
            if (count < cap) {
                found[count].cx = x0 + (int)(sx / area), found[count].cy = y0 + (int)(sy / area), found[count].area = area, found[count].value = seed;
                count++;
            }
        }
    }
    return count;
}

int lamp_blobs(const uint8_t *pixels, int pitch, int x0, int y0, int x1, int y1, const uint8_t member[256], LampBlob *out, int max)
{
    if (pixels == NULL || out == NULL || member == NULL || max <= 0 || x1 < x0 || y1 < y0) return 0;
    const int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if ((long)w * (long)h > 1 << 20) return 0;   /* a car's rectangle is never this big; a bad one would only cost time */
    uint16_t *seen = calloc((size_t)w * (size_t)h, sizeof *seen);
    int *stack = malloc(sizeof(int) * STACK);
    LampBlob found[LAMP_FLARE_BLOBS * 4];
    if (seen == NULL || stack == NULL) {
        free(seen), free(stack);
        return 0;
    }
    int count = scan_blobs(pixels, pitch, x0, y0, w, h, member, seen, 1, stack, found, (int)(sizeof found / sizeof found[0]));
    qsort(found, (size_t)count, sizeof found[0], by_area);
    if (count > max) count = max;
    memcpy(out, found, sizeof found[0] * (size_t)count);
    free(seen), free(stack);
    return count;
}

enum { FRAME_FOUND = 4096, FRAME_MAX_PIXELS = 1 << 24 };

int lamp_blobs_frame(const uint8_t *pixels, int pitch, int x0, int y0, int x1, int y1, const uint8_t member[256], LampBlob *out, int max)
{
    static uint16_t *seen;    /* kept between frames: the street lamps are scanned every one */
    static size_t seen_size;
    static uint16_t stamp;
    static int *stack;
    static LampBlob *found;
    if (pixels == NULL || out == NULL || member == NULL || max <= 0 || x1 < x0 || y1 < y0) return 0;
    const int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if ((long)w * (long)h > FRAME_MAX_PIXELS) return 0;
    const size_t need = (size_t)w * (size_t)h;
    if (need > seen_size) {
        uint16_t *grown = realloc(seen, need * sizeof *seen);
        if (grown == NULL) return 0;
        seen = grown, seen_size = need, stamp = 0;
        memset(seen, 0, need * sizeof *seen);
    }
    if (stack == NULL) stack = malloc(sizeof(int) * STACK);
    if (found == NULL) found = malloc(sizeof(LampBlob) * FRAME_FOUND);
    if (stack == NULL || found == NULL) return 0;
    if (++stamp == 0) {   /* the stamp wrapped: clear once in 65535 scans */
        memset(seen, 0, need * sizeof *seen);
        stamp = 1;
    }
    int count = scan_blobs(pixels, pitch, x0, y0, w, h, member, seen, stamp, stack, found, FRAME_FOUND);
    qsort(found, (size_t)count, sizeof found[0], by_area);
    if (count > max) count = max;
    memcpy(out, found, sizeof found[0] * (size_t)count);
    return count;
}

int lamp_flare_radius(int area)
{
    int r = 0;
    while ((r + 1) * (r + 1) * 3 <= area) r++;   /* the blob's radius: sqrt(area / 3), a little under sqrt(area / pi) */
    r = r * LAMP_FLARE_REACH / 10;
    return r < LAMP_FLARE_MIN_RADIUS ? LAMP_FLARE_MIN_RADIUS : r > LAMP_FLARE_MAX_RADIUS ? LAMP_FLARE_MAX_RADIUS : r;
}

int lamp_flare_radius_scaled(int area, int percent)
{
    const int r = lamp_flare_radius(area) * (percent < 100 ? 100 : percent) / 100;
    return r > LAMP_FLARE_BIG_RADIUS ? LAMP_FLARE_BIG_RADIUS : r;
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

int lamp_far_fade(int32_t z, int32_t far_z)
{
    if (far_z <= 0 || z <= far_z) return 100;
    if (z >= 2 * far_z) return 0;
    return (int)((int64_t)(2 * far_z - z) * 100 / far_z);
}
