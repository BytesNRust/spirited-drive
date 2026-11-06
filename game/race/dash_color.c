/* port: see dash_color.h. */
#include "game/race/dash_color.h"
#include "game/race/dash_mask.h"
#include "game/platform/platform.h"

#include "formats/png.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_SIDE = 4096, DARK_BELOW = 24, SLOT_NEAR = 48 };

static int distance(const uint8_t a[3], const uint8_t b[3])
{
    return abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2]);
}

/* the slot (1-based) of colour c: an earlier one within SLOT_NEAR, else a new one, else the nearest when all are taken */
static int slot_of(DashColour *out, const uint8_t c[3])
{
    int best = 0, best_d = 1 << 30;
    for (int s = 0; s < out->count; s++) {
        const int d = distance(out->rgb[s], c);
        if (d < best_d) best = s, best_d = d;
    }
    if (out->count > 0 && best_d <= SLOT_NEAR) return best + 1;
    if (out->count < DASH_COLOUR_SLOTS) {
        memcpy(out->rgb[out->count], c, 3);
        return ++out->count;
    }
    return best + 1;
}

int dash_color_parse(const uint8_t *png, size_t len, int width, int height, DashColour *out)
{
    if (out != NULL) out->slot = NULL, out->count = 0;
    if (png == NULL || out == NULL || width <= 0 || height <= 0 || width > MAX_SIDE || height > MAX_SIDE) return 0;
    PngImage img;
    if (png_read(png, len, &img) != PNG_OK) return 0;
    uint8_t *slot = calloc((size_t)width * (size_t)height, 1);
    if (slot == NULL) {
        png_free(img.pixels);
        return 0;
    }
    for (int y = 0; y < height; y++) {
        const size_t sy = (size_t)y * img.height / (size_t)height;
        for (int x = 0; x < width; x++) {
            const size_t sx = (size_t)x * img.width / (size_t)width, at = sy * img.width + sx;
            const uint8_t *c = img.indexed ? img.palette + 3 * img.pixels[at] : img.pixels + 4 * at;
            if (!img.indexed && img.pixels[4 * at + 3] < 128) continue;   /* unpainted */
            if (c[0] < DARK_BELOW && c[1] < DARK_BELOW && c[2] < DARK_BELOW) continue;   /* black: no colour */
            slot[(size_t)y * (size_t)width + (size_t)x] = (uint8_t)slot_of(out, c);
        }
    }
    png_free(img.pixels);
    out->slot = slot;
    return 1;
}

static int load_stem(const char *stem, DashColour *out)
{
    size_t size = 0;
    uint8_t *file = dash_asset_read(stem, "dash_color.png", &size);
    if (file == NULL) return 0;
    const int ok = dash_color_parse(file, size, DASH_MASK_W, DASH_MASK_H, out);
    platform_free_file(file);
    return ok;
}

int dash_color_load(const char *stem, DashColour *out)
{
    if (out != NULL) out->slot = NULL, out->count = 0;
    if (stem == NULL || stem[0] == '\0' || out == NULL) return 0;
    if (load_stem(stem, out)) return 1;
    char upper[64];
    size_t i = 0;
    for (; stem[i] != '\0' && i < sizeof upper - 1; i++) upper[i] = (char)toupper((unsigned char)stem[i]);
    upper[i] = '\0';
    return strcmp(upper, stem) != 0 && load_stem(upper, out);
}

void dash_color_free(DashColour *c)
{
    if (c == NULL) return;
    free(c->slot);
    c->slot = NULL, c->count = 0;
}
