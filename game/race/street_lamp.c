#include "game/race/street_lamp.h"

#include <ctype.h>
#include <string.h>

/* CY1's board (scenery texture 10): the lens is rows 7..9, x 48..62, the housing and arm above it (the transparent texels are 255) */
static const StreetLamp CY1_LAMP = { 10, 64, 128, { 46, 7, 63, 9 }, 55, 10, 5, 10, 255 };

int street_lamp_for(const char *track, StreetLamp *out)
{
    if (track == NULL || out == NULL) return 0;
    const char *name = "CY1";
    for (const char *t = track; *name != '\0'; t++, name++)
        if (toupper((unsigned char)*t) != *name) return 0;
    *out = CY1_LAMP;
    return 1;
}

StreetLampPaint street_lamp_paint(const StreetLamp *lamp, uint8_t *pixels, int width, int height, const uint8_t entry[STREET_LAMP_ENTRIES])
{
    StreetLampPaint done = { 0, 0 };
    if (lamp == NULL || pixels == NULL || entry == NULL || width != lamp->width || height != lamp->height) return done;
    const int below = lamp->lens_rect[3];
    for (int y = lamp->lens_rect[1]; y <= below; y++)
        for (int x = lamp->lens_rect[0]; x <= lamp->lens_rect[2] && x < width; x++) {
            uint8_t *t = &pixels[y * width + x];
            if (*t == lamp->transparent || *t == entry[0]) continue;
            *t = entry[0];
            done.lens++;
        }
    for (int y = below + 1; y < height && y <= lamp->halo_y + lamp->rim; y++)
        for (int x = lamp->halo_x - lamp->rim; x <= lamp->halo_x + lamp->rim; x++) {
            if (x < 0 || x >= width) continue;
            uint8_t *t = &pixels[y * width + x];
            if (*t != lamp->transparent) continue;
            const int dx = x - lamp->halo_x, dy = y - lamp->halo_y, d2 = dx * dx + dy * dy;
            if (d2 <= lamp->core * lamp->core && (x + y) % 2 == 0) *t = entry[1], done.halo++;                       /* the core: a checker */
            else if (d2 > lamp->core * lamp->core && d2 <= lamp->rim * lamp->rim && x % 2 == 0 && y % 2 == 0) *t = entry[2], done.halo++;   /* the rim: one in four */
        }
    return done;
}
