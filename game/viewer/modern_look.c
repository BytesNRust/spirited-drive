/* port: EXTRAS' Texture Filter and Fog for a track (modern_look.h). */
#include "viewer/modern_look.h"

#include <string.h>

/* the farthest road is about 857 deep a segment drawn (24000 at 0x1c, 43000 at ultra's): the fog whole a little
 * before it, so the last strip melts into the horizon instead of ending; from a tenth of the way, a haze over the middle distance */
static const float FOG_END_PER_SEGMENT = 780.0f, FOG_START_SHARE = 0.1f;

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
    for (int k = 0; k < 3; k++) {
        if (count == 0) {
            rgb[k] = MODERN_LOOK_NO_HORIZON;
            continue;
        }
        const uint32_t foot = (sum[k] + count / 2) / count;
        rgb[k] = (uint8_t)(foot + ((255 - foot) * MODERN_LOOK_LIFT_PERCENT + 50) / 100);
    }
}

const ModernStyle *modern_look_style(ModernLook *look, const TrackArt *art, int filter, int fog, int32_t segments)
{
    if (!filter && !fog) return NULL;
    look->style = (ModernStyle){ filter != 0, NULL, 0, 0 };
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
