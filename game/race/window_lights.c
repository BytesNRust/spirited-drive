#include "game/race/window_lights.h"

#include <ctype.h>

enum { LIT_PERCENT = 40 };

/* CY1's brick facade (scenery texture 40, 77 placements, 63 x 63): eight columns by four rows of windows over a shop row */
static const WindowBoard CY1_BOARDS[] = {
    { 40, 63, 63, 8, 4, { { 6, 10 }, { 12, 16 }, { 18, 22 }, { 24, 28 }, { 30, 34 }, { 36, 41 }, { 44, 50 }, { 52, 58 } }, { { 9, 15 }, { 19, 25 }, { 29, 35 }, { 40, 46 } } },
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
