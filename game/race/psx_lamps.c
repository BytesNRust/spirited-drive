#include "game/race/psx_lamps.h"

#include <ctype.h>
#include <string.h>

/* CY1: the ceiling's white oval (x 33-41, y 68-76 of byte 38's 80 x 80); CY2: the amber fitting (byte 37); CY3: the ceiling lamp (byte 20)
 * and the wall lights (byte 19); TR4: the orange strips of byte 20 (128 x 128, y 15-48). AL1 and AL3 have none. */
enum { ALL = 255 };   /* a rectangle's far edge: the texture's own */
static const PsxLamp CY1[] = { { 38, 5, { 0, 74, 87, 63, 67 }, { 33, 68, 41, 76 }, NULL } };
static const PsxLamp CY2[] = { { 37, 4, { 75, 53, 32, 44 }, { 43, 50, 77, 60 }, NULL } };
static const PsxLamp CY3[] = { { 20, 4, { 0, 38, 67, 77 }, { 34, 0, 42, 15 }, NULL }, { 19, 3, { 55, 68, 87 }, { 0, 65, ALL, 69 }, NULL } };
/* TR3 (Vertigo Ridge): the amber bulb in the tunnel portal's ceiling (byte 19, 128 x 128, the strip at x 46-78, y 2-13) */
static const PsxLamp TR3[] = { { 19, 0, { 0 }, { 46, 2, 78, 13 }, NULL }, { 21, 0, { 0 }, { 36, 2, 61, 11 }, NULL } };   /* byte 21: 102 x 102, the strip at x 36-61, y 2-11 */
static const PsxLamp TR4[] = { { 20, 2, { 60, 29 }, { 0, 15, ALL, 48 }, NULL } };

static const struct { const char *track; const PsxLamp *lamps; int count; } TRACKS[] = {
    { "CY1", CY1, 1 }, { "CY2", CY2, 1 }, { "CY3", CY3, 2 }, { "TR3", TR3, 2 }, { "TR4", TR4, 1 },
};

static int same_name(const char *a, const char *b)
{
    for (; *a != '\0' && *b != '\0'; a++, b++)
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
    return *a == *b;
}

int psx_lamps_for(const char *track, const PsxLamp **out)
{
    if (track == NULL) return 0;
    for (size_t i = 0; i < sizeof TRACKS / sizeof *TRACKS; i++)
        if (same_name(track, TRACKS[i].track)) {
            if (out != NULL) *out = TRACKS[i].lamps;
            return TRACKS[i].count;
        }
    return 0;
}

void psx_lamp_exempt(const PsxLamp *lamp, const uint8_t *pixels, int w, int h, uint8_t exempt[256])
{
    uint8_t inside[256] = { 0 }, outside[256] = { 0 };
    memset(exempt, 0, 256);
    if (lamp->set != NULL) {   /* the marks name the colours: those under the yellow */
        memcpy(exempt, lamp->set, 256);
        return;
    }
    for (int k = 0; k < lamp->count; k++) exempt[lamp->idx[k]] = 1;
    if (pixels == NULL || w <= 0 || h <= 0) return;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const int in = x >= lamp->rect[0] && x <= lamp->rect[2] && y >= lamp->rect[1] && y <= lamp->rect[3];
            (in ? inside : outside)[pixels[y * w + x]] = 1;
        }
    for (int c = 0; c < 256; c++)
        if (inside[c] && !outside[c]) exempt[c] = 1;
}

void psx_lamp_tables(const uint8_t exempt[256], const uint8_t (*lit)[256], int levels, uint8_t (*out)[256])
{
    memcpy(out, lit, (size_t)levels * 256);
    for (int l = 0; l < levels; l++)
        for (int c = 0; c < 256; c++)
            if (exempt[c]) out[l][c] = (uint8_t)c;
}

size_t psx_lamp_recolour(uint8_t *pixels, size_t count, const uint8_t remap[256])
{
    size_t changed = 0;
    for (size_t i = 0; pixels != NULL && i < count; i++)
        if (remap[pixels[i]] != pixels[i]) pixels[i] = remap[pixels[i]], changed++;
    return changed;
}

size_t psx_lamp_recolour_rect(const PsxLamp *lamp, uint8_t *pixels, int w, int h, const uint8_t remap[256])
{
    size_t changed = 0;
    if (lamp == NULL || pixels == NULL || w <= 0 || h <= 0) return 0;
    for (int y = lamp->rect[1]; y <= lamp->rect[3] && y < h; y++)
        for (int x = lamp->rect[0]; x <= lamp->rect[2] && x < w; x++) {
            uint8_t *p = &pixels[y * w + x];
            if (remap[*p] != *p) *p = remap[*p], changed++;
        }
    return changed;
}
