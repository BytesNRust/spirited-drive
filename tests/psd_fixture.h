/* A writer of small PSDs for the tests of the PSD reader: version 1, 8 bits, RGB, layers raw or PackBits, Pascal and unicode names, bounds of their own,
 * group markers. Not a general writer: what the reader has to read, and a few wrong files for it to refuse. */
#ifndef TNFS_PSD_FIXTURE_H
#define TNFS_PSD_FIXTURE_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint8_t *p; size_t n, cap; } FxBuf;

typedef struct {
    const char *name;     /* the Pascal name */
    const char *uname;    /* the "luni" name (ASCII, written as UTF-16), or NULL */
    int kind;             /* PSD_LAYER_* group marker (lsct), 0 for none */
    int left, top, right, bottom;
    const uint8_t *rgba;  /* (right - left) x (bottom - top) pixels; NULL for a layer with no pixels */
    int rle;              /* 1: the channels are PackBits */
    int alpha;            /* 1: a transparency channel (-1) */
    int compression;      /* 0: from `rle`; 2 or 3: a ZIP mark with no data behind it */
} FxLayer;

static void fx_put(FxBuf *b, const void *d, size_t n)
{
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2; b->p = realloc(b->p, b->cap); }
    if (n != 0) memcpy(b->p + b->n, d, n);   /* a layer with no pixels puts nothing: d is NULL */
    b->n += n;
}
static void fx_u8(FxBuf *b, unsigned v) { const uint8_t x = (uint8_t)v; fx_put(b, &x, 1); }
static void fx_u16(FxBuf *b, unsigned v) { fx_u8(b, v >> 8); fx_u8(b, v & 255); }
static void fx_u32(FxBuf *b, uint32_t v) { fx_u16(b, v >> 16); fx_u16(b, v & 65535); }
static void fx_patch32(FxBuf *b, size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) b->p[at + (size_t)i] = (uint8_t)(v >> (24 - 8 * i)); }

/* one channel's row as PackBits: repeats for runs of 3 or more, literals otherwise */
static void fx_packbits(FxBuf *b, const uint8_t *row, int w)
{
    int x = 0;
    while (x < w) {
        int run = 1;
        while (x + run < w && run < 128 && row[x + run] == row[x]) ++run;
        if (run >= 3) { fx_u8(b, (unsigned)(1 - run) & 255); fx_u8(b, row[x]); x += run; continue; }
        int lit = 0;
        while (x + lit < w && lit < 128 && !(x + lit + 2 < w && row[x + lit] == row[x + lit + 1] && row[x + lit] == row[x + lit + 2])) ++lit;
        fx_u8(b, (unsigned)(lit - 1)); fx_put(b, row + x, (size_t)lit); x += lit;
    }
}

static void fx_channel(FxBuf *out, const FxLayer *l, int c)
{
    const int w = l->right - l->left, h = l->bottom - l->top;
    uint8_t *plane = malloc((size_t)(w > 0 && h > 0 ? w * h : 1));
    for (int i = 0; i < w * h; ++i) plane[i] = l->rgba != NULL ? l->rgba[i * 4 + c] : 0;
    if (l->compression != 0) { fx_u16(out, (unsigned)l->compression); free(plane); return; }
    fx_u16(out, (unsigned)l->rle);
    if (!l->rle) fx_put(out, plane, (size_t)(w * h));
    else {
        FxBuf rows = { 0 };
        for (int y = 0; y < h; ++y) {
            const size_t before = rows.n;
            fx_packbits(&rows, plane + y * w, w);
            fx_u16(out, (unsigned)(rows.n - before));
        }
        fx_put(out, rows.p, rows.n);
        free(rows.p);
    }
    free(plane);
}

/* a PSD of `count` layers (the bottom first) on a width x height canvas; `depth`, `mode` and `version` are the header's (8, 3, 1 for a good file) */
static FxBuf fx_psd(int width, int height, const FxLayer *layers, int count, int depth, int mode, int version)
{
    FxBuf b = { 0 };
    fx_put(&b, "8BPS", 4); fx_u16(&b, (unsigned)version);
    for (int i = 0; i < 6; ++i) fx_u8(&b, 0);
    fx_u16(&b, 3); fx_u32(&b, (uint32_t)height); fx_u32(&b, (uint32_t)width); fx_u16(&b, (unsigned)depth); fx_u16(&b, (unsigned)mode);
    fx_u32(&b, 0); fx_u32(&b, 0);   /* colour mode data, image resources */
    const size_t section = b.n; fx_u32(&b, 0);
    const size_t info = b.n; fx_u32(&b, 0);
    fx_u16(&b, (unsigned)count);
    FxBuf data[16];
    for (int i = 0; i < count; ++i) {
        const FxLayer *l = &layers[i];
        const int nch = l->kind != 0 ? 0 : l->alpha ? 4 : 3;
        fx_u32(&b, (uint32_t)l->top); fx_u32(&b, (uint32_t)l->left); fx_u32(&b, (uint32_t)l->bottom); fx_u32(&b, (uint32_t)l->right);
        fx_u16(&b, (unsigned)nch);
        memset(&data[i], 0, sizeof data[i]);
        for (int c = 0; c < nch; ++c) {
            const size_t before = data[i].n;
            fx_channel(&data[i], l, c == 3 ? 3 : c);
            fx_u16(&b, c == 3 ? 0xFFFF : (unsigned)c); fx_u32(&b, (uint32_t)(data[i].n - before));
        }
        fx_put(&b, "8BIM", 4); fx_put(&b, "norm", 4); fx_u8(&b, 255); fx_u8(&b, 0); fx_u8(&b, 0); fx_u8(&b, 0);
        FxBuf extra = { 0 };
        fx_u32(&extra, 0); fx_u32(&extra, 0);
        const size_t name_len = strlen(l->name);
        fx_u8(&extra, (unsigned)name_len); fx_put(&extra, l->name, name_len);
        while ((extra.n - 8) % 4 != 0) fx_u8(&extra, 0);
        if (l->kind != 0) { fx_put(&extra, "8BIMlsct", 8); fx_u32(&extra, 4); fx_u32(&extra, (uint32_t)l->kind); }
        if (l->uname != NULL) {
            const size_t n = strlen(l->uname);
            fx_put(&extra, "8BIMluni", 8); fx_u32(&extra, (uint32_t)(4 + n * 2));
            fx_u32(&extra, (uint32_t)n);
            for (size_t k = 0; k < n; ++k) fx_u16(&extra, (unsigned char)l->uname[k]);
        }
        fx_u32(&b, (uint32_t)extra.n); fx_put(&b, extra.p, extra.n);
        free(extra.p);
    }
    for (int i = 0; i < count; ++i) { fx_put(&b, data[i].p, data[i].n); free(data[i].p); }
    fx_patch32(&b, info, (uint32_t)(b.n - info - 4));
    fx_u32(&b, 0);   /* the global layer mask info */
    fx_patch32(&b, section, (uint32_t)(b.n - section - 4));
    return b;
}

#endif
