/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see credits_relief.h. The thresholds were set on the owner's approved mockups (i030), from the PC SE CD's credits.fsh. */
#include "game/frontend/credits_relief.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    W = CREDITS_RELIEF_W, H = CREDITS_RELIEF_H, N = W * H, PAGES = CREDITS_RELIEF_PAGES,
    LOGO_W = 286, LOGO_H = 84,   /* the small logo at the top left, on every page: left as it is */
    BANNER_ROWS = 70,            /* the configuration screens' banner and rule: no estimate from them up there */
    GOOD_FROM_ROW = 62,          /* rows the relighting may learn from */
    MEDIAN_R = 7,                /* the smoothed medoid: a 15 x 15 median */
    RELIGHT_R = 20,              /* the relighting's window: 41 x 41 */
    AMP_R = 10, AMP_GROW = 10,   /* where the configuration relief has its "&": 21 x 21 means, grown by 10 */
    AMP_LEVEL = 22,              /* that much brighter than the credits there */
    KEEP = 18,                   /* the medoid kept when this near the estimate */
    BAD = 26,                    /* a chosen pixel this far from the estimate is a leftover: filled from its neighbours */
    FILL_PASSES = 60,
    BLEND_R = 4,                 /* the soft edge between the two estimates: three 9 x 9 means */
    DARK = 35,                   /* darker than the smoothed choice by this much: a shadow left by text */
    TEXT_GROW = 2,               /* a page's text: its own colours grown by this much, over their soft edge */
    TEXT_RATIO_PERCENT = 8       /* an entry is the text's when the relief alone uses it this rarely, against the pages' use of it */
};

const uint8_t CREDITS_RELIEF_TEXT_RGB[CREDITS_RELIEF_TEXT_COLOURS][3] = {
    { 255, 206, 6 }, { 222, 180, 40 }, { 190, 155, 70 }, { 170, 137, 4 }, { 85, 69, 2 },   /* TEXT_YELLOW, towards the relief and towards black */
    { 232, 211, 183 }, { 200, 180, 150 }, { 140, 130, 115 },                            /* TEXT_CREAM, towards the relief, TEXT_DIM */
    { 255, 255, 255 }, { 0, 0, 0 } };

static float lum3(const uint8_t *c) { return 0.30f * c[0] + 0.59f * c[1] + 0.11f * c[2]; }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* the mean over a (2r+1)^2 square, the picture's edge repeated outward */
static void box_mean(const float *in, float *out, float *tmp, int r)
{
    const float k = 1.0f / (float)(2 * r + 1);
    for (int y = 0; y < H; y++) {
        const float *row = in + y * W;
        float s = 0;
        for (int i = -r; i <= r; i++) s += row[clampi(i, 0, W - 1)];
        for (int x = 0; x < W; x++) {
            tmp[y * W + x] = s * k;
            s += row[clampi(x + r + 1, 0, W - 1)] - row[clampi(x - r, 0, W - 1)];
        }
    }
    for (int x = 0; x < W; x++) {
        float s = 0;
        for (int i = -r; i <= r; i++) s += tmp[clampi(i, 0, H - 1) * W + x];
        for (int y = 0; y < H; y++) {
            out[y * W + x] = s * k;
            s += tmp[clampi(y + r + 1, 0, H - 1) * W + x] - tmp[clampi(y - r, 0, H - 1) * W + x];
        }
    }
}

/* a mask grown by r in every direction (a square) */
static void grow(uint8_t *mask, uint8_t *tmp, int r)
{
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            uint8_t v = 0;
            for (int i = clampi(x - r, 0, W - 1); i <= clampi(x + r, 0, W - 1) && !v; i++) v = mask[y * W + i];
            tmp[y * W + x] = v;
        }
    for (int x = 0; x < W; x++)
        for (int y = 0; y < H; y++) {
            uint8_t v = 0;
            for (int i = clampi(y - r, 0, H - 1); i <= clampi(y + r, 0, H - 1) && !v; i++) v = tmp[i * W + x];
            mask[y * W + x] = v;
        }
}

/* the median of a (2r+1)^2 square of 0..255 values, the edge repeated: a running histogram along each row */
static void median(const uint8_t *in, uint8_t *out, int r)
{
    const int half = (2 * r + 1) * (2 * r + 1) / 2 + 1;
    for (int y = 0; y < H; y++) {
        int hist[256] = { 0 };
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) hist[in[clampi(y + dy, 0, H - 1) * W + clampi(dx, 0, W - 1)]]++;
        for (int x = 0; x < W; x++) {
            int count = 0, v = 0;
            while (count + hist[v] < half) count += hist[v++];
            out[y * W + x] = (uint8_t)v;
            for (int dy = -r; dy <= r; dy++) {
                const int row = clampi(y + dy, 0, H - 1) * W;
                hist[in[row + clampi(x - r, 0, W - 1)]]--;
                hist[in[row + clampi(x + r + 1, 0, W - 1)]]++;
            }
        }
    }
}

