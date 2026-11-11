/* The credits' relief without its text (credits_relief.h), on made-up pages: a known relief, each page's strokes at other places, a block three pages share,
 * the logo every page shares, and a configuration relief with a bright "&" the credits do not have. */
#include "game/frontend/credits_relief.h"
#include "test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { W = CREDITS_RELIEF_W, H = CREDITS_RELIEF_H, WHITE = 250, RED = 252 };

static uint8_t relief_at(int x, int y) { return (uint8_t)lrint(100 + 40 * sin(x / 23.0) + 30 * cos(y / 17.0)); }
static int in_logo(int x, int y) { return x >= 20 && x < 200 && y >= 20 && y < 60; }
static int in_shared_block(int x, int y) { return x >= 100 && x < 180 && y >= 400 && y < 412; }

static int lum(const uint8_t *c) { return (30 * c[0] + 59 * c[1] + 11 * c[2]) / 100; }

static void make(uint8_t *pages[5], uint8_t pal[768], uint8_t *menu_rgb)
{
    for (int i = 0; i < 240; i++) pal[3 * i] = pal[3 * i + 1] = pal[3 * i + 2] = (uint8_t)i;
    for (int i = 240; i < 256; i++) pal[3 * i] = pal[3 * i + 1] = pal[3 * i + 2] = 0;
    pal[3 * WHITE] = pal[3 * WHITE + 1] = pal[3 * WHITE + 2] = 255;
    pal[3 * RED] = 220, pal[3 * RED + 1] = 20, pal[3 * RED + 2] = 20;
    for (int p = 0; p < 5; p++)
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                uint8_t v = relief_at(x, y);
                const int line = (y - 80 - 13 * p) % 65, stroke = (x / 3) % 4 != 0;
                if (y >= 80 && line >= 0 && line < 8 && x >= 300 && x < 560 && stroke) v = WHITE;   /* a page's names */
                if (y >= 80 && line == 8 && x >= 301 && x < 561) v = 0;                            /* their shadow */
                if (p < 3 && in_shared_block(x, y)) v = WHITE;                                     /* three pages agree on text here */
                if (in_logo(x, y)) v = RED;
                pages[p][y * W + x] = v;
            }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int v = relief_at(x, y);
            const double dx = x - 540, dy = y - 400;
            if (dx * dx + dy * dy < 50 * 50) v = 240;   /* the configuration screens' "&" */
            if (y < 50) v = 200;                          /* and their banner */
            memset(&menu_rgb[3 * (y * W + x)], v, 3);
        }
}

static void the_text_goes_and_the_relief_stays(void)
{
    uint8_t *pages[5], pal[768], palette[768];
    uint8_t *menu = malloc((size_t)W * H * 3), *out = malloc((size_t)W * H);
    for (int p = 0; p < 5; p++) pages[p] = malloc((size_t)W * H);
    make(pages, pal, menu);
    CHECK(credits_relief_compose((const uint8_t *const *)pages, pal, menu, out, palette) == 1);
    long off = 0, white = 0, shared_off = 0, logo_red = 0, logo = 0, n = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const uint8_t *c = &palette[3 * out[y * W + x]];
            if (in_logo(x, y)) {
                logo++;
                logo_red += c[0] == 220 && c[1] == 20;
                continue;
            }
            n++;
            const int d = abs(lum(c) - relief_at(x, y));
            off += d > 6;
            white += lum(c) > 245;
            if (in_shared_block(x, y)) shared_off += d > 6;
        }
    CHECK(off * 1000 <= n * 5);   /* at most 0.5 % of the pixels off the relief */
    CHECK_EQ_INT((int)white, 0);   /* no text left */
    CHECK_EQ_INT((int)shared_off, 0);   /* where three of the five pages carry text */
    CHECK_EQ_INT((int)logo_red, (int)logo);   /* the logo stays */
    int yellow = 0;
    for (int k = 0; k < 256; k++) yellow |= palette[3 * k] == 255 && palette[3 * k + 1] == 206 && palette[3 * k + 2] == 6;
    CHECK(yellow);   /* the text's own colours have entries */
    for (int p = 0; p < 5; p++) free(pages[p]);
    free(menu), free(out);
}

int main(void)
{
    the_text_goes_and_the_relief_stays();
    return TEST_REPORT();
}
