/* See added_card.h. */
#include "game/frontend/added_card.h"

#include "formats/shpi_build.h"
#include "formats/mod_ini.h"
#include "game/frontend/picture_palette.h"
#include "game/frontend/text_wrap.h"

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

static const uint8_t NAME_RED[3] = { 238, 0, 0 };

/* the name's coverage at (mx, my) of its mask, 0 outside it */
static int coverage(const AddedCardName *n, int mx, int my)
{
    return mx >= 0 && my >= 0 && mx < n->w && my < n->h ? n->mask[my * n->w + mx] : 0;
}

/* `p`'s colour moved `c` fifteenths of the way to `to` */
static void mix(uint8_t *p, const uint8_t to[3], int c)
{
    for (int k = 0; k < 3; k++) p[k] = (uint8_t)((p[k] * (15 - c) + to[k] * c + 7) / 15);
}

void added_card_draw_name(uint8_t *rgba, int w, int h, const AddedCardName *name, int base)
{
    static const uint8_t BLACK[3] = { 0, 0, 0 };
    if (name == NULL || name->mask == NULL) return;
    const int x0 = ADDED_CARD_NAME_X, y0 = base - name->base_row;
    for (int my = 0; my <= name->h; my++)   /* one more row and column: the shadow's */
        for (int mx = 0; mx <= name->w; mx++) {
            const int x = x0 + mx, y = y0 + my;
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            uint8_t *p = rgba + 4 * ((size_t)y * w + x);
            const int shadow = coverage(name, mx - 1, my - 1), ink = coverage(name, mx, my);
            if (shadow > 0) mix(p, BLACK, shadow > 15 ? 15 : shadow);
            if (ink > 0) mix(p, NAME_RED, ink > 15 ? 15 : ink);
        }
}

int added_card_name_fit(const char *name, int max_w, int (*measure)(void *ctx, const char *s), void *ctx, char *out, int size)
{
    if (size <= 0) return 0;
    char words[MOD_INI_NAME_MAX * 4 + 1];
    text_words(name, words, (int)sizeof words);
    const char *s = words;
    while (*s == ' ') s++;
    int n = (int)strlen(s);
    while (n > 0 && s[n - 1] == ' ') n--;
    if (n < size && n > 0) {
        snprintf(out, (size_t)size, "%.*s", n, s);
        if (measure(ctx, out) <= max_w) return 1;
    }
    for (int k = n - 1; k >= 0; k--) {   /* cut: the most characters that fit with "..." after them */
        int keep = k;
        while (keep > 0 && s[keep - 1] == ' ') keep--;
        if (keep == 0 || keep + 4 > size) continue;
        snprintf(out, (size_t)size, "%.*s...", keep, s);
        if (measure(ctx, out) <= max_w) return 1;
    }
    out[0] = '\0';
    return 0;
}

uint8_t *added_card_bank(const PngImage *img, int rival, int from_card, const uint8_t palette[768], const uint8_t usable[256], size_t *size,
                         char *why, size_t why_cap)
{
    return added_card_bank_named(img, rival, from_card, NULL, palette, usable, size, why, why_cap);
}

/* `rgba` (w x h) dithered into `palette` and made the shape bank whose "0000" sits at (x, y) */
static uint8_t *bank_of(const uint8_t *rgba, int w, int h, int x, int y, const uint8_t palette[768], const uint8_t usable[256], size_t *size,
                        char *why, size_t why_cap)
{
    uint8_t *pixels = malloc((size_t)w * h);
    uint8_t *bank = NULL;
    if (rgba != NULL && pixels != NULL) {
        picture_palette_dither(rgba, w, h, palette, usable, pixels);
        const ShpiDescEntry e[2] = {
            { .name = { '0', '0', '0', '0' }, .type = BITMAP, .width = (uint16_t)w, .height = (uint16_t)h,
              .misc = { 0, 0, (int16_t)x, (int16_t)y }, .data = pixels, .block_size = SHPI_BLOCK_AUTO, .dir_index = 0 },
            { .name = { '!', 'P', 'A', 'L' }, .type = PALETTE_8, .width = 256, .height = 1, .data = palette, .block_size = SHPI_BLOCK_AUTO,
              .dir_index = 1 },
        };
        ExchangeError err;
        if (!shpi_build(e, 2, &bank, size, &err)) snprintf(why, why_cap, "the card could not be made: %s", err.message), bank = NULL, *size = 0;
    } else {
        snprintf(why, why_cap, "out of memory");
    }
    free(pixels);
    return bank;
}

uint8_t *added_card_bank_named(const PngImage *img, int rival, int from_card, const AddedCardName *name, const uint8_t palette[768],
                               const uint8_t usable[256], size_t *size, char *why, size_t why_cap)
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
    if (rgba != NULL) added_card_draw_name(rgba, w, h, name, rival ? ADDED_RIVAL_NAME_BASE : ADDED_CARD_NAME_BASE);
    uint8_t *bank = bank_of(rgba, w, h, rival ? ADDED_RIVAL_X : ADDED_CARD_X, rival ? ADDED_RIVAL_Y : ADDED_CARD_Y, palette, usable, size, why, why_cap);
    free(rgba);
    return bank;
}

/* the flat card a track without a card.png has: dark grey inside a 2 px lighter frame */
static uint8_t *flat_track_rgba(void)
{
    enum { FRAME = 2 };
    static const uint8_t INSIDE[3] = { 40, 40, 40 }, EDGE[3] = { 120, 120, 120 };
    uint8_t *rgba = malloc((size_t)ADDED_TRACK_W * ADDED_TRACK_H * 4);
    if (rgba == NULL) return NULL;
    for (int y = 0; y < ADDED_TRACK_H; y++)
        for (int x = 0; x < ADDED_TRACK_W; x++) {
            const int edge = x < FRAME || y < FRAME || x >= ADDED_TRACK_W - FRAME || y >= ADDED_TRACK_H - FRAME;
            uint8_t *d = rgba + 4 * ((size_t)y * ADDED_TRACK_W + x);
            memcpy(d, edge ? EDGE : INSIDE, 3), d[3] = 255;
        }
    return rgba;
}

uint8_t *added_track_card_rgba(const PngImage *img, const AddedCardName *name, char *why, size_t why_cap)
{
    if (img != NULL && (img->pixels == NULL || (int)img->width != ADDED_TRACK_W || (int)img->height != ADDED_TRACK_H)) {
        snprintf(why, why_cap, "card.png is %ux%u, not %dx%d", img->width, img->height, ADDED_TRACK_W, ADDED_TRACK_H);
        return NULL;
    }
    uint8_t *rgba = img != NULL ? rgba_of(img, 0, 0, ADDED_TRACK_W, ADDED_TRACK_H) : flat_track_rgba();
    if (rgba == NULL) snprintf(why, why_cap, "no memory for the card");
    else added_card_draw_name(rgba, ADDED_TRACK_W, ADDED_TRACK_H, name, ADDED_TRACK_NAME_BASE);
    return rgba;
}

uint8_t *added_track_card_bank(const PngImage *img, const AddedCardName *name, const uint8_t palette[768], const uint8_t usable[256],
                               size_t *size, char *why, size_t why_cap)
{
    *size = 0;
    uint8_t *rgba = added_track_card_rgba(img, name, why, why_cap);
    if (rgba == NULL) return NULL;
    uint8_t *bank = bank_of(rgba, ADDED_TRACK_W, ADDED_TRACK_H, ADDED_TRACK_X, ADDED_TRACK_Y, palette, usable, size, why, why_cap);
    free(rgba);
    return bank;
}
