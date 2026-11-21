/* psd_read.h: the PSD reader. The format's numbers are big-endian. Every read goes through a cursor that records running past the end, so a
 * truncated or lying file fails in words and never reads outside `bytes`. */
#include "psd_read.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_CHANNELS = 64, MAX_LAYERS = 4096, MAX_SIDE = 30000 };
enum { COMPRESSION_RAW = 0, COMPRESSION_RLE = 1 };
#define MAX_PIXELS ((uint64_t)1 << 28)

typedef struct {
    const uint8_t *p, *end;
    int bad;
} Cur;

typedef struct {
    int16_t id[MAX_CHANNELS];
    uint32_t len[MAX_CHANNELS];
    int count;
} Channels;

static const uint8_t *take(Cur *c, size_t n)
{
    if (c->bad || (size_t)(c->end - c->p) < n) return c->bad = 1, NULL;
    const uint8_t *at = c->p;
    c->p += n;
    return at;
}

static uint32_t rd(Cur *c, int bytes)
{
    const uint8_t *b = take(c, (size_t)bytes);
    uint32_t v = 0;
    for (int i = 0; b != NULL && i < bytes; ++i) v = v << 8 | b[i];
    return v;
}

static void fail(char *why, size_t cap, const char *text)
{
    if (why != NULL && cap > 0) snprintf(why, cap, "%s", text);
}

static Cur sub(Cur *c, size_t n)   /* the next n bytes as a cursor of their own, which the outer one steps over */
{
    const uint8_t *at = take(c, n);
    Cur s = { at, at != NULL ? at + n : NULL, at == NULL };
    return s;
}

/* a layer's name, trimmed and lower case, is one of `names` */
static int name_is(const char *name, const char *const *names, int count)
{
    while (*name != '\0' && isspace((unsigned char)*name)) ++name;
    size_t n = strlen(name);
    while (n > 0 && isspace((unsigned char)name[n - 1])) --n;
    for (int i = 0; i < count; ++i) {
        if (strlen(names[i]) != n) continue;
        size_t k = 0;
        while (k < n && tolower((unsigned char)name[k]) == tolower((unsigned char)names[i][k])) ++k;
        if (k == n) return 1;
    }
    return 0;
}

/* UTF-16BE units to UTF-8 (a lone surrogate becomes U+FFFD) */
static char *utf8_of(Cur *c, uint32_t units)
{
    char *out = malloc((size_t)units * 4 + 1);
    if (out == NULL) return NULL;
    size_t n = 0;
    for (uint32_t i = 0; i < units && !c->bad; ++i) {
        uint32_t u = rd(c, 2);
        if (u >= 0xD800 && u < 0xDC00 && i + 1 < units) {
            const uint8_t *q = c->p;
            const uint32_t lo = c->end - q >= 2 ? (uint32_t)q[0] << 8 | q[1] : 0;
            if (lo >= 0xDC00 && lo < 0xE000) { u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00); rd(c, 2); ++i; }
            else u = 0xFFFD;
        } else if (u >= 0xD800 && u < 0xE000) u = 0xFFFD;
        if (u < 0x80) out[n++] = (char)u;
        else if (u < 0x800) { out[n++] = (char)(0xC0 | u >> 6); out[n++] = (char)(0x80 | (u & 63)); }
        else if (u < 0x10000) { out[n++] = (char)(0xE0 | u >> 12); out[n++] = (char)(0x80 | ((u >> 6) & 63)); out[n++] = (char)(0x80 | (u & 63)); }
        else { out[n++] = (char)(0xF0 | u >> 18); out[n++] = (char)(0x80 | ((u >> 12) & 63)); out[n++] = (char)(0x80 | ((u >> 6) & 63)); out[n++] = (char)(0x80 | (u & 63)); }
    }
    out[n] = '\0';
    return out;
}

/* One layer's tagged blocks: "luni" (the unicode name) and "lsct" (the group marker); the rest is skipped by length. */
static void read_blocks(Cur *x, PsdLayer *layer)
{
    while (!x->bad && x->end - x->p >= 12) {
        const uint8_t *sig = take(x, 4), *key = take(x, 4);
        const uint32_t len = rd(x, 4);
        if (x->bad || (memcmp(sig, "8BIM", 4) != 0 && memcmp(sig, "8B64", 4) != 0)) return;
        Cur block = sub(x, len);
        if (block.bad) return;
        if (memcmp(key, "lsct", 4) == 0) {
            const uint32_t kind = rd(&block, 4);
            if (!block.bad && kind <= PSD_LAYER_DIVIDER) layer->kind = (int)kind;
        } else if (memcmp(key, "luni", 4) == 0) {
            const uint32_t units = rd(&block, 4);
            if (!block.bad && (size_t)(block.end - block.p) / 2 >= units) {
                char *text = utf8_of(&block, units);
                if (text != NULL) { free(layer->name); layer->name = text; }
            }
        }
    }
}

