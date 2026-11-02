#include "game/race/window_lights.h"

#include <ctype.h>
#include <string.h>

enum { LIT_PERCENT = 40 };

/* CY1's brick facade (scenery texture 40, 77 placements, 63 x 63): eight columns by four rows of windows over a shop row */
static const WindowBoard CY1_BOARDS[] = {
    { 40, 63, 63, 8, 4, { { 6, 10 }, { 12, 16 }, { 18, 22 }, { 24, 28 }, { 30, 34 }, { 36, 41 }, { 44, 50 }, { 52, 58 } }, { { 9, 15 }, { 19, 25 }, { 29, 35 }, { 40, 46 } } },
};

/* CY2's storeys (scenery textures 25, 27 and 29, 64 x 32, 62 placements: a grey, a tan and an orange wall): six windows by two rows, the glass inside the dark
 * frames; and its red brick block (texture 17, 63 x 63, 16 placements): eight columns by four rows over the shop row */
#define CY2_STOREY(n, h) { n, 64, h, 6, 2, { { 3, 8 }, { 14, 19 }, { 24, 29 }, { 35, 40 }, { 46, 51 }, { 56, 61 } }, { { 3, 11 }, { 17, 26 } } }
static const WindowBoard CY2_BOARDS[] = {
    CY2_STOREY(25, 32),
    CY2_STOREY(27, 32),
    CY2_STOREY(29, 31),
    { 17, 63, 63, 8, 4, { { 2, 5 }, { 10, 14 }, { 17, 20 }, { 23, 26 }, { 29, 33 }, { 37, 41 }, { 46, 51 }, { 56, 59 } }, { { 8, 16 }, { 19, 27 }, { 29, 37 }, { 39, 47 } } },
};

/* TR2's (Autumn Valley) blue glass tower (scenery texture 37, 128 x 128, two copies of a 64-wide facade side by side): two rows of four panes in each half */
static const WindowBoard TR2_BOARDS[] = {
    { 37, 128, 128, 8, 2, { { 9, 18 }, { 22, 31 }, { 35, 44 }, { 47, 56 }, { 73, 82 }, { 86, 95 }, { 99, 108 }, { 111, 120 } }, { { 8, 16 }, { 23, 30 } } },
};

/* CY3's white tower (scenery texture 21, 32 x 128, 49 placements): nineteen ribbons of glass, each split in three to light a part of it */
static const WindowBoard CY3_BOARDS[] = {
    { 21, 32, 128, 3, 19, { { 4, 12 }, { 13, 21 }, { 22, 30 } },
      { { 19, 21 }, { 25, 27 }, { 31, 33 }, { 37, 38 }, { 42, 44 }, { 48, 50 }, { 54, 56 }, { 59, 61 }, { 65, 67 }, { 71, 73 }, { 76, 78 }, { 83, 84 }, { 88, 90 },
        { 94, 96 }, { 100, 102 }, { 106, 107 }, { 111, 113 }, { 117, 119 }, { 123, 125 } } },
};

static int is_track(const char *track, const char *name)
{
    for (const char *t = track; *name != '\0'; t++, name++)
        if (toupper((unsigned char)*t) != *name) return 0;
    return 1;
}

int window_boards_for(const char *track, const WindowBoard **out)
{
    if (track == NULL || out == NULL) return 0;
    if (is_track(track, "CY1")) {
        *out = CY1_BOARDS;
        return (int)(sizeof CY1_BOARDS / sizeof CY1_BOARDS[0]);
    }
    if (is_track(track, "CY2")) {
        *out = CY2_BOARDS;
        return (int)(sizeof CY2_BOARDS / sizeof CY2_BOARDS[0]);
    }
    if (is_track(track, "TR2")) {
        *out = TR2_BOARDS;
        return (int)(sizeof TR2_BOARDS / sizeof TR2_BOARDS[0]);
    }
    if (is_track(track, "CY3")) {
        *out = CY3_BOARDS;
        return (int)(sizeof CY3_BOARDS / sizeof CY3_BOARDS[0]);
    }
    return 0;
}

static uint32_t hash3(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca6bu ^ (c + 0x165667b1u) * 0xc2b2ae35u;
    h ^= h >> 15, h *= 0x2c1b3c6du, h ^= h >> 12, h *= 0x297a2d39u, h ^= h >> 15;
    return h;
}

size_t window_board_paint(const WindowBoard *board, uint8_t *pixels, int width, int height, uint8_t entry, uint32_t seed)
{
    if (board == NULL || pixels == NULL || width != board->width || height != board->height) return 0;
    size_t lit = 0;
    for (int r = 0; r < board->nrow; r++)
        for (int c = 0; c < board->ncol; c++) {
            if (hash3(seed, (uint32_t)board->scenery, (uint32_t)(r * WINDOW_COLS_MAX + c)) % 100 >= LIT_PERCENT) continue;
            for (int y = board->row[r][0]; y <= board->row[r][1] && y < height; y++)
                for (int x = board->col[c][0]; x <= board->col[c][1] && x < width; x++) pixels[y * width + x] = entry;
            lit++;
        }
    return lit;
}

