/* port: see psx_sky_draw.h. */
#include "viewer/psx_sky_draw.h"

#include <stdlib.h>
#include <string.h>

int psx_sky_draw_make(PsxSkyDraw *d, const PsxSky *sky, const PsxSkyInf *inf)
{
    psx_sky_draw_free(d);
    if (!psx_sky_bitmap_build(sky, inf, &d->bitmap)) return 0;
    d->horz = (FshEntry){ { 0 }, FSH_TYPE_BITMAP, 0, (uint16_t)d->bitmap.width, (uint16_t)d->bitmap.height,
                          { 0, (int16_t)d->bitmap.horizon_row, 0, 0 }, d->bitmap.pixels,
                          (size_t)d->bitmap.width * (size_t)d->bitmap.height };
    d->ready = 1;
    return 1;
}

int psx_sky_draw_make_indexed(PsxSkyDraw *d, const FshEntry *horizon, const uint8_t palette[768])
{
    psx_sky_draw_free(d);
    if (horizon == NULL || horizon->payload == NULL || horizon->width == 0 || horizon->height == 0) return 0;
    const size_t count = (size_t)horizon->width * horizon->height;
    if (count > horizon->payload_size) return 0;
    uint8_t *pixels = malloc(count);
    if (pixels == NULL) return 0;
    memcpy(pixels, horizon->payload, count);
    d->bitmap = (PsxSkyBitmap){ horizon->width, horizon->height, horizon->misc[1], pixels, { 0 } };
    memcpy(d->bitmap.palette, palette, 768);
    d->horz = *horizon;
    d->horz.payload = pixels;
    d->horz.payload_size = count;
    d->ready = 1;
    return 1;
}

const FshEntry *psx_sky_draw_in_palette(PsxSkyDraw *d)
{
    if (!d->ready || !d->smooth.ready) return NULL;
    const size_t count = d->horz.payload_size;
    if (d->mapped == NULL) {
        d->mapped = malloc(count);
        if (d->mapped == NULL) return NULL;
        d->mapped_ok = 0;
    }
    if (!d->mapped_ok || memcmp(d->mapped_map, d->smooth.map, sizeof d->mapped_map) != 0) {
        for (size_t i = 0; i < count; i++) d->mapped[i] = d->smooth.map[d->horz.payload[i]];
        memcpy(d->mapped_map, d->smooth.map, sizeof d->mapped_map);
        d->mapped_ok = 1;
    }
    d->in_palette = d->horz;
    d->in_palette.payload = d->mapped;
    return &d->in_palette;
}

void psx_sky_draw_target(PsxSkyDraw *d, const uint8_t target[768])
{
    if (d->ready) horizon_smooth_sky(&d->smooth, d->bitmap.palette, target);
}

void psx_sky_draw_scale(PsxSkyDraw *d, const float mul[3])
{
    if (!d->ready) return;
    for (int i = 0; i < 256; i++)
        for (int k = 0; k < 3; k++) {
            const float v = (float)d->bitmap.palette[3 * i + k] * mul[k] + 0.5f;
            d->bitmap.palette[3 * i + k] = (uint8_t)(v > 255.0f ? 255.0f : v);
        }
    d->smooth.ready = 0, d->mapped_ok = 0;   /* matched again at the next target */
}

void psx_sky_draw_rehue(PsxSkyDraw *d, const float ramp[3])
{
    if (!d->ready) return;
    const float ramp_luma = 0.30f * ramp[0] + 0.59f * ramp[1] + 0.11f * ramp[2];
    if (ramp_luma <= 0.0f) return;
    for (int i = 0; i < 256; i++) {
        uint8_t *const c = d->bitmap.palette + 3 * i;
        const float luma = 0.30f * c[0] + 0.59f * c[1] + 0.11f * c[2];
        for (int k = 0; k < 3; k++) {
            const float v = luma * ramp[k] / ramp_luma + 0.5f;
            c[k] = (uint8_t)(v > 255.0f ? 255.0f : v);
        }
    }
    d->smooth.ready = 0, d->mapped_ok = 0;   /* matched again at the next target */
}

/* the stars go down the sky above its horizon row, thinning toward it (a band of the bitmap's upper rows put none in a PS1 panorama's view, whose 448 rows
 * show only the lowest ones: TR1), and off the land: a column's silhouette starts at the first row whose
 * rows (their mean, over STAR_EDGE_ROWS) are under STAR_EDGE_PERCENT of the ones above them: a step, which a gradient darkening to the horizon, dithered or not,
 * never makes; no star is put on it or the row over it. The density is raised by the band's share
 * (STAR_TOP_PER_MILLE). */
enum { STAR_GREYS = 3, STAR_DARK_LUMA = 90, STAR_HORIZON_GAP = 2, STAR_TOP_PER_MILLE = 7, STAR_EDGE_ROWS = 4,
       STAR_EDGE_PERCENT = 60, STAR_EDGE_FROM_LUMA = 6 };
static const uint8_t STAR_GREY[STAR_GREYS] = { 110, 170, 235 };

static uint32_t star_hash(uint32_t x, uint32_t y, uint32_t seed)
{
    uint32_t h = x * 0x9e3779b1u ^ y * 0x85ebca6bu ^ seed * 0xc2b2ae35u;
    h ^= h >> 15, h *= 0x2c1b3c6du, h ^= h >> 12, h *= 0x297a2d39u, h ^= h >> 15;
    return h;
}

