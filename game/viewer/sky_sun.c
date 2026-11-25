#include "game/viewer/sky_sun.h"

#include "game/race/lamp_flare.h"
#include "game/sys/palette.h"
#include "formats/fixed_math.h"

#include <math.h>
#include <string.h>

enum { FAR = 1 << 20 };   /* the direction's length in the 16.16 camera space: only its ratios matter */

int sky_sun_place(const View *view, const int32_t matrix[9], float azimuth, float elevation, SkySunDisc *out)
{
    if (view == NULL || matrix == NULL || out == NULL) return 0;
    const double ce = cos((double)elevation);
    const int32_t world[3] = { (int32_t)lrint(ce * cos((double)azimuth) * FAR), (int32_t)lrint(sin((double)elevation) * FAR),
                               (int32_t)lrint(ce * sin((double)azimuth) * FAR) };
    int32_t cam[3], screen[2];
    mat3_apply(world, matrix, cam);
    if (!view_project(view, cam, screen)) return 0;
    const double pi = 3.14159265358979323846, focal = (double)view->focal_x;
    out->x = screen[0], out->y = screen[1];
    out->core = (int)lrint(focal * tan(SKY_SUN_CORE_TENTHS / 10.0 * pi / 180.0));
    out->halo = (int)lrint(focal * tan(SKY_SUN_HALO_TENTHS / 10.0 * pi / 180.0));
    if (out->core < 2) out->core = 2;
    if (out->halo < out->core + 2) out->halo = out->core + 2;
    return 1;
}

/* the sun's colour added to a pixel's, in percent by dither level: a wide faint halo and a bright disc that saturates */
static const int HALO_PERCENT[LAMP_FLARE_LEVELS] = { 10, 22, 38 }, CORE_PERCENT[LAMP_FLARE_LEVELS] = { 55, 90, 140 };
static const int SUN_RGB[3] = { 255, 238, 196 };

void sky_sun_draw(const SkySunDisc *disc, const uint8_t palette[768], uint8_t *pixels, int pitch, int x0, int y0, int x1, int y1)
{
    if (disc == NULL || palette == NULL || pixels == NULL) return;
    static uint8_t maps[2][LAMP_FLARE_LEVELS][256], made_palette[768];
    static int made;
    const uint8_t *const usable = palette_usable();
    if (!made || memcmp(made_palette, palette, 768) != 0) {   /* made again only when the palette changes */
        for (int k = 0; k < 2; k++)
            for (int l = 0; l < LAMP_FLARE_LEVELS; l++)
                for (int d = 0; d < 256; d++) {
                    int rgb[3];
                    for (int c = 0; c < 3; c++) {
                        rgb[c] = palette[3 * d + c] + SUN_RGB[c] * (k == 0 ? HALO_PERCENT[l] : CORE_PERCENT[l]) / 100;
                        if (rgb[c] > 255) rgb[c] = 255;
                    }
                    maps[k][l][d] = usable[d] ? (uint8_t)palette_nearest(palette, usable, rgb[0], rgb[1], rgb[2]) : (uint8_t)d;
                }
        memcpy(made_palette, palette, 768), made = 1;
    }
    const uint8_t *const halo[LAMP_FLARE_LEVELS] = { maps[0][0], maps[0][1], maps[0][2] };
    const uint8_t *const core[LAMP_FLARE_LEVELS] = { maps[1][0], maps[1][1], maps[1][2] };
    lamp_halo_draw(pixels, pitch, x0, y0, x1, y1, disc->x, disc->y, disc->halo, halo);
    lamp_halo_draw(pixels, pitch, x0, y0, x1, y1, disc->x, disc->y, disc->core, core);
}
