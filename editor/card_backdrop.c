#include "card_backdrop.h"

#include "formats/png.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CardBackdrop card_backdrop_default(void)
{
    CardBackdrop b;
    memset(&b, 0, sizeof b);
    b.kind = CARD_BACKDROP_PLAIN;
    b.top[0] = 24, b.top[1] = 28, b.top[2] = 36;
    memcpy(b.bottom, b.top, 3);
    return b;
}

static int refuse(char *why, size_t cap, const char *what, const char *detail)
{
    if (why != NULL && cap > 0) snprintf(why, cap, detail != NULL ? "%s: %s" : "%s", what, detail);
    return 0;
}

/* the whole file `path`, malloc'd; NULL with `why` */
static uint8_t *read_all(const char *path, size_t *size, char *why, size_t why_cap)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        snprintf(why, why_cap, "cannot read %s: %s", path, strerror(errno));
        return NULL;
    }
    size_t cap = 1 << 16, len = 0;
    uint8_t *data = malloc(cap);
    while (data != NULL) {
        const size_t got = fread(data + len, 1, cap - len, f);
        len += got;
        if (len < cap) break;
        uint8_t *bigger = cap > (64u << 20) ? NULL : realloc(data, cap * 2);
        if (bigger == NULL) {
            free(data);
            data = NULL;
            break;
        }
        data = bigger, cap *= 2;
    }
    fclose(f);
    if (data == NULL) snprintf(why, why_cap, "%s is too big or there is no memory", path);
    *size = len;
    return data;
}

/* the picture as RGBA (indexed ones expanded); NULL with `why` */
static uint8_t *picture_rgba(const char *path, uint32_t *w, uint32_t *h, char *why, size_t why_cap)
{
    size_t size = 0;
    uint8_t *file = read_all(path, &size, why, why_cap);
    if (file == NULL) return NULL;
    PngImage img;
    const PngResult r = png_read(file, size, &img);
    free(file);
    if (r != PNG_OK) {
        snprintf(why, why_cap, "%s is not a picture Studio can read (%s)", path, png_result_str(r));
        return NULL;
    }
    *w = img.width, *h = img.height;
    if (!img.indexed) return img.pixels;
    uint8_t *rgba = malloc((size_t)img.width * img.height * 4);
    if (rgba != NULL)
        for (size_t i = 0; i < (size_t)img.width * img.height; ++i) {
            memcpy(rgba + 4 * i, img.palette + 3 * (size_t)img.pixels[i], 3);
            rgba[4 * i + 3] = 255;
        }
    png_free(img.pixels);
    if (rgba == NULL) snprintf(why, why_cap, "no memory for %s", path);
    return rgba;
}

/* the picture covering w x h (scaled to fill it, the overhang cut evenly): each pixel the average of the source pixels it covers, over `under` where see-through */
static void cover(const uint8_t *src, uint32_t sw, uint32_t sh, int w, int h, const uint8_t under[3], uint8_t *rgba)
{
    const double scale = fmax((double)w / sw, (double)h / sh);
    const double ox = ((double)sw - w / scale) / 2, oy = ((double)sh - h / scale) / 2;
    for (int y = 0; y < h; ++y) {
        int y0 = (int)floor(oy + y / scale), y1 = (int)ceil(oy + (y + 1) / scale);
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < w; ++x) {
            int x0 = (int)floor(ox + x / scale), x1 = (int)ceil(ox + (x + 1) / scale);
            if (x1 <= x0) x1 = x0 + 1;
            long sum[3] = { 0, 0, 0 }, n = 0;
            for (int sy = y0 < 0 ? 0 : y0; sy < y1 && sy < (int)sh; ++sy)
                for (int sx = x0 < 0 ? 0 : x0; sx < x1 && sx < (int)sw; ++sx) {
                    const uint8_t *p = src + 4 * ((size_t)sy * sw + (size_t)sx);
                    for (int k = 0; k < 3; ++k) sum[k] += (p[k] * p[3] + under[k] * (255 - p[3]) + 127) / 255;
                    n++;
                }
            uint8_t *out = rgba + 4 * ((size_t)y * w + x);
            for (int k = 0; k < 3; ++k) out[k] = n > 0 ? (uint8_t)((sum[k] + n / 2) / n) : under[k];
            out[3] = 255;
        }
    }
}

int card_backdrop_fill(const CardBackdrop *b, int w, int h, uint8_t *rgba, char *why, size_t why_cap)
{
    if (b == NULL || rgba == NULL || w < 1 || h < 1) return refuse(why, why_cap, "no backdrop to make", NULL);
    if (b->kind == CARD_BACKDROP_FILE) {
        if (b->file[0] == '\0') return refuse(why, why_cap, "choose the backdrop's picture", NULL);
        uint32_t sw = 0, sh = 0;
        char reason[400] = "";
        uint8_t *src = picture_rgba(b->file, &sw, &sh, reason, sizeof reason);
        if (src == NULL) return refuse(why, why_cap, reason, NULL);
        cover(src, sw, sh, w, h, b->top, rgba);
        png_free(src);
        return 1;
    }
    for (int y = 0; y < h; ++y) {
        uint8_t colour[3];
        for (int k = 0; k < 3; ++k) {
            const double t = b->kind == CARD_BACKDROP_GRADIENT && h > 1 ? (double)y / (h - 1) : 0.0;
            colour[k] = (uint8_t)lround(b->top[k] + (b->bottom[k] - b->top[k]) * t);
        }
        for (int x = 0; x < w; ++x) memcpy(rgba + 4 * ((size_t)y * w + x), colour, 3), rgba[4 * ((size_t)y * w + x) + 3] = 255;
    }
    return 1;
}