static int luma_of(const uint8_t *palette, int i)
{
    return (299 * palette[3 * i] + 587 * palette[3 * i + 1] + 114 * palette[3 * i + 2]) / 1000;
}

/* the STAR_GREYS entries the picture uses least, their texels moved to the nearest of the others */
static void free_entries(PsxSkyDraw *d, int chosen[STAR_GREYS])
{
    const size_t count = (size_t)d->bitmap.width * (size_t)d->bitmap.height;
    size_t used[256] = { 0 };
    for (size_t i = 0; i < count; i++) used[d->bitmap.pixels[i]]++;
    for (int k = 0; k < STAR_GREYS; k++) {
        int best = -1;
        for (int e = 0; e < 256; e++) {
            int taken = 0;
            for (int j = 0; j < k; j++) taken |= chosen[j] == e;
            if (!taken && (best < 0 || used[e] < used[best])) best = e;
        }
        chosen[k] = best;
    }
    uint8_t into[256];
    for (int e = 0; e < 256; e++) {
        into[e] = (uint8_t)e;
        int is_chosen = 0;
        for (int k = 0; k < STAR_GREYS; k++) is_chosen |= chosen[k] == e;
        if (!is_chosen || used[e] == 0) continue;
        long best_d = -1;
        for (int o = 0; o < 256; o++) {
            int other_chosen = 0;
            for (int k = 0; k < STAR_GREYS; k++) other_chosen |= chosen[k] == o;
            if (other_chosen || used[o] == 0) continue;
            long dist = 0;
            for (int c = 0; c < 3; c++) {
                const long diff = (long)d->bitmap.palette[3 * e + c] - d->bitmap.palette[3 * o + c];
                dist += diff * diff;
            }
            if (best_d < 0 || dist < best_d) best_d = dist, into[e] = (uint8_t)o;
        }
    }
    for (size_t i = 0; i < count; i++) d->bitmap.pixels[i] = into[d->bitmap.pixels[i]];
}

/* the first row of column x that the land takes (the sky's, from the first row down), or `horizon` when the column has none: the row from which the mean of the
 * next STAR_EDGE_ROWS rows is under STAR_EDGE_PERCENT of the mean of the STAR_EDGE_ROWS before it (means, so a dithered or noisy gradient is not an edge) */
static int silhouette_top(const PsxSkyDraw *d, int x, int horizon)
{
    const int width = d->bitmap.width;
    for (int y = STAR_EDGE_ROWS; y + STAR_EDGE_ROWS <= horizon; y++) {
        int above = 0, below = 0;
        for (int k = 0; k < STAR_EDGE_ROWS; k++) {
            above += luma_of(d->bitmap.palette, d->bitmap.pixels[(size_t)(y - 1 - k) * (size_t)width + (size_t)x]);
            below += luma_of(d->bitmap.palette, d->bitmap.pixels[(size_t)(y + k) * (size_t)width + (size_t)x]);
        }
        if (above >= STAR_EDGE_FROM_LUMA * STAR_EDGE_ROWS && below * 100 < above * STAR_EDGE_PERCENT) return y;
    }
    return horizon;
}

int psx_sky_draw_stars(PsxSkyDraw *d, uint32_t seed)
{
    if (!d->ready || d->bitmap.pixels == NULL) return 0;
    const int width = d->bitmap.width, horizon = d->bitmap.horizon_row;
    const int limit = horizon - STAR_HORIZON_GAP;
    if (width <= 0 || limit <= 0 || horizon > d->bitmap.height) return 0;
    int chosen[STAR_GREYS];
    free_entries(d, chosen);
    for (int k = 0; k < STAR_GREYS; k++)
        for (int c = 0; c < 3; c++) d->bitmap.palette[3 * chosen[k] + c] = STAR_GREY[k];
    int *const tops = malloc((size_t)width * sizeof *tops);   /* each column's land, found once */
    if (tops == NULL) return 0;
    d->star_rows = limit, d->star_land_columns = 0;
    for (int x = 0; x < width; x++) {
        tops[x] = silhouette_top(d, x, horizon);
        if (tops[x] < limit + 1) d->star_land_columns++;
    }
    int stars = 0;
    for (int y = 0; y < limit; y++) {
        const uint32_t per_mille = 1 + (uint32_t)(STAR_TOP_PER_MILLE * (limit - y) / limit);   /* thinning to the horizon */
        for (int x = 0; x < width; x++) {
            if (y >= tops[x] - 1) continue;
            uint8_t *texel = &d->bitmap.pixels[(size_t)y * (size_t)width + (size_t)x];
            const uint32_t h = star_hash((uint32_t)x, (uint32_t)y, seed);
            if (h % 1000 >= per_mille || luma_of(d->bitmap.palette, *texel) >= STAR_DARK_LUMA) continue;
            const uint32_t pick = (h >> 12) % 8;   /* one in eight bright, three in eight middle, the rest dim */
            *texel = (uint8_t)chosen[pick == 0 ? 2 : pick < 4 ? 1 : 0];
            stars++;
        }
    }
    free(tops);
    d->smooth.ready = 0, d->mapped_ok = 0;   /* matched again at the next target */
    return stars;
}

void psx_sky_draw_free(PsxSkyDraw *d)
{
    psx_sky_bitmap_free(&d->bitmap);
    free(d->mapped);
    d->mapped = NULL, d->mapped_ok = 0;
    d->ready = 0;
    d->smooth.ready = 0;
}
