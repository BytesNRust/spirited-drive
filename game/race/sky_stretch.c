/* port: see sky_stretch.h. */
#include "game/race/sky_stretch.h"

#include <stdlib.h>
#include <string.h>

/* port guard: the stretch stays within what the horizon's row table (HORIZON_ROWS) can map */
enum { MAX_ABOVE = 0x1e0 };

static int32_t scaled(int32_t rows, int32_t scale) { return (int32_t)((uint32_t)rows * (uint32_t)scale) >> 16; }

/* the palette entry nearest (squared RGB distance) to the mean of the art's first rows, skipping the transparent 0xff */
static uint8_t mean_colour(const FshEntry *horz, int32_t w, const uint8_t *palette)
{
    int64_t sum[3] = { 0, 0, 0 }, n = 0;
    const int32_t rows = (int16_t)horz->misc[1] < SKY_FILL_MEAN_ROWS ? (int16_t)horz->misc[1] : SKY_FILL_MEAN_ROWS;
    for (int32_t r = 0; r < rows; r++)
        for (int32_t x = 0; x < w; x++) {
            const uint8_t p = horz->payload[(size_t)r * w + x];
            if (p == 0xff) continue;
            for (int k = 0; k < 3; k++) sum[k] += palette[3 * p + k];
            n++;
        }
    if (n == 0) return horz->payload[0];
    int best = horz->payload[0];
    int64_t best_d = INT64_MAX;
    for (int i = 0; i < 0xff; i++) {
        int64_t d = 0;
        for (int k = 0; k < 3; k++) {
            const int64_t e = (int64_t)palette[3 * i + k] * n - sum[k];
            d += e * e;
        }
        if (d < best_d) best_d = d, best = i;
    }
    return (uint8_t)best;
}

/* the colour of column x for the rows put above the art (SKY_FILL_COLUMN): the top row's pixel, unless every one of the
 * `rows` - 1 rows under it holds one other colour, a lone odd pixel in row 0 (AL1: a 18-column run of it) that the rows
 * below do not share; then that shared colour. Cloud columns, which vary from row to row, keep their top pixel. */
static uint8_t column_colour(const FshEntry *horz, int32_t w, int32_t rows, int32_t x)
{
    const uint8_t top = horz->payload[x];
    if (rows < 2) return top;
    const uint8_t below = horz->payload[w + x];
    for (int32_t r = 1; r < rows; r++)
        if (horz->payload[(size_t)r * w + x] != below) return top;
    return below != top && below != 0xff ? below : top;
}

static int lightness(const uint8_t *palette, uint8_t i) { return (77 * palette[3 * i] + 150 * palette[3 * i + 1] + 29 * palette[3 * i + 2]) >> 8; }

/* a star at the top of column x: its colour much lighter (SKY_FILL_STAR_LUMA) than the pixel under it, which is the sky (the art's rows under a star are not lit, as a cloud's top is) */
static int is_star(const FshEntry *horz, int32_t w, int32_t x, uint8_t colour, const uint8_t *palette)
{
    const uint8_t under = horz->payload[w + x];
    if (colour == 0xff || under == 0xff) return 0;
    const int l = lightness(palette, colour), u = lightness(palette, under);
    return l >= u + SKY_FILL_STAR_LUMA && 2 * l >= SKY_FILL_STAR_RATIO_X2 * u;   /* relative: a dim star on a dark sky counts, a grey bump on the sky's grey does not */
}

/* SKY_FILL_COLUMN_QUIET: the row of column colours (column_colour's) with its stars taken out: a run of at most SKY_FILL_STAR_WIDTH columns of stars (a longer run is a cloud's
 * highlight, and stays) takes the colour of the column beside the run, the left one, else the right one */