/* one layer record: its bounds, channels and name, up to the next record */
static int read_record(Cur *c, PsdLayer *layer, Channels *ch)
{
    layer->top = (int32_t)rd(c, 4), layer->left = (int32_t)rd(c, 4), layer->bottom = (int32_t)rd(c, 4), layer->right = (int32_t)rd(c, 4);
    const uint32_t count = rd(c, 2);
    if (c->bad || count > MAX_CHANNELS) return 0;
    ch->count = (int)count;
    for (uint32_t i = 0; i < count; ++i) ch->id[i] = (int16_t)rd(c, 2), ch->len[i] = rd(c, 4);
    take(c, 12);   /* "8BIM", the blend key, opacity, clipping, flags, filler */
    Cur extra = sub(c, rd(c, 4));
    if (c->bad) return 0;
    take(&extra, rd(&extra, 4));   /* the layer mask */
    take(&extra, rd(&extra, 4));   /* the blending ranges */
    const uint32_t name_len = rd(&extra, 1);
    const uint8_t *name = take(&extra, name_len);
    if (extra.bad) return 0;
    take(&extra, (4 - (name_len + 1) % 4) % 4);
    layer->name = malloc((size_t)name_len + 1);
    if (layer->name == NULL) return 0;
    memcpy(layer->name, name, name_len);
    layer->name[name_len] = '\0';
    read_blocks(&extra, layer);
    return 1;
}

/* a channel's w x h bytes into `plane`: 1 when they read */
static int read_channel(const uint8_t *data, size_t len, uint32_t w, uint32_t h, uint8_t *plane, char *why, size_t cap)
{
    Cur c = { data, data + len, 0 };
    const uint32_t mode = rd(&c, 2);
    if (mode == COMPRESSION_RAW) {
        const uint8_t *src = take(&c, (size_t)w * h);
        if (src == NULL) return fail(why, cap, "the PSD is truncated"), 0;
        memcpy(plane, src, (size_t)w * h);
        return 1;
    }
    if (mode != COMPRESSION_RLE) return fail(why, cap, "a layer is ZIP compressed: save the PSD without compression or with RLE"), 0;
    Cur counts = sub(&c, (size_t)h * 2);
    if (c.bad) return fail(why, cap, "the PSD is truncated"), 0;
    for (uint32_t y = 0; y < h; ++y) {
        Cur row = sub(&c, rd(&counts, 2));
        uint32_t x = 0;
        while (!row.bad && x < w && row.p < row.end) {
            const int n = (int8_t)rd(&row, 1);
            if (n >= 0) {   /* n + 1 bytes as they are */
                const uint8_t *src = take(&row, (size_t)n + 1);
                if (src == NULL || x + (uint32_t)n + 1 > w) return fail(why, cap, "a layer's RLE row is wrong"), 0;
                memcpy(plane + (size_t)y * w + x, src, (size_t)n + 1);
                x += (uint32_t)n + 1;
            } else if (n != -128) {   /* one byte, 1 - n times */
                const uint8_t *src = take(&row, 1);
                const uint32_t run = (uint32_t)(1 - n);
                if (src == NULL || x + run > w) return fail(why, cap, "a layer's RLE row is wrong"), 0;
                memset(plane + (size_t)y * w + x, *src, run);
                x += run;
            }
        }
        if (c.bad || row.bad || x != w) return fail(why, cap, "a layer's RLE row is wrong"), 0;
    }
    return 1;
}

/* A wanted layer's pixels: the planes of channels 0, 1, 2 and -1 (alpha, 255 without) made into RGBA. */
static int read_pixels(PsdLayer *layer, const Channels *ch, const uint8_t *const *data, char *why, size_t cap)
{
    const uint32_t w = (uint32_t)(layer->right - layer->left), h = (uint32_t)(layer->bottom - layer->top);
    const size_t n = (size_t)w * h;
    layer->rgba = malloc(n * 4);
    uint8_t *plane = malloc(n);
    if (layer->rgba == NULL || plane == NULL) return free(plane), fail(why, cap, "out of memory"), 0;
    memset(layer->rgba, 255, n * 4);
    int seen = 0;
    for (int i = 0; i < ch->count; ++i) {
        const int slot = ch->id[i] == -1 ? 3 : ch->id[i];
        if (slot < 0 || slot > 3) continue;   /* a layer mask's channel: ignored, as the Python reader does */
        if (!read_channel(data[i], ch->len[i], w, h, plane, why, cap)) return free(plane), 0;
        for (size_t k = 0; k < n; ++k) layer->rgba[k * 4 + (size_t)slot] = plane[k];
        if (slot < 3) seen |= 1 << slot;
    }
    free(plane);
    if (seen != 7) return fail(why, cap, "a layer lacks its colour channels"), 0;
    return 1;
}

