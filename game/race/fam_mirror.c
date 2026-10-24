/* port: the art of a race the CD holds no FAM for (fam_mirror.h). */
#include "game/race/fam_mirror.h"

#include "formats/chunk.h"

#include <string.h>

enum { TERRAIN = 0, SCENERY = 1, TRANSPARENT = 0xff };

static const char PAL_NAME[4] = { '!', 'p', 'a', 'l' };

/* One bitmap the three FAMs share by place and name, all the same size: the target's to write. */
typedef struct {
    uint8_t *target;
    const uint8_t *reference, *other;
    int width, height, root_child;
    const char *name;   /* the bitmap's 4 characters */
} Shared;

typedef void (*SharedFn)(void *ctx, const Shared *s);

/* 1 when `m` is `r` flipped left to right, both w x h */
static int is_flip(const uint8_t *r, const uint8_t *m, int w, int h)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (m[y * w + x] != r[y * w + (w - 1 - x)]) return 0;
    return 1;
}

static void flip(uint8_t *p, int w, int h)
{
    for (int y = 0; y < h; y++)
        for (int a = 0, b = w - 1; a < b; a++, b--) {
            const uint8_t t = p[y * w + a];
            p[y * w + a] = p[y * w + b], p[y * w + b] = t;
        }
}

static int bitmap_named(const Fsh *f, const char name[4], FshEntry *e)
{
    uint32_t j;
    return fsh_find(f, name, &j) == FSH_OK && fsh_entry(f, j, e) == FSH_OK && e->type == FSH_TYPE_BITMAP;
}

static void bank(uint8_t *target, const Chunk *t, const Chunk *r, const Chunk *o, int root_child, SharedFn fn,
                 void *ctx)
{
    Fsh tf, rf, of;
    if (chunk_bitmaps(t, &tf) != FSH_OK || chunk_bitmaps(r, &rf) != FSH_OK || chunk_bitmaps(o, &of) != FSH_OK) return;
    for (uint32_t i = 0; i < tf.count; i++) {
        FshEntry te, re, oe;
        if (fsh_entry(&tf, i, &te) != FSH_OK || te.type != FSH_TYPE_BITMAP) continue;
        if (!bitmap_named(&rf, te.name, &re) || !bitmap_named(&of, te.name, &oe)) continue;
        if (re.width != te.width || re.height != te.height || oe.width != te.width || oe.height != te.height) continue;
        const Shared s = { target + (te.payload - target), re.payload, oe.payload, te.width, te.height, root_child,
                           te.name };
        fn(ctx, &s);
    }
}

static void walk(uint8_t *target, const Chunk *t, const Chunk *r, const Chunk *o, int depth, int root_child,
                 SharedFn fn, void *ctx)
{
    if (t->kind != r->kind || t->kind != o->kind || depth > CHUNK_MAX_DEPTH) return;
    if (t->kind == CHUNK_KIND_BITMAP) {
        bank(target, t, r, o, root_child, fn, ctx);
        return;
    }
    if (t->kind != CHUNK_KIND_HEADER) return;
    for (uint32_t i = 0; i < t->count && i < r->count && i < o->count; i++) {
        Chunk tc, rc, oc;
        if (chunk_child(t, i, &tc) == CHUNK_OK && chunk_child(r, i, &rc) == CHUNK_OK && chunk_child(o, i, &oc) == CHUNK_OK)
            walk(target, &tc, &rc, &oc, depth + 1, depth == 0 ? (int)i : root_child, fn, ctx);
    }
}

static int each_shared(uint8_t *target, size_t target_size, const uint8_t *reference, size_t reference_size,
                       const uint8_t *other, size_t other_size, SharedFn fn, void *ctx)
{
    Chunk t, r, o;
    if (chunk_open(&t, target, target_size) != CHUNK_OK || chunk_open(&r, reference, reference_size) != CHUNK_OK ||
        chunk_open(&o, other, other_size) != CHUNK_OK)
        return 0;
    walk(target, &t, &r, &o, 0, -1, fn, ctx);
    return 1;
}

static void sign(void *ctx, const Shared *s)
{
    const size_t n = (size_t)s->width * s->height;
    if (memcmp(s->reference, s->other, n) == 0 || !is_flip(s->reference, s->other, s->width, s->height)) return;
    flip(s->target, s->width, s->height);
    ++*(int *)ctx;
}

int fam_mirror_signs(uint8_t *target, size_t target_size, const uint8_t *reference, size_t reference_size,
                     const uint8_t *mirror, size_t mirror_size)
{
    int flipped = 0;
    each_shared(target, target_size, reference, reference_size, mirror, mirror_size, sign, &flipped);
    return flipped;
}

/* the bitmaps to flip by name, under one root child */
typedef struct {
    uint8_t *fam;
    const char (*names)[4];
    int count, root_child, flipped;
} Named;

