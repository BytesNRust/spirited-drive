#include "game/race/street_lamp.h"

#include <ctype.h>
#include <string.h>

/* CY1's board (scenery texture 10): the lens is rows 7..9, x 48..62, the housing and arm above it (the transparent texels are 255) */
static const StreetLamp CY1_LAMP = { 10, 64, 128, { 46, 7, 63, 9 }, 55, 10, 5, 10, 255, 0 };

/* Cyberia's other two (CY2 and CY3, scenery texture 7, 172 and 163 placements): the same lamp as CY1's, the lens rows 7..9 at the arm's end; CY2's board is a
 * texel wider */
static const StreetLamp CY2_LAMP = { 7, 65, 128, { 46, 7, 64, 9 }, 55, 10, 5, 10, 255, 0 };
static const StreetLamp CY3_LAMP = { 7, 64, 128, { 46, 7, 63, 9 }, 55, 10, 5, 10, 255, 0 };

/* Transtropolis' (TR7) street lamp (scenery texture 28, 64 x 127, 79 placements): a grey head on an arch, the lens under it at rows 16..19, x 50..57 */
static const StreetLamp TR7_LAMP = { 28, 64, 127, { 50, 16, 57, 19 }, 54, 21, 5, 10, 255, 0 };

/* the foundry's truss (TR7, scenery texture 22, 194 x 24, 6 placements): two shaded bulbs hang from it, their lit undersides at rows 22 and 23, x 44..54 and 140..150 */
static const StreetLamp TR7_TRUSS_LAMPS[] = {
    { 22, 194, 24, { 44, 22, 54, 23 }, 49, 21, 3, 6, 255, 1 },
    { 22, 194, 24, { 140, 22, 150, 23 }, 145, 21, 3, 6, 255, 1 },
};

/* Lost Vegas' (TR4) burning barrels (scenery texture 29, 98 placements, and 31, one): the flame is already bright, so there is no lens, only the glow round it
 * on the board's transparent texels (rows 0..39, the barrel under it is left alone) */
static const StreetLamp TR4_BARRELS[] = {
    { 29, 50, 120, { 0, 0, 0, -1 }, 25, 24, 6, 20, 255, 1 },
    { 31, 50, 120, { 0, 0, 0, -1 }, 25, 24, 6, 20, 255, 1 },
};

/* CL1's board (scenery texture 10, 42 placements): the lamp with its arm; the lens is the amber at the arm's end, rows 7 and 8, x 76..84 (the housing above
 * it is grey, rows 0..6); the halo hangs under it */
static const StreetLamp CL1_LAMP = { 10, 87, 128, { 76, 7, 84, 8 }, 80, 10, 5, 10, 255, 0 };

/* Autumn Valley's (TR2) floodlight poles (scenery texture 5, 38 placements): four heads over rows 0..21 and the pole (x 17..19) under them; the lit faces are
 * the heads' lower rows, the halo hangs from their foot */
static const StreetLamp TR2_LAMP = { 5, 38, 148, { 0, 8, 37, 20 }, 18, 21, 5, 10, 255, 0 };

static int is_track(const char *track, const char *name)
{
    for (const char *t = track; *name != '\0'; t++, name++)
        if (toupper((unsigned char)*t) != *name) return 0;
    return 1;
}

/* Vertigo Ridge's (TR3): the lantern post (scenery texture 33, 19 placements: the glass at rows 8..25) and the wooden truss's hanging bulb (texture 34, 6
 * placements, in the tunnel mouths: the bulb at x 69..75, y 69..80, the halo round it on the board's transparent texels) */
static const StreetLamp TR3_LAMPS[] = {
    { 33, 16, 150, { 2, 8, 13, 25 }, 8, 26, 4, 9, 255, 0 },
    { 34, 144, 82, { 69, 69, 75, 80 }, 72, 75, 5, 14, 255, 1 },
};

int street_lamps_for(const char *track, StreetLamp *out, int max)
{
    if (track == NULL || out == NULL || max <= 0) return 0;
    if (is_track(track, "CY1")) out[0] = CY1_LAMP;
    else if (is_track(track, "CY2")) out[0] = CY2_LAMP;
    else if (is_track(track, "CY3")) out[0] = CY3_LAMP;
    else if (is_track(track, "TR4")) {
        int n = 0;
        for (; n < 2 && n < max; n++) out[n] = TR4_BARRELS[n];
        return n;
    } else if (is_track(track, "TR7")) {
        out[0] = TR7_LAMP;
        int n = 1;
        for (int i = 0; i < 2 && n < max; i++) out[n++] = TR7_TRUSS_LAMPS[i];
        return n;
    }
    else if (is_track(track, "CL1")) out[0] = CL1_LAMP;
    else if (is_track(track, "TR2")) out[0] = TR2_LAMP;
    else if (is_track(track, "TR3")) {
        int n = 0;
        for (; n < 2 && n < max; n++) out[n] = TR3_LAMPS[n];
        return n;
    } else return 0;
    return 1;
}