/* the by-darkness boards (window_lights.h), per track: a facade's windows are its dark openings; the numbers are the board, its size, the luma under which a
 * texel is glass, and the biggest patch taken for a window (a roof's shadow or a door is bigger) */
#define DARKS(n, w, h, luma, mw, mh) { n, w, h, { 0, 0, (w) - 1, (h) - 1 }, luma, 2, 3, mw, mh, 255 }

/* CL1's pink balcony slabs and round tower (25, 27, 29) */
static const WindowDark CL1_DARKS[] = { DARKS(25, 64, 64, 70, 12, 20), DARKS(27, 63, 63, 70, 12, 20), DARKS(29, 64, 63, 70, 12, 20) };
/* AL1: the farm houses */
static const WindowDark AL1_DARKS[] = { DARKS(15, 125, 62, 60, 14, 20), DARKS(16, 126, 62, 60, 14, 20) };
/* TR4 (Lost Vegas): the pawn shop and the bail bonds */
static const WindowDark TR4_DARKS[] = { DARKS(38, 100, 100, 60, 16, 24), DARKS(39, 100, 100, 60, 16, 24), DARKS(40, 100, 68, 60, 16, 24) };
/* CY3's tall towers */
static const WindowDark CY3_DARKS[] = { DARKS(24, 64, 126, 70, 10, 14), DARKS(26, 63, 126, 70, 10, 14) };

#define DARK_SET(name, set) if (is_track(track, name)) { *out = set; return (int)(sizeof set / sizeof set[0]); }

int window_darks_for(const char *track, const WindowDark **out)
{
    if (track == NULL || out == NULL) return 0;
    DARK_SET("CL1", CL1_DARKS)
    DARK_SET("AL1", AL1_DARKS)
    DARK_SET("TR4", TR4_DARKS)
    DARK_SET("CY3", CY3_DARKS)
    return 0;
}

enum { DARK_SIDE_MAX = 256 };

static int luma_of(const uint8_t *palette, uint8_t index)
{
    const uint8_t *c = palette + 3 * index;
    return (299 * c[0] + 587 * c[1] + 114 * c[2]) / 1000;
}

size_t window_dark_paint(const WindowDark *board, uint8_t *pixels, int width, int height, const uint8_t *palette, uint8_t entry, uint32_t seed)
{
    if (board == NULL || pixels == NULL || palette == NULL || width != board->width || height != board->height) return 0;
    if (width > DARK_SIDE_MAX || height > DARK_SIDE_MAX) return 0;
    static uint8_t seen[DARK_SIDE_MAX * DARK_SIDE_MAX];
    static uint32_t queue[DARK_SIDE_MAX * DARK_SIDE_MAX];
    memset(seen, 0, (size_t)width * (size_t)height);
    uint8_t dark[256];
    for (int i = 0; i < 256; i++) dark[i] = i != board->transparent && i != entry && luma_of(palette, (uint8_t)i) < board->dark_luma;
    const int x0 = board->region[0] > 0 ? board->region[0] : 0, y0 = board->region[1] > 0 ? board->region[1] : 0;
    const int x1 = board->region[2] < width - 1 ? board->region[2] : width - 1, y1 = board->region[3] < height - 1 ? board->region[3] : height - 1;
    size_t lit = 0;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            const size_t at = (size_t)y * (size_t)width + (size_t)x;
            if (seen[at] || !dark[pixels[at]]) continue;
            size_t head = 0, tail = 0;
            int bx0 = x, bx1 = x, by0 = y, by1 = y;
            seen[at] = 1, queue[tail++] = (uint32_t)at;
            while (head < tail) {
                const uint32_t p = queue[head++];
                const int px = (int)(p % (uint32_t)width), py = (int)(p / (uint32_t)width);
                if (px < bx0) bx0 = px;
                if (px > bx1) bx1 = px;
                if (py < by0) by0 = py;
                if (py > by1) by1 = py;
                const int nx[4] = { px - 1, px + 1, px, px }, ny[4] = { py, py, py - 1, py + 1 };
                for (int k = 0; k < 4; k++) {
                    if (nx[k] < x0 || nx[k] > x1 || ny[k] < y0 || ny[k] > y1) continue;
                    const size_t q = (size_t)ny[k] * (size_t)width + (size_t)nx[k];
                    if (seen[q] || !dark[pixels[q]]) continue;
                    seen[q] = 1, queue[tail++] = (uint32_t)q;
                }
            }
            const int bw = bx1 - bx0 + 1, bh = by1 - by0 + 1;
            if (bw < board->min_w || bh < board->min_h || bw > board->max_w || bh > board->max_h) continue;
            if (hash3(seed, (uint32_t)board->scenery, (uint32_t)(by0 * DARK_SIDE_MAX + bx0)) % 100 >= LIT_PERCENT) continue;
            for (size_t i = 0; i < tail; i++) pixels[queue[i]] = entry;
            lit++;
        }
    return lit;
}