static void flip_named(void *ctx, const Shared *s)
{
    Named *const g = ctx;
    if (s->root_child != g->root_child) return;
    for (int i = 0; i < g->count; i++)
        if (memcmp(s->name, g->names[i], 4) == 0) {
            flip(s->target, s->width, s->height);
            g->flipped++;
            return;
        }
}

int fam_flip_named(uint8_t *fam, size_t size, int root_child, const char (*names)[4], int count)
{
    Named g = { fam, names, count, root_child, 0 };
    each_shared(fam, size, fam, size, fam, size, flip_named, &g);
    return g.flipped;
}

/* a FAM's track palette (terrain bank 0's "!pal"), 8 bits a channel */
static int track_palette(const uint8_t *fam, size_t size, uint8_t rgb[768])
{
    Chunk root, terrain, first;
    Fsh f;
    FshEntry e;
    uint32_t j;
    return chunk_open(&root, fam, size) == CHUNK_OK && chunk_child(&root, TERRAIN, &terrain) == CHUNK_OK &&
           terrain.kind == CHUNK_KIND_HEADER && chunk_child(&terrain, 0, &first) == CHUNK_OK &&
           chunk_bitmaps(&first, &f) == FSH_OK && fsh_find(&f, PAL_NAME, &j) == FSH_OK && fsh_entry(&f, j, &e) == FSH_OK &&
           fsh_palette_rgb(&e, rgb) == FSH_OK;
}

/* per channel least squares of the timed colour on the day colour: sums over the pixels the two share by place */
typedef struct {
    const uint8_t *day_rgb, *timed_rgb;
    double n, x[3], y[3], xx[3], xy[3];
} Fit;

static void fit_pair(void *ctx, const Shared *s)
{
    Fit *f = ctx;
    if (s->root_child != TERRAIN && s->root_child != SCENERY) return;
    const size_t n = (size_t)s->width * s->height;
    for (size_t i = 0; i < n; i++) {
        const uint8_t d = s->other[i], t = s->target[i];
        if (d == TRANSPARENT || t == TRANSPARENT) continue;
        f->n += 1;
        for (int c = 0; c < 3; c++) {
            const double x = f->day_rgb[3 * d + c], y = f->timed_rgb[3 * t + c];
            f->x[c] += x, f->y[c] += y, f->xx[c] += x * x, f->xy[c] += x * y;
        }
    }
}

static int nearest(const uint8_t rgb[768], const double c[3])
{
    int best = 0;
    double best_d = 1e30;
    for (int i = 0; i < TRANSPARENT; i++) {
        double d = 0;
        for (int k = 0; k < 3; k++) {
            const double e = rgb[3 * i + k] - c[k];
            d += e * e;
        }
        if (d < best_d) best_d = d, best = i;
    }
    return best;
}

typedef struct {
    const uint8_t *map;
    int copied;
} Copy;

/* the rally's own bitmaps (not the day's) into the timed art, through the map */
static void copy_rally(void *ctx, const Shared *s)
{
    Copy *c = ctx;
    if (s->root_child != TERRAIN && s->root_child != SCENERY) return;
    const size_t n = (size_t)s->width * s->height;
    if (memcmp(s->reference, s->other, n) == 0) return;
    for (size_t i = 0; i < n; i++) s->target[i] = c->map[s->other[i]];
    c->copied++;
}

/* `src` (sw x sh, its rows from `top` down) into `dst` (dw x dh), nearest sampled, each pixel through `map` */
static void scale_into(uint8_t *dst, int dw, int dh, const uint8_t *src, int sw, int sh, int top, const uint8_t *map)
{
    for (int y = 0; y < dh; y++) {
        const uint8_t *row = src + (size_t)(top + y * (sh - top) / dh) * sw;
        for (int x = 0; x < dw; x++) dst[y * dw + x] = map[row[x * sw / dw]];
    }
}

typedef void (*BankFn)(void *ctx, uint8_t *target, const Fsh *t, const Fsh *r, const Fsh *o, int root_child);

/* walk's shape, a bank at a time (the three FSHs a place holds) */
static void walk_banks(uint8_t *target, const Chunk *t, const Chunk *r, const Chunk *o, int depth, int root_child,
                       BankFn fn, void *ctx)
{
    if (t->kind != r->kind || t->kind != o->kind || depth > CHUNK_MAX_DEPTH) return;
    if (t->kind == CHUNK_KIND_BITMAP) {
        Fsh tf, rf, of;
        if (chunk_bitmaps(t, &tf) == FSH_OK && chunk_bitmaps(r, &rf) == FSH_OK && chunk_bitmaps(o, &of) == FSH_OK)
            fn(ctx, target, &tf, &rf, &of, root_child);
        return;
    }
    if (t->kind != CHUNK_KIND_HEADER) return;
    for (uint32_t i = 0; i < t->count && i < r->count && i < o->count; i++) {
        Chunk tc, rc, oc;
        if (chunk_child(t, i, &tc) == CHUNK_OK && chunk_child(r, i, &rc) == CHUNK_OK && chunk_child(o, i, &oc) == CHUNK_OK)
            walk_banks(target, &tc, &rc, &oc, depth + 1, depth == 0 ? (int)i : root_child, fn, ctx);
    }
}