static int in_logo(int i) { return i % W < LOGO_W && i / W < LOGO_H; }

typedef struct {
    uint8_t *text[PAGES];   /* per page: 1 where it carries text */
    float *lmed, *est_m, *menu_l, *ref, *est, *t0, *t1, *t2, *t3, *t4, *t5;
    uint8_t *med, *a, *b, *mask, *mask2, *bytes, *bytes2;
} Work;

static void work_free(Work *w)
{
    float **f[] = { &w->lmed, &w->est_m, &w->menu_l, &w->ref, &w->est, &w->t0, &w->t1, &w->t2, &w->t3, &w->t4, &w->t5 };
    uint8_t **b[] = { &w->med, &w->a, &w->b, &w->mask, &w->mask2, &w->bytes, &w->bytes2 };
    for (size_t i = 0; i < sizeof f / sizeof f[0]; i++) free(*f[i]), *f[i] = NULL;
    for (size_t i = 0; i < sizeof b / sizeof b[0]; i++) free(*b[i]), *b[i] = NULL;
    for (int p = 0; p < PAGES; p++) free(w->text[p]), w->text[p] = NULL;
}

static int work_alloc(Work *w)
{
    memset(w, 0, sizeof *w);
    float **f[] = { &w->lmed, &w->est_m, &w->menu_l, &w->ref, &w->est, &w->t0, &w->t1, &w->t2, &w->t3, &w->t4, &w->t5 };
    for (size_t i = 0; i < sizeof f / sizeof f[0]; i++)
        if ((*f[i] = malloc(sizeof(float) * N)) == NULL) return 0;
    w->med = malloc(N), w->mask = malloc(N), w->mask2 = malloc(N), w->bytes = malloc(N), w->bytes2 = malloc(N);
    w->a = malloc(3 * (size_t)N), w->b = malloc(3 * (size_t)N);
    int ok = w->med && w->mask && w->mask2 && w->bytes && w->bytes2 && w->a && w->b;
    for (int p = 0; p < PAGES; p++) ok = ok && (w->text[p] = malloc(N)) != NULL;
    return ok;
}

/* per pixel the page whose colour is nearest the others' (the medoid): text is at other places on other pages */
static void medoid(const uint8_t *const pages[PAGES], const uint8_t *pal, const float *pal_l, Work *w)
{
    for (int i = 0; i < N; i++) {
        int best = 0, best_d = 1 << 30;
        for (int p = 0; p < PAGES; p++) {
            const uint8_t *cp = &pal[3 * pages[p][i]];
            int d = 0;
            for (int q = 0; q < PAGES; q++) {
                const uint8_t *cq = &pal[3 * pages[q][i]];
                d += abs(cp[0] - cq[0]) + abs(cp[1] - cq[1]) + abs(cp[2] - cq[2]);
            }
            if (d < best_d) best_d = d, best = p;
        }
        w->med[i] = (uint8_t)best;
        w->lmed[i] = pal_l[pages[best][i]];
    }
}

/* each page's text: the palette entries the pages use and the relief alone hardly does (where all five agree is relief), grown over the
 * letters' soft edge and shadow */
static void find_text(const uint8_t *const pages[PAGES], Work *w)
{
    long relief[256] = { 0 }, all[256] = { 0 }, n_relief = 0, n_all = 0;
    for (int i = 0; i < N; i++) {
        int same = 1;
        for (int p = 1; p < PAGES && same; p++) same = pages[p][i] == pages[0][i];
        if (same) relief[pages[0][i]]++, n_relief++;
        for (int p = 0; p < PAGES; p++) all[pages[p][i]]++, n_all++;
    }
    uint8_t is_text[256];
    for (int k = 0; k < 256; k++)   /* relief share / all share < ratio, in integers */
        is_text[k] = all[k] > 0 && relief[k] * n_all * 100 < (long long)TEXT_RATIO_PERCENT * all[k] * n_relief;
    for (int p = 0; p < PAGES; p++) {
        for (int i = 0; i < N; i++) w->text[p][i] = is_text[pages[p][i]];
        grow(w->text[p], w->mask2, TEXT_GROW);
    }
}

/* per pixel, among the pages with no text there (all of them when every page has): the medoid where it is near the estimate, else the page
 * nearest it */