int street_lamps_for_day(const char *track, StreetLamp *out, int max)
{
    if (track == NULL || out == NULL || max <= 0) return 0;
    int n = 0;
    if (is_track(track, "TR3")) {
        for (; n < 2 && n < max; n++) out[n] = TR3_LAMPS[n];
    } else if (is_track(track, "TR7")) {
        for (; n < 2 && n < max; n++) out[n] = TR7_TRUSS_LAMPS[n];
    }
    return n;
}

int street_lamp_for(const char *track, StreetLamp *out)
{
    StreetLamp all[STREET_LAMPS_MAX];
    if (street_lamps_for(track, all, STREET_LAMPS_MAX) <= 0 || out == NULL) return 0;
    *out = all[0];
    return 1;
}

size_t street_lamp_nodes(const StreetLamp *lamp, const TriDescriptor *descriptors, uint32_t descriptor_count, const TriScenery *scenery, uint32_t scenery_count, uint32_t *out,
                         size_t max)
{
    size_t n = 0;
    if (lamp == NULL || descriptors == NULL || scenery == NULL || out == NULL) return 0;
    for (uint32_t i = 0; i < scenery_count && n < max; i++) {
        if (scenery[i].descriptor >= descriptor_count || scenery[i].node < 0) continue;
        const TriDescriptor *d = &descriptors[scenery[i].descriptor];
        if ((d->type == 4 || d->type == 6) && d->resource_id / 4 == lamp->scenery) out[n++] = (uint32_t)scenery[i].node;
    }
    return n;
}

size_t street_lamps_nodes(const StreetLamp *lamps, int lamp_count, const TriDescriptor *descriptors, uint32_t descriptor_count, const TriScenery *scenery,
                          uint32_t scenery_count, uint32_t *out, size_t max)
{
    size_t n = 0;
    if (lamps == NULL || out == NULL) return 0;
    for (int i = 0; i < lamp_count && n < max; i++) {
        int seen = 0;
        for (int j = 0; j < i; j++) seen |= lamps[j].scenery == lamps[i].scenery;
        if (!seen) n += street_lamp_nodes(&lamps[i], descriptors, descriptor_count, scenery, scenery_count, out + n, max - n);
    }
    return n;
}

static StreetLampPaint paint(const StreetLamp *lamp, uint8_t *pixels, int width, int height, const uint8_t entry[STREET_LAMP_ENTRIES], int rim_on)
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
    for (int y = lamp->halo_around ? (lamp->halo_y - lamp->rim > 0 ? lamp->halo_y - lamp->rim : 0) : below + 1; y < height && y <= lamp->halo_y + lamp->rim; y++)
        for (int x = lamp->halo_x - lamp->rim; x <= lamp->halo_x + lamp->rim; x++) {
            if (x < 0 || x >= width) continue;
            uint8_t *t = &pixels[y * width + x];
            if (*t != lamp->transparent) continue;
            const int dx = x - lamp->halo_x, dy = y - lamp->halo_y, d2 = dx * dx + dy * dy;
            if (d2 <= lamp->core * lamp->core && (x + y) % 2 == 0) *t = entry[1], done.halo++;                       /* the core: a checker */
            else if (rim_on && d2 > lamp->core * lamp->core && d2 <= lamp->rim * lamp->rim && x % 2 == 0 && y % 2 == 0) *t = entry[2], done.halo++;   /* the rim: one in four */
        }
    return done;
}

StreetLampPaint street_lamp_paint(const StreetLamp *lamp, uint8_t *pixels, int width, int height, const uint8_t entry[STREET_LAMP_ENTRIES])
{
    return paint(lamp, pixels, width, height, entry, 1);
}

StreetLampPaint street_lamp_paint_day(const StreetLamp *lamp, uint8_t *pixels, int width, int height, const uint8_t entry[STREET_LAMP_ENTRIES])
{
    return paint(lamp, pixels, width, height, entry, 0);
}
