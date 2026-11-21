/* psd_mask.h: the two scripts' class rules, texel by texel. */
#include "psd_mask.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psd_read.h"

/* the classes' values: car_mask_codec.h's CAR_MASK_* and game/race/dash_mask.h's DASH_MASK_* (tests/test_psd_mask.c checks they are those) */
enum { PAINT = 0, GLASS = 128, BODY = 192, BLACK = 224, BRAKE = 240, POSITION = 244, HEAD = 248, REVERSE = 252, KEEP = 255 };
enum { D_AUTO = 0, D_DASH = 64, D_DIAL = 128, D_NOSE = 160, D_BLADE = 192, D_LIT = 255 };

static const char *const MASK_NAMES[] = { "mask" };
static const char *const MASK_WANT[] = { "mask" };
static const char *const DASH_WANT[] = { "mask", "limpia", "wipers", "color", "colour" };
static const char *const BLADE_NAMES[] = { "limpia", "wipers" };
static const char *const COLOR_NAMES[] = { "color", "colour" };

static int max3(int r, int g, int b) { return r > g ? (r > b ? r : b) : (g > b ? g : b); }
static int min3(int r, int g, int b) { return r < g ? (r < b ? r : b) : (g < b ? g : b); }

static int lamp_class(int r, int g, int b)
{
    if (r <= 64 && g >= 192 && b >= 192) return REVERSE;   /* cyan first: b == g would not see it as a lamp */
    if (r < 192 || b > 64) return 0;
    if (g <= 64) return BRAKE;
    if (g >= 96 && g <= 176) return POSITION;
    if (g >= 192) return HEAD;
    return 0;
}

static int is_magenta(int r, int g, int b)
{
    const int pink = r < b ? r : b;
    return pink >= 128 && g * 2 <= pink && abs(r - b) <= 32;
}

static int car_class(const uint8_t *px)
{
    const int r = px[0], g = px[1], b = px[2];
    if (px[3] < 128) return PAINT;
    const int lamp = lamp_class(r, g, b);
    if (lamp != 0) return lamp;
    if (is_magenta(r, g, b)) return BLACK;
    if (b > r && b > g) return GLASS;
    if (g > r && g > b) return BODY;   /* green is the one strongest channel */
    return max3(r, g, b) >= 64 ? KEEP : PAINT;
}

static int dash_class(const uint8_t *px)
{
    const int r = px[0], g = px[1], b = px[2];
    if (px[3] < 128 || max3(r, g, b) < 64) return D_AUTO;
    if (min3(r, g, b) >= 192 || (r >= 192 && g >= 192 && b <= 64)) return D_LIT;
    if (r >= 192 && g <= 64 && b <= 64) return D_BLADE;
    if (g >= 192 && b >= 192 && r <= 64) return D_NOSE;
    if (b > r && b > g) return D_DASH;
    if (g > r && g > b) return D_DIAL;
    return D_AUTO;
}

/* each pixel of `layer` on the canvas, with its canvas place; fn gets (canvas index, rgba) */
typedef void (*Texel)(uint8_t *dst, size_t at, const uint8_t *px);

static void paint(const PsdFile *psd, const PsdLayer *layer, uint8_t *dst, Texel fn)
{
    if (layer->rgba == NULL) return;
    for (int y = layer->top; y < layer->bottom; ++y)
        for (int x = layer->left; x < layer->right; ++x) {
            if (x < 0 || y < 0 || (uint32_t)x >= psd->width || (uint32_t)y >= psd->height) continue;
            fn(dst, (size_t)y * psd->width + (size_t)x, layer->rgba + ((size_t)(y - layer->top) * (size_t)(layer->right - layer->left) + (size_t)(x - layer->left)) * 4);
        }
}

static void car_texel(uint8_t *dst, size_t at, const uint8_t *px) { dst[at] = (uint8_t)car_class(px); }
static void dash_texel(uint8_t *dst, size_t at, const uint8_t *px) { dst[at] = (uint8_t)dash_class(px); }
static void blade_texel(uint8_t *dst, size_t at, const uint8_t *px)
{
    if (px[3] >= 128 && max3(px[0], px[1], px[2]) >= 64) dst[at] = D_BLADE;
}
static void color_texel(uint8_t *dst, size_t at, const uint8_t *px)
{
    if (px[3] >= 128) memcpy(dst + at * 4, px, 3), dst[at * 4 + 3] = 255;
}

