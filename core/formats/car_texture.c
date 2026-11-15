/* See car_texture.h. */
#include "formats/car_texture.h"

#include "formats/png.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { OWN_COLOURS = 0xfa, TRANSPARENT = 0xff, ALPHA_OPAQUE = 128 };

/* the bank's palette in 8-bit colour: a 6-bit one (0x22) shifted, an 8-bit one (0x24) as stored */
static void expand(const uint8_t pal[SHPI_PALETTE_BYTES], uint8_t type, uint8_t out[SHPI_PALETTE_BYTES])
{
    for (int i = 0; i < SHPI_PALETTE_BYTES; i++) out[i] = type == 0x22 ? (uint8_t)(pal[i] << 2) : pal[i];
}

/* the nearest of the bank's own colours 0x00..0xF9; *exact when it is the colour itself */
static uint8_t nearest(const uint8_t rgb[SHPI_PALETTE_BYTES], int r, int g, int b, int *exact)
{
    int best = 0, best_d = 1 << 30;
    for (int i = 0; i < OWN_COLOURS && best_d > 0; i++) {
        const int dr = rgb[3 * i] - r, dg = rgb[3 * i + 1] - g, db = rgb[3 * i + 2] - b, d = dr * dr + dg * dg + db * db;
        if (d < best_d) best_d = d, best = i;
    }
    *exact = best_d == 0;
    return (uint8_t)best;
}

/* each pixel of `img` as an index of `rgb`; the counts in `s` */
static void fit(const PngImage *img, const uint8_t rgb[SHPI_PALETTE_BYTES], int same_palette, uint8_t *px, CarTextureStats *s)
{
    for (uint32_t k = 0; k < s->pixels; k++) {
        int exact = 1;
        if (img->indexed && same_palette) {
            px[k] = img->pixels[k];
            s->kept_special += px[k] >= OWN_COLOURS;
            continue;
        }
        const uint8_t *c = img->indexed ? img->palette + 3 * img->pixels[k] : img->pixels + 4 * (size_t)k;
        if (!img->indexed && c[3] < ALPHA_OPAQUE) {
            px[k] = TRANSPARENT;
            s->transparent++;
            continue;
        }
        px[k] = nearest(rgb, c[0], c[1], c[2], &exact);
        s->moved += !exact;
    }
    uint8_t seen[256] = { 0 };
    for (uint32_t k = 0; k < s->pixels; k++) s->colours += !seen[px[k]]++;
}

int car_texture_from_png(const uint8_t *png, size_t len, const uint8_t pal[SHPI_PALETTE_BYTES], uint8_t pal_type,
                         uint8_t **pixels, uint16_t *width, uint16_t *height, CarTextureStats *stats, ExchangeError *err)
{
    *pixels = NULL, *width = *height = 0;
    memset(stats, 0, sizeof *stats);
    if (pal_type != 0x22 && pal_type != 0x24) { exchange_fail(err, "a palette of type 0x%02x (0x22 or 0x24)", pal_type); return 0; }
    uint32_t w = 0, h = 0;
    if (png_size(png, len, &w, &h) != PNG_OK) { exchange_fail(err, "not a PNG"); return 0; }
    if (w == 0 || h == 0 || w > SHPI_MAX_SIDE || h > SHPI_MAX_SIDE) {
        exchange_fail(err, "a PNG of %u x %u: a side is 1 to %d", w, h, SHPI_MAX_SIDE);
        return 0;
    }
    PngImage img;
    const PngResult r = png_read(png, len, &img);
    if (r != PNG_OK) { exchange_fail(err, "the PNG cannot be read: %s", png_result_str(r)); return 0; }
    uint8_t rgb[SHPI_PALETTE_BYTES];
    expand(pal, pal_type, rgb);
    uint8_t *px = malloc((size_t)img.width * img.height);
    if (px == NULL) { png_free(img.pixels); exchange_fail(err, "out of memory"); return 0; }
    stats->pixels = img.width * img.height;
    fit(&img, rgb, img.indexed && memcmp(img.palette, rgb, sizeof rgb) == 0, px, stats);
    *pixels = px, *width = (uint16_t)img.width, *height = (uint16_t)img.height;
    png_free(img.pixels);
    return 1;
}

static int taken_by(const char (*taken)[4], size_t n, const char name[4])
{
    for (size_t i = 0; i < n; i++)
        if (memcmp(taken[i], name, 4) == 0) return 1;
    return 0;
}

void car_texture_entry_name(const char *name, const char (*taken)[4], size_t n_taken, char out[4])
{
    char base[4];
    size_t len = strlen(name);
    for (int i = 0; i < 4; i++) {
        const unsigned char c = i < (int)len ? (unsigned char)name[i] : '_';
        base[i] = isalnum(c) || c == '_' || c == '-' ? (char)c : '_';
    }
    memcpy(out, base, 4);
    for (unsigned k = 1; taken_by(taken, n_taken, out) && k < 1000; k++) {
        char digits[8];
        const int n = snprintf(digits, sizeof digits, "%u", k);
        memcpy(out, base, 4);
        memcpy(out + 4 - n, digits, (size_t)n);
    }
}
