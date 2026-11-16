/* See added_card.h. */
#include "game/frontend/added_card.h"

#include "formats/shpi_build.h"
#include "game/frontend/picture_palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BITMAP = 0x7b, PALETTE_8 = 0x24 };

/* the `w` x `h` RGBA pixels of `img` from (x0, y0), an indexed one through its palette (alpha 255); NULL when memory ran out */
static uint8_t *rgba_of(const PngImage *img, int x0, int y0, int w, int h)
{
    uint8_t *out = malloc((size_t)w * h * 4);
    if (out == NULL) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const size_t at = (size_t)(y0 + y) * img->width + (size_t)(x0 + x);
            uint8_t *d = out + 4 * ((size_t)y * w + x);
            if (img->indexed) memcpy(d, img->palette + 3 * img->pixels[at], 3), d[3] = 255;
            else memcpy(d, img->pixels + 4 * at, 4);
        }
    return out;
}

uint8_t *added_card_bank(const PngImage *img, int rival, int from_card, const uint8_t palette[768], const uint8_t usable[256], size_t *size,
                         char *why, size_t why_cap)
{
    *size = 0;
    const int w = rival ? ADDED_RIVAL_W : ADDED_CARD_W, h = rival ? ADDED_RIVAL_H : ADDED_CARD_H;
    const int want_w = from_card ? ADDED_CARD_W : w, want_h = from_card ? ADDED_CARD_H : h;
    if (img == NULL || img->pixels == NULL || (int)img->width != want_w || (int)img->height != want_h) {
        snprintf(why, why_cap, "%s is %ux%u, not %dx%d", rival && !from_card ? "rival.png" : "card.png", img != NULL ? img->width : 0,
                 img != NULL ? img->height : 0, want_w, want_h);
        return NULL;
    }
    uint8_t *rgba = rgba_of(img, (want_w - w) / 2, (want_h - h) / 2, w, h);   /* the middle when it is card.png for the rival */
    uint8_t *pixels = malloc((size_t)w * h);
    uint8_t *bank = NULL;
    if (rgba != NULL && pixels != NULL) {
        picture_palette_nearest(rgba, w * h, palette, usable, pixels);
        const ShpiDescEntry e[2] = {
            { .name = { '0', '0', '0', '0' }, .type = BITMAP, .width = (uint16_t)w, .height = (uint16_t)h,
              .misc = { 0, 0, (int16_t)(rival ? ADDED_RIVAL_X : ADDED_CARD_X), (int16_t)(rival ? ADDED_RIVAL_Y : ADDED_CARD_Y) },
              .data = pixels, .block_size = SHPI_BLOCK_AUTO, .dir_index = 0 },
            { .name = { '!', 'P', 'A', 'L' }, .type = PALETTE_8, .width = 256, .height = 1, .data = palette, .block_size = SHPI_BLOCK_AUTO,
              .dir_index = 1 },
        };
        ExchangeError err;
        if (!shpi_build(e, 2, &bank, size, &err)) snprintf(why, why_cap, "the card could not be made: %s", err.message), bank = NULL, *size = 0;
    } else {
        snprintf(why, why_cap, "out of memory");
    }
    free(rgba), free(pixels);
    return bank;
}
