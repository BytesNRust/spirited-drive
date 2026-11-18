/* port (dep59 step 5, T7e2): Best Times' bank for an added track (added_best_art.h). */
#include "game/frontend/added_best_art.h"
#include "formats/fsh.h"
#include "formats/shpi_build.h"

#include <stdlib.h>
#include <string.h>

enum { ENTRIES_MAX = 48, BLUR_R = 6, BLUR_PASSES = 2 };

/* the bank's palette (768 bytes, RGB); 0 when it has none */
static int bank_palette(const uint8_t *coast, size_t size, uint8_t pal[768])
{
    Fsh f;
    FshEntry e;
    uint32_t at;
    return fsh_open(&f, coast, size) == FSH_OK && fsh_find(&f, "!pal", &at) == FSH_OK && fsh_entry(&f, at, &e) == FSH_OK &&
           fsh_palette_rgb(&e, pal) == FSH_OK;
}

/* the title's box blurred (owner 2026-11-18: as the cars' pages' strip is): the box and a margin round it to RGB by `pal`, a box blur within
 * them, the blurred pixels mixed with the page's own by their distance from the box (all of it inside, none at the margin's edge, so the band
 * has no edge of its own), each mapped back to the palette's nearest entry */
static void blur_box(uint8_t *px, const uint8_t pal[768])
{
    enum { MARGIN_X = ADDED_BEST_MARGIN_X, MARGIN_Y = ADDED_BEST_MARGIN_Y };
    const int x0 = ADDED_BEST_BOX_X0 - MARGIN_X < 0 ? 0 : ADDED_BEST_BOX_X0 - MARGIN_X, x1 = ADDED_BEST_BOX_X1 + MARGIN_X;
    const int y0 = ADDED_BEST_BOX_Y0 - MARGIN_Y < 0 ? 0 : ADDED_BEST_BOX_Y0 - MARGIN_Y, y1 = ADDED_BEST_BOX_Y1 + MARGIN_Y;
    const int w = x1 - x0 + 1, h = y1 - y0 + 1;
    uint8_t *rgb = malloc((size_t)w * h * 3), *tmp = malloc((size_t)w * h * 3), *own = malloc((size_t)w * h * 3);
    if (rgb == NULL || tmp == NULL || own == NULL) {
        free(rgb), free(tmp), free(own);
        return;
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) memcpy(rgb + 3 * (y * w + x), pal + 3 * px[(y0 + y) * ADDED_BEST_W + x0 + x], 3);
    memcpy(own, rgb, (size_t)w * h * 3);
    for (int pass = 0; pass < BLUR_PASSES; pass++) {
        for (int y = 0; y < h; y++)   /* along the rows, then down the columns */
            for (int x = 0; x < w; x++)
                for (int c = 0; c < 3; c++) {
                    int sum = 0;
                    for (int k = -BLUR_R; k <= BLUR_R; k++) sum += rgb[3 * (y * w + (x + k < 0 ? 0 : x + k >= w ? w - 1 : x + k)) + c];
                    tmp[3 * (y * w + x) + c] = (uint8_t)(sum / (2 * BLUR_R + 1));
                }
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                for (int c = 0; c < 3; c++) {
                    int sum = 0;
                    for (int k = -BLUR_R; k <= BLUR_R; k++) sum += tmp[3 * ((y + k < 0 ? 0 : y + k >= h ? h - 1 : y + k) * w + x) + c];
                    rgb[3 * (y * w + x) + c] = (uint8_t)(sum / (2 * BLUR_R + 1));
                }
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const int px_x = x0 + x, px_y = y0 + y;
            const int dx = px_x < ADDED_BEST_BOX_X0 ? ADDED_BEST_BOX_X0 - px_x : px_x > ADDED_BEST_BOX_X1 ? px_x - ADDED_BEST_BOX_X1 : 0;
            const int dy = px_y < ADDED_BEST_BOX_Y0 ? ADDED_BEST_BOX_Y0 - px_y : px_y > ADDED_BEST_BOX_Y1 ? px_y - ADDED_BEST_BOX_Y1 : 0;
            const int fx = dx * 256 / (MARGIN_X + 1), fy = dy * 256 / (MARGIN_Y + 1), away = fx > fy ? fx : fy;   /* 0 inside .. 255 at the edge */
            const int keep = away < 256 ? away : 256;   /* the page's own, out of 256 */
            if (keep == 256) continue;   /* the margin's edge: not a pixel of it changes */
            int rgbm[3];
            for (int c = 0; c < 3; c++) rgbm[c] = (rgb[3 * (y * w + x) + c] * (256 - keep) + own[3 * (y * w + x) + c] * keep) / 256;
            int best = 0, best_d = 1 << 30;
            for (int i = 0; i < 256; i++) {
                int d = 0;
                for (int c = 0; c < 3; c++) d += (pal[3 * i + c] - rgbm[c]) * (pal[3 * i + c] - rgbm[c]);
                if (d < best_d) best_d = d, best = i;
            }
            px[px_y * ADDED_BEST_W + px_x] = (uint8_t)best;
        }
    free(rgb), free(tmp), free(own);
}

int added_best_art_make(const uint8_t *coast, size_t size, uint8_t **out, size_t *out_size, char *why, size_t why_size)
{
    ShpiDescEntry e[ENTRIES_MAX];
    ExchangeError err;
    size_t n = 0;
    *out = NULL, *out_size = 0;
    if (coast == NULL || !shpi_describe(coast, size, e, ENTRIES_MAX, &n, &err)) {
        if (why != NULL && why_size > 0) strncpy(why, "not a bank", why_size - 1), why[why_size - 1] = '\0';
        return 0;
    }
    int bgnd = -1;
    for (size_t i = 0; i < n; i++)
        if (memcmp(e[i].name, "bgnd", 4) == 0 && e[i].type == 0x7b && e[i].width == ADDED_BEST_W && e[i].height == ADDED_BEST_H) bgnd = (int)i;
    uint8_t *px = bgnd >= 0 ? malloc((size_t)ADDED_BEST_W * ADDED_BEST_H) : NULL;
    if (px == NULL) {
        if (why != NULL && why_size > 0) strncpy(why, bgnd < 0 ? "no 640 x 480 bgnd" : "no room", why_size - 1), why[why_size - 1] = '\0';
        return 0;
    }
    memcpy(px, e[bgnd].data, (size_t)ADDED_BEST_W * ADDED_BEST_H);
    for (int y = ADDED_BEST_BOX_Y0; y <= ADDED_BEST_BOX_Y1; y++)   /* the page's own texture, from the rows below the column heads, same columns */
        memcpy(px + y * ADDED_BEST_W + ADDED_BEST_BOX_X0, px + (y + ADDED_BEST_SOURCE_SHIFT) * ADDED_BEST_W + ADDED_BEST_BOX_X0,
               (size_t)(ADDED_BEST_BOX_X1 - ADDED_BEST_BOX_X0 + 1));
    uint8_t pal[768];
    if (bank_palette(coast, size, pal)) blur_box(px, pal);
    e[bgnd].data = px;
    const int ok = shpi_build(e, n, out, out_size, &err);
    if (!ok && why != NULL && why_size > 0) strncpy(why, err.message, why_size - 1), why[why_size - 1] = '\0';
    free(px);
    return ok;
}