/* each far variant ("A004", "01A4") its near one ("A000", "01A0"), scaled to its size */
static void far_from_near(void *ctx, uint8_t *target, const Fsh *t, const Fsh *r, const Fsh *o, int root_child)
{
    (void)r, (void)o;
    if (root_child != TERRAIN) return;
    static uint8_t identity[256];
    for (int i = 0; i < 256; i++) identity[i] = (uint8_t)i;
    for (uint32_t i = 0; i < t->count; i++) {
        FshEntry fe, ne;
        if (fsh_entry(t, i, &fe) != FSH_OK || fe.type != FSH_TYPE_BITMAP || fe.name[3] != '4' ||
            (size_t)fe.width * fe.height > fe.payload_size)
            continue;
        char near[4];
        memcpy(near, fe.name, 4), near[3] = '0';
        if (!bitmap_named(t, near, &ne) || (size_t)ne.width * ne.height > ne.payload_size) continue;
        scale_into(target + (fe.payload - target), fe.width, fe.height, ne.payload, ne.width, ne.height, 0, identity);
        ++*(int *)ctx;
    }
}

int fam_rally_far(uint8_t *fam, size_t size)
{
    Chunk root;
    int filled = 0;
    if (chunk_open(&root, fam, size) == CHUNK_OK) walk_banks(fam, &root, &root, &root, 0, -1, far_from_near, &filled);
    return filled;
}

int fam_time_rally(uint8_t *timed, size_t timed_size, const uint8_t *day, size_t day_size, const uint8_t *rally,
                   size_t rally_size)
{
    uint8_t day_rgb[768], timed_rgb[768], rally_rgb[768];
    if (!track_palette(day, day_size, day_rgb) || !track_palette(timed, timed_size, timed_rgb) ||
        !track_palette(rally, rally_size, rally_rgb))
        return 0;
    Fit f = { day_rgb, timed_rgb, 0, { 0 }, { 0 }, { 0 }, { 0 } };
    each_shared(timed, timed_size, timed, timed_size, day, day_size, fit_pair, &f);
    double gain[3] = { 1, 1, 1 }, offset[3] = { 0, 0, 0 };
    for (int c = 0; c < 3 && f.n > 0; c++) {
        const double var = f.n * f.xx[c] - f.x[c] * f.x[c];
        if (var > 0) gain[c] = (f.n * f.xy[c] - f.x[c] * f.y[c]) / var;
        offset[c] = (f.y[c] - gain[c] * f.x[c]) / f.n;
    }
    uint8_t map[256];
    for (int i = 0; i < TRANSPARENT; i++) {
        double c[3];
        for (int k = 0; k < 3; k++) {
            c[k] = gain[k] * rally_rgb[3 * i + k] + offset[k];
            c[k] = c[k] < 0 ? 0 : c[k] > 255 ? 255 : c[k];
        }
        map[i] = (uint8_t)nearest(timed_rgb, c);
    }
    map[TRANSPARENT] = TRANSPARENT;
    Copy copy = { map, 0 };
    each_shared(timed, timed_size, day, day_size, rally, rally_size, copy_rally, &copy);
    return copy.copied;
}

/* the bitmap `name` under root child `root_child`, writable within `fam` */
typedef struct {
    int root_child;
    const char *name;
    uint8_t *at;
    int width, height;
} Find;

static void find_named(void *ctx, const Shared *s)
{
    Find *f = ctx;
    if (s->root_child == f->root_child && memcmp(s->name, f->name, 4) == 0 && f->at == NULL)
        f->at = s->target, f->width = s->width, f->height = s->height;
}

int fam_fill_named(uint8_t *fam, size_t size, int root_child, const char dst[4], const char src[4], int top_quarters)
{
    Find d = { root_child, dst, NULL, 0, 0 }, s = { root_child, src, NULL, 0, 0 };
    each_shared(fam, size, fam, size, fam, size, find_named, &d);
    each_shared(fam, size, fam, size, fam, size, find_named, &s);
    if (d.at == NULL || s.at == NULL || top_quarters < 0 || top_quarters > 3) return 0;
    uint8_t identity[256];
    for (int i = 0; i < 256; i++) identity[i] = (uint8_t)i;
    scale_into(d.at, d.width, d.height, s.at, s.width, s.height, s.height * top_quarters / 4, identity);
    return 1;
}
