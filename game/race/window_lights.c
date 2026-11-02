#include "game/race/window_lights.h"

#include <ctype.h>

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
