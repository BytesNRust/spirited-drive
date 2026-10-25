/* port: see psx_sky_bitmap.h. */
#include "game/race/psx_sky_bitmap.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { BINS = 1 << 15, COLOURS = 256 };

/* the magnified image: the gradient rows, then the panorama's rows each PSX_SKY_ZOOM times, blended */
static void fill_image(const PsxSky *sky, const PsxSkyInf *inf, int width, int height, uint8_t *rgb)
{
    for (int y = 0; y < PSX_SKY_GRADIENT_ROWS; y++) {
        const float t = (float)y / (float)(PSX_SKY_GRADIENT_ROWS - 1);
        for (int x = 0; x < width; x++)
            for (int k = 0; k < 3; k++)
                rgb[3 * ((size_t)y * width + x) + k] = (uint8_t)lroundf((float)inf->top[k] + t * (float)(inf->bottom[k] - inf->top[k]));
    }
    for (int y = PSX_SKY_GRADIENT_ROWS; y < height; y++) {
        const float sy = ((float)(y - PSX_SKY_GRADIENT_ROWS) + 0.5f) / PSX_SKY_ZOOM - 0.5f;
        int y0 = (int)floorf(sy);
        const float fy = sy - (float)y0;
        int y1 = y0 + 1;
        y0 = y0 < 0 ? 0 : y0, y1 = y1 >= sky->height ? sky->height - 1 : y1;
        for (int x = 0; x < width; x++) {
            const float sx = ((float)x + 0.5f) / PSX_SKY_ZOOM - 0.5f + (float)(sky->width * PSX_SKY_TURN_NUM / PSX_SKY_TURN_DEN);
            const int x0 = (int)floorf(sx);
            const float fx = sx - (float)x0;
            const int c0 = (x0 % sky->width + sky->width) % sky->width, c1 = (c0 + 1) % sky->width;
            for (int k = 0; k < 3; k++) {
                const float top = (1 - fx) * sky->rgb[3 * ((size_t)y0 * sky->width + c0) + k] + fx * sky->rgb[3 * ((size_t)y0 * sky->width + c1) + k];
                const float bottom = (1 - fx) * sky->rgb[3 * ((size_t)y1 * sky->width + c0) + k] + fx * sky->rgb[3 * ((size_t)y1 * sky->width + c1) + k];
                rgb[3 * ((size_t)y * width + x) + k] = (uint8_t)lroundf((1 - fy) * top + fy * bottom);
            }
        }
    }
}

typedef struct {
    int lo, hi;      /* the occupied bins [lo, hi) of the sorted list */
    uint64_t count;
} Box;

static int sort_channel;   /* 0 red, 1 green, 2 blue, for the comparison: set before each sort */

static int channel_of(int bin, int c) { return bin >> (10 - 5 * c) & 31; }

static int compare_bins(const void *a, const void *b)
{
    return channel_of(*(const int *)a, sort_channel) - channel_of(*(const int *)b, sort_channel);
}

/* the channel along which the box's bins spread widest, and the spread */
static int widest(const int *bins, const Box *box, int *spread)
{
    int best = 0, best_spread = -1;
    for (int c = 0; c < 3; c++) {
        int lo = 31, hi = 0;
        for (int i = box->lo; i < box->hi; i++) {
            const int v = channel_of(bins[i], c);
            lo = v < lo ? v : lo, hi = v > hi ? v : hi;
        }
        if (hi - lo > best_spread) best = c, best_spread = hi - lo;
    }
    *spread = best_spread;
    return best;
}