static int read_header(Cur *c, PsdFile *out, char *why, size_t cap)
{
    const uint8_t *sig = take(c, 4);
    if (sig == NULL || memcmp(sig, "8BPS", 4) != 0) return fail(why, cap, "not a PSD file"), 0;
    const uint32_t version = rd(c, 2);
    if (version == 2) return fail(why, cap, "a PSB (large document) is not supported: save it as a PSD"), 0;
    if (version != 1) return fail(why, cap, "not a PSD file"), 0;
    take(c, 6);
    rd(c, 2);   /* the channels of the merged image */
    out->height = rd(c, 4), out->width = rd(c, 4);
    const uint32_t depth = rd(c, 2), mode = rd(c, 2);
    if (c->bad) return fail(why, cap, "the PSD is truncated"), 0;
    if (depth != 8) { char t[96]; snprintf(t, sizeof t, "a %u-bit PSD: save it as 8 bits per channel", (unsigned)depth); return fail(why, cap, t), 0; }
    if (mode != 3) return fail(why, cap, "the PSD is not RGB: change the image's mode to RGB"), 0;
    if (out->width == 0 || out->height == 0 || out->width > MAX_SIDE || out->height > MAX_SIDE) return fail(why, cap, "the PSD's size is not usable"), 0;
    take(c, rd(c, 4));   /* the colour mode data */
    take(c, rd(c, 4));   /* the image resources */
    return !c->bad || (fail(why, cap, "the PSD is truncated"), 0);
}

static int layer_area_ok(const PsdLayer *l)
{
    const int64_t w = (int64_t)l->right - l->left, h = (int64_t)l->bottom - l->top;
    return w >= 0 && h >= 0 && (uint64_t)w * (uint64_t)h <= MAX_PIXELS;
}

static int read_layers(Cur *c, const char *const *want, int want_count, PsdFile *out, char *why, size_t cap)
{
    Cur section = sub(c, rd(c, 4));
    Cur info = sub(&section, rd(&section, 4));
    if (section.bad || info.bad) return fail(why, cap, "the PSD is truncated"), 0;
    const int32_t raw = (int32_t)(int16_t)rd(&info, 2);
    const int count = raw < 0 ? -raw : raw;
    if (info.bad || count == 0) return fail(why, cap, "the PSD has no layers"), 0;
    if (count > MAX_LAYERS) return fail(why, cap, "the PSD has too many layers"), 0;
    Channels *ch = calloc((size_t)count, sizeof *ch);
    out->layers = calloc((size_t)count, sizeof *out->layers);
    if (ch == NULL || out->layers == NULL) return free(ch), fail(why, cap, "out of memory"), 0;
    out->count = count;
    int ok = 1;
    for (int i = 0; ok && i < count; ++i) {
        ok = read_record(&info, &out->layers[i], &ch[i]);
        if (!ok) fail(why, cap, "the PSD is truncated or a layer record is wrong");
        else if (!layer_area_ok(&out->layers[i])) ok = 0, fail(why, cap, "a layer's size is not usable");
    }
    for (int i = 0; ok && i < count; ++i) {   /* the channels' data, layer after layer */
        PsdLayer *l = &out->layers[i];
        const uint8_t *data[MAX_CHANNELS];
        for (int k = 0; ok && k < ch[i].count; ++k) {
            data[k] = take(&info, ch[i].len[k]);
            if (data[k] == NULL) ok = 0, fail(why, cap, "the PSD is truncated");
        }
        const int area = l->right > l->left && l->bottom > l->top;
        if (ok && area && l->kind == PSD_LAYER_NORMAL && name_is(l->name, want, want_count)) ok = read_pixels(l, &ch[i], data, why, cap);
    }
    free(ch);
    return ok;
}

int psd_parse(const uint8_t *bytes, size_t len, const char *const *want, int want_count, PsdFile *out, char *why, size_t why_cap)
{
    memset(out, 0, sizeof *out);
    Cur c = { bytes, bytes + len, 0 };
    if (!read_header(&c, out, why, why_cap) || !read_layers(&c, want, want_count, out, why, why_cap)) return psd_free(out), 0;
    return 1;
}

void psd_free(PsdFile *psd)
{
    for (int i = 0; i < psd->count; ++i) free(psd->layers[i].name), free(psd->layers[i].rgba);
    free(psd->layers);
    memset(psd, 0, sizeof *psd);
}

const PsdLayer *psd_find_layer(const PsdFile *psd, const char *const *names, int name_count)
{
    for (int i = psd->count - 1; i >= 0; --i) {
        const PsdLayer *l = &psd->layers[i];
        if (l->kind == PSD_LAYER_NORMAL && name_is(l->name, names, name_count)) return l;
    }
    return NULL;
}
