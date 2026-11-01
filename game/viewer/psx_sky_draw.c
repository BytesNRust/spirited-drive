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

void psx_sky_draw_free(PsxSkyDraw *d)
{
    psx_sky_bitmap_free(&d->bitmap);
    free(d->mapped);
    d->mapped = NULL, d->mapped_ok = 0;
    d->ready = 0;
    d->smooth.ready = 0;
}