static void choose(const uint8_t *const pages[PAGES], const uint8_t *pal, const float *pal_l, const float *est, const Work *w, uint8_t *rgb)
{
    for (int i = 0; i < N; i++) {
        int clean = 0;
        for (int q = 0; q < PAGES; q++) clean += !w->text[q][i];
        int p = w->med[i];
        if (fabsf(w->lmed[i] - est[i]) >= KEEP || (clean > 0 && w->text[p][i])) {
            float best = 1e9f;
            for (int q = 0; q < PAGES; q++) {
                if (clean > 0 && w->text[q][i]) continue;
                const float d = fabsf(pal_l[pages[q][i]] - est[i]);
                if (d < best) best = d, p = q;
            }
        }
        memcpy(&rgb[3 * i], &pal[3 * pages[p][i]], 3);
    }
}

/* the configuration relief relit to the credits' light: a local straight-line fit of medoid on it, over the pixels all five pages share */
static void relight(const uint8_t *const pages[PAGES], Work *w)
{
    for (int i = 0; i < N; i++) {
        int same = i / W >= GOOD_FROM_ROW;
        for (int p = 1; p < PAGES && same; p++) same = pages[p][i] == pages[0][i];
        const float g = same ? 1.0f : 0.0f;
        w->t0[i] = g, w->t1[i] = g * w->lmed[i], w->t2[i] = g * w->menu_l[i];
        w->t3[i] = g * w->lmed[i] * w->menu_l[i], w->t4[i] = g * w->menu_l[i] * w->menu_l[i];
    }
    float *sums[] = { w->t0, w->t1, w->t2, w->t3, w->t4 };
    for (int k = 0; k < 5; k++) box_mean(sums[k], sums[k], w->t5, RELIGHT_R);
    for (int i = 0; i < N; i++) {
        const float sw = w->t0[i] + 1e-6f, ma = w->t1[i] / sw, mb = w->t2[i] / sw;
        const float gain = (w->t3[i] / sw - ma * mb) / (w->t4[i] / sw - mb * mb + 20.0f);
        w->ref[i] = gain * w->menu_l[i] + (ma - gain * mb);
    }
}

/* the pixels marked in mask, filled from their known neighbours a ring at a time */
static void fill(uint8_t *rgb, uint8_t *unknown, uint8_t *next)
{
    for (int pass = 0; pass < FILL_PASSES; pass++) {
        int left = 0;
        memcpy(next, unknown, N);
        for (int i = 0; i < N; i++) {
            if (!unknown[i]) continue;
            const int x = i % W, y = i / W;
            int s[3] = { 0, 0, 0 }, n = 0;
            const int nb[4] = { x > 0 ? i - 1 : -1, x < W - 1 ? i + 1 : -1, y > 0 ? i - W : -1, y < H - 1 ? i + W : -1 };
            for (int k = 0; k < 4; k++)
                if (nb[k] >= 0 && !unknown[nb[k]]) {
                    for (int c = 0; c < 3; c++) s[c] += rgb[3 * nb[k] + c];
                    n++;
                }
            if (n == 0) {
                left++;
                continue;
            }
            for (int c = 0; c < 3; c++) rgb[3 * i + c] = (uint8_t)((s[c] + n / 2) / n);
            next[i] = 0;
        }
        memcpy(unknown, next, N);
        if (left == 0) break;
    }
}

/* the nearest palette entry, among those not marked off */
static int nearest(const uint8_t *pal, const uint8_t *off, const uint8_t *c)
{
    int best = 0, best_d = 1 << 30;
    for (int k = 0; k < 256; k++) {
        if (off != NULL && off[k]) continue;
        const int dr = pal[3 * k] - c[0], dg = pal[3 * k + 1] - c[1], db = pal[3 * k + 2] - c[2], d = dr * dr + dg * dg + db * db;
        if (d < best_d) best_d = d, best = k;
    }
    return best;
}