/* the colours: median cut of the occupied bins, each box the average of its pixels; remap[bin] the box it fell in */
static int cut(const uint32_t *counts, uint8_t palette[768], uint8_t remap[BINS])
{
    int *bins = malloc(sizeof(int) * BINS);
    if (bins == NULL) return 0;
    int n = 0;
    for (int b = 0; b < BINS; b++)
        if (counts[b] != 0) bins[n++] = b;
    Box boxes[COLOURS];
    int box_count = 1;
    boxes[0] = (Box){ 0, n, 0 };
    for (int i = 0; i < n; i++) boxes[0].count += counts[bins[i]];
    while (box_count < COLOURS) {
        int pick = -1;
        for (int i = 0; i < box_count; i++)
            if (boxes[i].hi - boxes[i].lo > 1 && (pick < 0 || boxes[i].count > boxes[pick].count)) pick = i;
        if (pick < 0) break;
        const Box box = boxes[pick];
        int spread;
        sort_channel = widest(bins, &box, &spread);
        qsort(bins + box.lo, (size_t)(box.hi - box.lo), sizeof(int), compare_bins);
        uint64_t seen = 0;
        int at = box.lo;
        while (at < box.hi - 1 && seen + counts[bins[at]] < (box.count + 1) / 2) seen += counts[bins[at++]];
        at++;   /* the median bin ends the lower half */
        if (at >= box.hi) at = box.hi - 1;
        Box lower = { box.lo, at, 0 }, upper = { at, box.hi, 0 };
        for (int i = lower.lo; i < lower.hi; i++) lower.count += counts[bins[i]];
        for (int i = upper.lo; i < upper.hi; i++) upper.count += counts[bins[i]];
        boxes[pick] = lower, boxes[box_count++] = upper;
    }
    memset(palette, 0, 768);
    for (int i = 0; i < box_count; i++) {
        uint64_t sum[3] = { 0, 0, 0 };
        for (int j = boxes[i].lo; j < boxes[i].hi; j++) {
            for (int c = 0; c < 3; c++) sum[c] += (uint64_t)counts[bins[j]] * (uint64_t)(channel_of(bins[j], c) * 8 + 4);
            remap[bins[j]] = (uint8_t)i;
        }
        for (int c = 0; c < 3; c++) palette[3 * i + c] = (uint8_t)(boxes[i].count != 0 ? (sum[c] + boxes[i].count / 2) / boxes[i].count : 0);
    }
    free(bins);
    return 1;
}

int psx_sky_bitmap_build(const PsxSky *sky, const PsxSkyInf *inf, PsxSkyBitmap *out)
{
    memset(out, 0, sizeof *out);
    if (sky == NULL || sky->rgb == NULL || sky->width <= 0 || sky->height <= 0) return 0;
    const int width = sky->width * PSX_SKY_ZOOM, height = PSX_SKY_GRADIENT_ROWS + sky->height * PSX_SKY_ZOOM;
    const size_t count = (size_t)width * height;
    uint8_t *rgb = malloc(count * 3), *pixels = malloc(count);
    uint32_t *counts = calloc(BINS, sizeof *counts);
    uint8_t *remap = malloc(BINS);
    int ok = rgb != NULL && pixels != NULL && counts != NULL && remap != NULL;
    if (ok) {
        fill_image(sky, inf, width, height, rgb);
        for (size_t i = 0; i < count; i++) counts[(rgb[3 * i] >> 3) << 10 | (rgb[3 * i + 1] >> 3) << 5 | rgb[3 * i + 2] >> 3]++;
        ok = cut(counts, out->palette, remap);
    }
    if (ok) {
        for (size_t i = 0; i < count; i++)
            pixels[i] = remap[(rgb[3 * i] >> 3) << 10 | (rgb[3 * i + 1] >> 3) << 5 | rgb[3 * i + 2] >> 3];
        out->width = width, out->height = height, out->horizon_row = PSX_SKY_GRADIENT_ROWS + PSX_SKY_ZOOM * (sky->height * PSX_SKY_HORIZON_NUM / PSX_SKY_HORIZON_DEN), out->pixels = pixels;
        pixels = NULL;
    }
    free(rgb), free(pixels), free(counts), free(remap);
    return ok;
}

void psx_sky_bitmap_free(PsxSkyBitmap *bitmap)
{
    free(bitmap->pixels);
    bitmap->pixels = NULL;
}
