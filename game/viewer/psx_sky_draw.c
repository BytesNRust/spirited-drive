/* port: see psx_sky_draw.h. */
#include "viewer/psx_sky_draw.h"

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

void psx_sky_draw_target(PsxSkyDraw *d, const uint8_t target[768])
{
    if (d->ready) horizon_smooth_sky(&d->smooth, d->bitmap.palette, target);
}

void psx_sky_draw_free(PsxSkyDraw *d)
{
    psx_sky_bitmap_free(&d->bitmap);
    d->ready = 0;
    d->smooth.ready = 0;
}
