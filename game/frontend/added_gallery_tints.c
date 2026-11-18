/* See added_gallery_page.h: the colours the SETUP panel previews (dep59 showcase, PR C): the car as it comes (the mean colour of its card),
 * each skin (the mean colour of its first texture that is not a mask) and the page's background tint (the first paint of the mod). */
#include "game/frontend/added_gallery_page.h"

#include "formats/png.h"
#include "game/platform/platform.h"
#include "game/sys/car_pieces.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MEAN_MAX_PIXELS = 4 * 1024 * 1024 };   /* a texture bigger than this is not read for its mean */

/* the mean colour of the opaque pixels of the PNG at `path` into `rgb`; 1 when there was a picture with some */
static int png_mean(const char *path, uint8_t rgb[3])
{
    size_t size = 0;
    uint8_t *data = platform_read_file(path, &size);
    uint32_t w = 0, h = 0;
    PngImage img;
    int ok = data != NULL && png_size(data, size, &w, &h) == PNG_OK && (uint64_t)w * h <= MEAN_MAX_PIXELS && png_read(data, size, &img) == PNG_OK;
    if (data != NULL) platform_free_file(data);
    if (!ok) return 0;
    uint64_t sum[3] = { 0, 0, 0 }, n = 0;
    for (size_t i = 0; i < (size_t)img.width * img.height; i++) {
        const uint8_t *c = img.indexed ? img.palette + 3 * img.pixels[i] : img.pixels + 4 * i;
        if (!img.indexed && img.pixels[4 * i + 3] < 128) continue;
        for (int k = 0; k < 3; k++) sum[k] += c[k];
        n++;
    }
    png_free(img.pixels);
    for (int k = 0; k < 3; k++) rgb[k] = n > 0 ? (uint8_t)(sum[k] / n) : 0;
    return n > 0;
}

/* a skin's mean colour: the first of the PNGs of skins/<name>/ that is not a mask (*_mask.png), `fallback` when there is none */
static void skin_mean(const CarPieceCard *c, int skin, const uint8_t fallback[3], uint8_t rgb[3])
{
    char folder[CAR_PIECE_DIR_MAX + CAR_PIECE_SKIN_NAME_MAX + 16];
    int count = 0, found = 0;
    snprintf(folder, sizeof folder, "%s/skins/%s", c->dir, c->skin[skin]);
    char **names = platform_list_files(folder, "*.png", &count);
    for (int i = 0; names != NULL && i < count && !found; i++) {
        const size_t n = strlen(names[i]);
        char path[sizeof folder + 80];
        if (n >= 9 && strcmp(names[i] + n - 9, "_mask.png") == 0) continue;
        snprintf(path, sizeof path, "%s/%s", folder, names[i]);
        found = png_mean(path, rgb);
    }
    if (names != NULL) platform_free_names(names);
    if (!found) memcpy(rgb, fallback, 3);
}

/* the paint's hue as a dark, muted tone (owner 2026-11-18: never a strong colour, a strong paint gives a dark tone): the brightest channel 72 (the
 * sepia's 70), the saturation at most 55% */
static void muted(const uint8_t rgb[3], uint8_t out[3])
{
    enum { DARK = 72, SATURATION_PERCENT = 55 };
    int mx = rgb[0];
    if (rgb[1] > mx) mx = rgb[1];
    if (rgb[2] > mx) mx = rgb[2];
    if (mx == 0) {
        out[0] = out[1] = out[2] = DARK / 2;
        return;
    }
    int mn = rgb[0];
    if (rgb[1] < mn) mn = rgb[1];
    if (rgb[2] < mn) mn = rgb[2];
    const int saturation = (mx - mn) * 100 / mx;
    for (int i = 0; i < 3; i++) {
        const int c = rgb[i] * DARK / mx;   /* the colour at the dark brightness */
        out[i] = (uint8_t)(saturation > SATURATION_PERCENT ? DARK - (DARK - c) * SATURATION_PERCENT / saturation : c);
    }
}

void added_gallery_tints_make(Page *p)
{
    char path[CAR_PIECE_DIR_MAX + 16];
    snprintf(path, sizeof path, "%s/card.png", p->card.dir);
    static const uint8_t GREY[3] = { 96, 84, 72 };
    if (p->warrior || !png_mean(path, p->default_rgb)) memcpy(p->default_rgb, GREY, 3);
    memcpy(p->skin_rgb[0], p->default_rgb, 3);
    for (int k = 1; k < p->card.skins && k <= CAR_PIECE_SKINS_MAX; k++) skin_mean(&p->card, k, p->default_rgb, p->skin_rgb[k]);
    p->has_tint = !p->warrior && p->card.info.paint_count > 0;   /* the background takes the mod's first paint: fixed, whatever SETUP picks */
    if (p->has_tint) muted(p->card.info.paints[0].rgb, p->tint);
}