static void quiet_stars(const FshEntry *horz, int32_t w, const uint8_t *palette, uint8_t *row)
{
    int32_t x = 0;
    while (x < w) {
        if (!is_star(horz, w, x, row[x], palette)) {
            x++;
            continue;
        }
        int32_t end = x;
        while (end + 1 < w && is_star(horz, w, end + 1, row[end + 1], palette)) end++;
        if (end - x + 1 <= SKY_FILL_STAR_WIDTH) {
            const uint8_t beside = x > 0 ? row[x - 1] : end + 1 < w ? row[end + 1] : horz->payload[w + x];
            for (int32_t k = x; k <= end; k++) row[k] = beside;
        }
        x = end + 1;
    }
}

/* the 4x4 ordered-dither threshold, 0..15 */
static int bayer(int32_t x, int32_t y)
{
    static const uint8_t M[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    return M[y & 3][x & 3];
}

const FshEntry *sky_stretch(SkyStretch *s, const FshEntry *horz, int32_t scale, int32_t line, int32_t top, int mode,
                            const uint8_t *palette)
{
    if (mode != SKY_FILL_COLUMN && !((mode == SKY_FILL_BLEND || mode == SKY_FILL_COLUMN_QUIET) && palette != NULL)) return horz;
    if (horz == NULL || horz->payload == NULL || scale <= 0 || line == INT32_MIN) return horz;
    const int32_t w = (int16_t)horz->width, h = (int16_t)horz->height, above = (int16_t)horz->misc[1];
    if (w <= 0 || h <= 0 || above <= 0 || above > h || (size_t)w * (size_t)h > horz->payload_size) return horz;
    const int64_t needed = (int64_t)line - top;
    if (needed <= scaled(above, scale)) return horz;
    /* the fewest rows whose scaled height covers it: ceil(needed / scale) */
    int64_t rows = ((needed << 16) + scale - 1) / scale;
    if (rows > MAX_ABOVE) rows = MAX_ABOVE;
    if (rows <= above) return horz;
    const int32_t grown = (int32_t)rows, height = h - above + grown;
    const size_t size = (size_t)w * (size_t)height;
    if (s->cap < size) {
        uint8_t *bigger = realloc(s->pixels, size);
        if (bigger == NULL) return horz;
        s->pixels = bigger, s->cap = size;
    }
    const int32_t added = grown - above;   /* the rows put on top of the art's own */
    const uint8_t mean = mode == SKY_FILL_BLEND ? mean_colour(horz, w, palette) : 0;
    for (int32_t r = 0; r < added; r++) {
        uint8_t *const row = s->pixels + (size_t)r * w;
        if (mode == SKY_FILL_COLUMN || mode == SKY_FILL_COLUMN_QUIET) {
            if (r == 0) {   /* the first added row carries the column colours; the rest copy it */
                const int32_t sample = above < SKY_FILL_COLUMN_ROWS ? above : SKY_FILL_COLUMN_ROWS;
                for (int32_t x = 0; x < w; x++) row[x] = column_colour(horz, w, sample, x);
                if (mode == SKY_FILL_COLUMN_QUIET) quiet_stars(horz, w, palette, row);
            } else {
                memcpy(row, s->pixels, (size_t)w);
            }
            continue;
        }
        /* the mean colour, taking in the top row's pixel more and more over the last rows before the art */
        const int32_t d = added - r;   /* 1 on the row just above the art */
        for (int32_t x = 0; x < w; x++) {
            const uint8_t p = horz->payload[x];
            const int share = d >= SKY_FILL_BLEND_ROWS ? 0 : (SKY_FILL_BLEND_ROWS - d + 1) * 16 / (SKY_FILL_BLEND_ROWS + 1);
            row[x] = p != 0xff && bayer(x, r) < share ? p : mean;
        }
    }
    memcpy(s->pixels + (size_t)added * w, horz->payload, (size_t)above * w);
    memcpy(s->pixels + (size_t)grown * w, horz->payload + (size_t)above * w, (size_t)(h - above) * w);
    s->entry = *horz;
    s->entry.height = (uint16_t)height;
    s->entry.misc[1] = (int16_t)grown;
    s->entry.payload = s->pixels;
    s->entry.payload_size = size;
    return &s->entry;
}

void sky_stretch_free(SkyStretch *s)
{
    free(s->pixels);
    memset(s, 0, sizeof *s);
}