/* the RGB picture into the credits' palette, its least used entries given to the text's colours */
static void to_palette(const uint8_t *rgb, const uint8_t page_pal[768], uint8_t *pixels, uint8_t palette[768])
{
    static int16_t cache[1 << 18];   /* 6 bits a channel */
    memset(cache, 0xff, sizeof cache);
    int count[256] = { 0 };
    for (int i = 0; i < N; i++) {
        const uint8_t *c = &rgb[3 * i];
        const int key = (c[0] >> 2) << 12 | (c[1] >> 2) << 6 | (c[2] >> 2);
        if (cache[key] < 0) {
            const uint8_t q[3] = { (uint8_t)((c[0] & 0xfc) | 2), (uint8_t)((c[1] & 0xfc) | 2), (uint8_t)((c[2] & 0xfc) | 2) };
            cache[key] = (int16_t)nearest(page_pal, NULL, q);
        }
        pixels[i] = (uint8_t)cache[key];
        count[pixels[i]]++;
    }
    uint8_t off[256] = { 0 };
    for (int t = 0; t < CREDITS_RELIEF_TEXT_COLOURS; t++) {
        int least = -1;
        for (int k = 255; k >= 0; k--)
            if (!off[k] && (least < 0 || count[k] < count[least])) least = k;
        off[least] = 1;
    }
    memcpy(palette, page_pal, 768);
    int remap[256];
    for (int k = 0; k < 256; k++) remap[k] = off[k] ? nearest(page_pal, off, &page_pal[3 * k]) : k;
    for (int i = 0; i < N; i++) pixels[i] = (uint8_t)remap[pixels[i]];
    for (int k = 0, t = 0; k < 256; k++)
        if (off[k]) memcpy(&palette[3 * k], CREDITS_RELIEF_TEXT_RGB[t++], 3);
}

int credits_relief_compose(const uint8_t *const pages[CREDITS_RELIEF_PAGES], const uint8_t page_pal[768], const uint8_t *menu_rgb,
                           uint8_t pixels[CREDITS_RELIEF_W * CREDITS_RELIEF_H], uint8_t palette[768])
{
    Work w;
    if (!work_alloc(&w)) {
        work_free(&w);
        return 0;
    }
    float pal_l[256];
    for (int k = 0; k < 256; k++) pal_l[k] = lum3(&page_pal[3 * k]);
    medoid(pages, page_pal, pal_l, &w);
    find_text(pages, &w);
    /* the smoothed medoid: an estimate free of thin strokes, everywhere */
    for (int i = 0; i < N; i++) w.bytes[i] = (uint8_t)clampi((int)w.lmed[i], 0, 255);
    median(w.bytes, w.bytes2, MEDIAN_R);
    for (int i = 0; i < N; i++) w.est_m[i] = w.bytes2[i];
    /* the first choice: against that estimate alone (kept inside the "&" below) */
    choose(pages, page_pal, pal_l, w.est_m, &w, w.a);
    /* the configuration relief, relit; and where its "&" stands */
    for (int i = 0; i < N; i++) w.menu_l[i] = lum3(&menu_rgb[3 * i]);
    relight(pages, &w);
    box_mean(w.menu_l, w.t0, w.t5, AMP_R);
    box_mean(w.est_m, w.t1, w.t5, AMP_R);
    for (int i = 0; i < N; i++) w.mask[i] = w.t0[i] - w.t1[i] > AMP_LEVEL;
    grow(w.mask, w.mask2, AMP_GROW);
    for (int i = 0; i < W * BANNER_ROWS; i++) w.mask[i] = 1;
    /* the second choice: against the relit relief where it can be trusted, its leftovers filled from their neighbours */
    for (int i = 0; i < N; i++) w.est[i] = w.mask[i] ? w.est_m[i] : w.ref[i];
    choose(pages, page_pal, pal_l, w.est, &w, w.b);
    for (int i = 0; i < N; i++) w.bytes[i] = fabsf(lum3(&w.b[3 * i]) - w.est[i]) > BAD;
    grow(w.bytes, w.mask2, 1);
    for (int i = 0; i < N; i++)
        if (in_logo(i)) w.bytes[i] = 0;
    fill(w.b, w.bytes, w.bytes2);
    /* the two blended over a soft edge round the "&" */
    for (int i = 0; i < N; i++) w.t0[i] = w.mask[i] ? 1.0f : 0.0f;
    for (int k = 0; k < 3; k++) box_mean(w.t0, w.t0, w.t5, BLEND_R);
    uint8_t *const out = w.b;
    for (int i = 0; i < N; i++) {
        const float a = w.t0[i];
        for (int c = 0; c < 3; c++) out[3 * i + c] = (uint8_t)clampi((int)lrintf(w.a[3 * i + c] * a + w.b[3 * i + c] * (1.0f - a)), 0, 255);
    }
    /* a shadow left by text: the first choice there, when it is nearer what the relief should be (where three pages carry text, the first
     * choice can be the text itself, brighter for the wrong reason) */
    for (int i = 0; i < N; i++) {
        const float lo = lum3(&out[3 * i]), la = lum3(&w.a[3 * i]);
        w.bytes[i] = lo < la - DARK && fabsf(la - w.est[i]) < fabsf(lo - w.est[i]);
    }
    grow(w.bytes, w.mask2, 2);
    for (int i = 0; i < N; i++)
        if (w.bytes[i] && !in_logo(i)) memcpy(&out[3 * i], &w.a[3 * i], 3);
    to_palette(out, page_pal, pixels, palette);
    work_free(&w);
    return 1;
}