static int no_mask(char *why, size_t cap)
{
    if (why != NULL && cap > 0) snprintf(why, cap, "no layer named \"mask\"");
    return 0;
}

int psd_car_mask_bytes(const uint8_t *bytes, size_t len, uint8_t **grey, uint32_t *w, uint32_t *h, char *why, size_t why_cap)
{
    PsdFile psd;
    if (!psd_parse(bytes, len, MASK_WANT, 1, &psd, why, why_cap)) return 0;
    const PsdLayer *layer = psd_find_layer(&psd, MASK_NAMES, 1);
    uint8_t *out = layer != NULL ? calloc((size_t)psd.width * psd.height, 1) : NULL;
    if (layer == NULL || out == NULL) {
        const int missing = layer == NULL;
        *w = psd.width, *h = psd.height;
        psd_free(&psd);
        if (missing) return no_mask(why, why_cap);
        if (why != NULL && why_cap > 0) snprintf(why, why_cap, "out of memory");
        return 0;
    }
    paint(&psd, layer, out, car_texel);
    *grey = out, *w = psd.width, *h = psd.height;
    psd_free(&psd);
    return 1;
}

int psd_dash_mask_bytes(const uint8_t *bytes, size_t len, uint8_t **classes, uint8_t **color_rgba, uint32_t *w, uint32_t *h, char *why, size_t why_cap)
{
    PsdFile psd;
    if (!psd_parse(bytes, len, DASH_WANT, 5, &psd, why, why_cap)) return 0;
    const PsdLayer *mask = psd_find_layer(&psd, MASK_NAMES, 1);
    if (mask == NULL) return psd_free(&psd), no_mask(why, why_cap);
    const size_t n = (size_t)psd.width * psd.height;
    const PsdLayer *colour = color_rgba != NULL ? psd_find_layer(&psd, COLOR_NAMES, 2) : NULL;
    uint8_t *cls = calloc(n, 1), *rgba = colour != NULL ? calloc(n, 4) : NULL;
    if (cls == NULL || (colour != NULL && rgba == NULL)) {
        free(cls), free(rgba), psd_free(&psd);
        if (why != NULL && why_cap > 0) snprintf(why, why_cap, "out of memory");
        return 0;
    }
    paint(&psd, mask, cls, dash_texel);
    const PsdLayer *blades = psd_find_layer(&psd, BLADE_NAMES, 2);
    if (blades != NULL) paint(&psd, blades, cls, blade_texel);
    if (colour != NULL) paint(&psd, colour, rgba, color_texel);
    if (color_rgba != NULL) *color_rgba = rgba;
    *classes = cls, *w = psd.width, *h = psd.height;
    psd_free(&psd);
    return 1;
}

static uint8_t *slurp(const char *path, size_t *len, char *why, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return snprintf(why != NULL ? why : (char[1]){ 0 }, why != NULL ? cap : 1, "the PSD cannot be opened"), NULL;
    uint8_t *buf = NULL;
    size_t n = 0, room = 1 << 20;
    for (;;) {
        uint8_t *bigger = realloc(buf, room);
        if (bigger == NULL) break;
        buf = bigger;
        n += fread(buf + n, 1, room - n, f);
        if (n < room) { fclose(f); *len = n; return buf; }
        room *= 2;
    }
    fclose(f);
    free(buf);
    return snprintf(why != NULL ? why : (char[1]){ 0 }, why != NULL ? cap : 1, "the PSD cannot be read"), NULL;
}

int psd_car_mask(const char *psd_path, uint8_t **grey, uint32_t *w, uint32_t *h, char *why, size_t why_cap)
{
    size_t len;
    uint8_t *bytes = slurp(psd_path, &len, why, why_cap);
    if (bytes == NULL) return 0;
    const int ok = psd_car_mask_bytes(bytes, len, grey, w, h, why, why_cap);
    free(bytes);
    return ok;
}

int psd_dash_mask(const char *psd_path, uint8_t **classes, uint8_t **color_rgba, uint32_t *w, uint32_t *h, char *why, size_t why_cap)
{
    size_t len;
    uint8_t *bytes = slurp(psd_path, &len, why, why_cap);
    if (bytes == NULL) return 0;
    const int ok = psd_dash_mask_bytes(bytes, len, classes, color_rgba, w, h, why, why_cap);
    free(bytes);
    return ok;
}
