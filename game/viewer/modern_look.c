/* port: EXTRAS' Texture Filter and Fog for a track (modern_look.h). */
#include "viewer/modern_look.h"

#include <string.h>

/* the farthest road is about 857 deep a segment drawn (24000 at 0x1c, 43000 at ultra's): the fog whole a little
 * before it, so the last strip melts into the horizon instead of ending; from a tenth of the way, a haze over the middle distance */
static const float FOG_END_PER_SEGMENT = 780.0f, FOG_START_SHARE = 0.1f;

/* a colour's grey (Rec. 601 luma) */
static int grey_of(const int c[3])
{
    return (299 * c[0] + 587 * c[1] + 114 * c[2] + 500) / 1000;
}

void modern_look_fog_colour(const TrackArt *art, uint8_t rgb[3])
{
    const FshEntry *h = &art->horizon;
    uint32_t sum[3] = { 0 }, count = 0;
    if (art->has_horizon && h->payload != NULL) {
        const int from = h->height > MODERN_LOOK_FOOT_ROWS ? h->height - MODERN_LOOK_FOOT_ROWS : 0;
        for (int y = from; y < h->height; y++)
            for (int x = 0; x < h->width; x++) {
                const size_t at = (size_t)y * h->width + x;
                if (at >= h->payload_size || h->payload[at] == 0xff) continue;
                for (int k = 0; k < 3; k++) sum[k] += art->palette[3 * h->payload[at] + k];
                count++;
            }
    }
    if (count == 0) {
        rgb[0] = rgb[1] = rgb[2] = MODERN_LOOK_NO_HORIZON;
        return;
    }
    int c[3];
    for (int k = 0; k < 3; k++) c[k] = (int)((sum[k] + count / 2) / count);
    if (grey_of(c) < MODERN_LOOK_NIGHT) {
        for (int k = 0; k < 3; k++) rgb[k] = (uint8_t)c[k];
        return;
    }
    for (int k = 0; k < 3; k++) c[k] += ((255 - c[k]) * MODERN_LOOK_LIFT_PERCENT + 50) / 100;
    const int grey = grey_of(c);
    for (int k = 0; k < 3; k++) rgb[k] = (uint8_t)(c[k] + ((grey - c[k]) * MODERN_LOOK_GREY_PERCENT) / 100);
}

const ModernStyle *modern_look_style(ModernLook *look, const TrackArt *art, int filter, int fog, int32_t segments)
{
    if (!filter && !fog) return NULL;
    look->style = (ModernStyle){ filter != 0, NULL, 0, 0, 0, 0 };
    if (!fog) return &look->style;
    uint8_t rgb[3];
    modern_look_fog_colour(art, rgb);
    if (!look->made || memcmp(look->palette, art->palette, sizeof look->palette) != 0 ||
        memcmp(look->rgb, rgb, sizeof rgb) != 0) {
        modern_fog_tables(art->palette, rgb, look->tables);
        memcpy(look->palette, art->palette, sizeof look->palette);
        memcpy(look->rgb, rgb, sizeof rgb);
        look->made = 1;
    }
    look->style.fog = (const uint8_t (*)[256])look->tables;
    look->style.fog_end = FOG_END_PER_SEGMENT * (float)segments;
    look->style.fog_start = look->style.fog_end * FOG_START_SHARE;
    return &look->style;
}

const ModernStyle *modern_look_fixed_fog(ModernLook *look, const TrackArt *art, int filter, const uint8_t rgb[3], float start,
                                         float end)
{
    look->style = (ModernStyle){ filter != 0, NULL, 0, 0, 0, 0 };
    if (!look->made || memcmp(look->palette, art->palette, sizeof look->palette) != 0 ||
        memcmp(look->rgb, rgb, sizeof look->rgb) != 0) {
        modern_fog_tables(art->palette, rgb, look->tables);
        memcpy(look->palette, art->palette, sizeof look->palette);
        memcpy(look->rgb, rgb, sizeof look->rgb);
        look->made = 1;
    }
    look->style.fog = (const uint8_t (*)[256])look->tables;
    look->style.fog_start = start;
    look->style.fog_end = end;
    return &look->style;
}
