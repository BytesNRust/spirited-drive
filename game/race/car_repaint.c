#include "game/race/car_repaint.h"
#include "game/race/car_paint.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    ENTRIES = 256, PALETTE_8BIT = 0x24, PALETTE_6BIT = 0x22,
    CHROMATIC_SPREAD = 16,      /* an entry whose channels spread this much (8 bits) is coloured, whatever its lightness */
    NEUTRAL_SPREAD = 24,        /* a grey body's family: spread up to this */
    MIN_MAIN_PIXELS = 8,        /* an entry the main textures show at least this often... */
    HUE_WINDOW = 30,
    FIRST_RESERVED = 0xfa       /* 0xfa..0xff: the police slots and the transparent index */
};

static int spread_of(const uint8_t c[3])
{
    const int hi = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]);
    const int lo = c[0] < c[1] ? (c[0] < c[2] ? c[0] : c[2]) : (c[1] < c[2] ? c[1] : c[2]);
    return hi - lo;
}

static float luma_of(const uint8_t c[3]) { return 0.30f * c[0] + 0.59f * c[1] + 0.11f * c[2]; }

static float hue_of(const uint8_t c[3])
{
    return atan2f(1.7320508f * (float)(c[1] - c[2]), (float)(2 * c[0] - c[1] - c[2])) * 57.29578f;
}

static float hue_apart(float a, float b)
{
    float d = fabsf(a - b);
    return d > 180.0f ? 360.0f - d : d;
}

/* the bank's palette entry `e` as 8-bit colour */
static void colour_of(const RepaintBank *bank, int e, uint8_t out[3])
{
    const int shift = bank->type == PALETTE_6BIT ? 2 : 0;
    for (int c = 0; c < 3; c++) out[c] = (uint8_t)(bank->palette[0][3 * e + c] << shift);
}

static int second_valid(const RepaintBank *bank)
{
    return bank->palettes > 1 && bank->palette[1] != NULL && (bank->type2 == PALETTE_8BIT || bank->type2 == PALETTE_6BIT);
}

static void second_colour_of(const RepaintBank *bank, int e, uint8_t out[3])
{
    const int shift = bank->type2 == PALETTE_6BIT ? 2 : 0;
    for (int c = 0; c < 3; c++) out[c] = (uint8_t)(bank->palette[1][3 * e + c] << shift);
}

static void set_colour(const RepaintBank *bank, int e, const uint8_t rgb[3])
{
    const int shift = bank->type == PALETTE_6BIT ? 2 : 0;
    for (int c = 0; c < 3; c++) bank->palette[0][3 * e + c] = (uint8_t)(rgb[c] >> shift);
}

static void set_second(const RepaintBank *bank, int e, const uint8_t rgb[3])
{
    const int shift = bank->type2 == PALETTE_6BIT ? 2 : 0;
    for (int c = 0; c < 3; c++) bank->palette[1][3 * e + c] = (uint8_t)(rgb[c] >> shift);
}

/* what the textures say about each entry, and the paint family they make */
typedef struct {
    uint32_t main[ENTRIES], end[ENTRIES], other[ENTRIES];
    uint8_t paint[ENTRIES];   /* the entries the main textures show heavily and that are paint: the family's core */
    int body_chromatic;
    float hue, spread_median, spread_max, luma_max;
    int any;
} Survey;

/* 1 when texel `i` of `tex` is in the rectangle left as it was (the plate) */
static int kept(const RepaintTexture *tex, size_t i)
{
    if (tex->keep[2] <= 0 || tex->keep[3] <= 0 || tex->width <= 0) return 0;
    const int x = (int)(i % (size_t)tex->width), y = (int)(i / (size_t)tex->width);
    return x >= tex->keep[0] && x < tex->keep[0] + tex->keep[2] && y >= tex->keep[1] && y < tex->keep[1] + tex->keep[3];
}

static void survey(const RepaintBank *bank, int floor, Survey *s)
{
    memset(s, 0, sizeof *s);
    for (int t = 0; t < bank->texture_count; t++) {
        const RepaintTexture *tex = &bank->textures[t];
        uint32_t *into = tex->body == REPAINT_MAIN ? s->main : tex->body == REPAINT_END ? s->end : s->other;
        for (size_t i = 0; i < tex->count; i++) (kept(tex, i) ? s->other : into)[tex->pixels[i]]++;
    }
    /* the entries the main textures show heavily and that are paint, not glass or shadow: coloured, or a grey lighter than the floor */
    uint8_t cand[ENTRIES] = { 0 };
    uint64_t colourful = 0, grey = 0;
    for (int e = 0; e < ENTRIES; e++) {
        uint8_t c[3];
        colour_of(bank, e, c);
        const uint32_t body = s->main[e] + s->end[e];
        if (s->main[e] < MIN_MAIN_PIXELS || s->main[e] * 4 < body) continue;
        if (spread_of(c) >= CHROMATIC_SPREAD) cand[e] = 1, colourful += s->main[e];
        else if (luma_of(c) >= (float)floor) cand[e] = 1, grey += s->main[e];
    }
    /* a coloured body when most of the paint's pixels are coloured: its greys (the underside, the shadows) are then not its paint */
    const int coloured = colourful > grey;
    uint64_t total = 0;
    float sx = 0, sy = 0;
    for (int e = 0; e < ENTRIES; e++) {
        uint8_t c[3];
        colour_of(bank, e, c);
        if (!cand[e] || (coloured && spread_of(c) < CHROMATIC_SPREAD)) continue;
        s->paint[e] = 1, s->any = 1;
        total += s->main[e];
        const float spread = (float)spread_of(c);
        if (spread > s->spread_max) s->spread_max = spread;
        if (luma_of(c) > s->luma_max) s->luma_max = luma_of(c);
        if (spread_of(c) >= CHROMATIC_SPREAD) {
            const float h = hue_of(c) * 0.01745329f;
            sx += cosf(h) * (float)s->main[e], sy += sinf(h) * (float)s->main[e];
        }
    }
    if (!s->any) return;
    /* the median spread, by pixels: sorted by spread (the entries are few) */
    int order[ENTRIES], n = 0;
    for (int e = 0; e < ENTRIES; e++) {
        if (!s->paint[e]) continue;
        uint8_t c[3];
        colour_of(bank, e, c);
        int k = n++;
        while (k > 0) {
            uint8_t o[3];
            colour_of(bank, order[k - 1], o);
            if (spread_of(o) <= spread_of(c)) break;
            order[k] = order[k - 1], k--;
        }
        order[k] = e;
    }
    uint64_t before = 0;
    for (int k = 0; k < n; k++) {
        uint8_t c[3];
        colour_of(bank, order[k], c);
        before += s->main[order[k]];
        s->spread_median = (float)spread_of(c);
        if (before * 2 >= total) break;
    }
    s->body_chromatic = coloured;
    s->hue = atan2f(sy, sx) * 57.29578f;
}

/* 1 when entry `e`'s colour is in the paint family: the core's, or a texel of the end textures or an entry of the main ones too
 * thin to be the core that looks like it */
static int in_family(const RepaintBank *bank, const Survey *s, int e, int floor)
{
    if (s->paint[e]) return 1;
    uint8_t c[3];
    colour_of(bank, e, c);
    const float spread = (float)spread_of(c);
    if (s->body_chromatic)
        return spread >= 0.35f * s->spread_median && spread <= 1.3f * s->spread_max && hue_apart(hue_of(c), s->hue) <= HUE_WINDOW &&
               luma_of(c) <= s->luma_max * 1.08f + 4.0f;   /* a lamp is brighter than the paint's lightest tone */
    return spread <= NEUTRAL_SPREAD && luma_of(c) >= (float)floor && luma_of(c) <= s->luma_max * 1.1f + 8.0f;
}

typedef int (*ColourFn)(void *ctx, int entry, const uint8_t in[3], uint8_t out[3]);

/* the spare entries and the new colours given so far */
typedef struct {
    uint8_t used[ENTRIES];     /* an entry some texture of the bank uses, or reserved */
    uint8_t given[ENTRIES];    /* the spare entries that carry a new colour */
    uint8_t rgb[ENTRIES][3];
    int count;
} Spares;

static int spare_for(const RepaintBank *bank, Spares *sp, const uint8_t want[3], const uint8_t second[3])
{
    for (int i = 0; i < ENTRIES; i++)
        if (sp->given[i] && memcmp(sp->rgb[i], want, 3) == 0) return i;
    for (int i = 0; i < FIRST_RESERVED; i++)
        if (!sp->used[i] && !sp->given[i]) {
            sp->given[i] = 1, sp->count++;
            memcpy(sp->rgb[i], want, 3);
            set_colour(bank, i, want);
            if (second_valid(bank)) set_second(bank, i, second);
            return i;
        }
    int best = -1, best_d = 1 << 30;
    for (int i = 0; i < ENTRIES; i++) {
        if (!sp->given[i]) continue;
        const int d = (sp->rgb[i][0] - want[0]) * (sp->rgb[i][0] - want[0]) + (sp->rgb[i][1] - want[1]) * (sp->rgb[i][1] - want[1]) +
                      (sp->rgb[i][2] - want[2]) * (sp->rgb[i][2] - want[2]);
        if (d < best_d) best = i, best_d = d;
    }
    return best;
}

static int repaint(const RepaintBank *bank, int floor, ColourFn fn, void *ctx, const Survey *s)
{
    int remap[ENTRIES], changed = 0;
    Spares *sp = calloc(1, sizeof *sp);
    if (sp == NULL) return 0;
    for (int e = 0; e < ENTRIES; e++) {
        remap[e] = -1;
        sp->used[e] = (uint8_t)(s->main[e] + s->end[e] + s->other[e] != 0 || e >= FIRST_RESERVED);
    }
    for (int e = 0; e < ENTRIES; e++) {
        if (s->main[e] + s->end[e] == 0 || !in_family(bank, s, e, floor)) continue;
        uint8_t in[3], out[3];
        colour_of(bank, e, in);
        if (!fn(ctx, e, in, out)) continue;
        uint8_t second[3];
        memcpy(second, out, 3);
        if (second_valid(bank)) {   /* the car's own colours at that entry, by the same rule */
            uint8_t own[3];
            second_colour_of(bank, e, own);
            if (!fn(ctx, e, own, second)) memcpy(second, own, 3);
        }
        if (s->other[e] == 0) {   /* only the body textures show it: recoloured where it is */
            set_colour(bank, e, out);
            if (second_valid(bank)) set_second(bank, e, second);
            remap[e] = e;
        } else {
            remap[e] = spare_for(bank, sp, out, second);
        }
    }
    for (int t = 0; t < bank->texture_count; t++) {
        const RepaintTexture *tex = &bank->textures[t];
        if (tex->body == REPAINT_NONE) continue;
        for (size_t i = 0; i < tex->count; i++) {
            if (kept(tex, i)) continue;
            const int to = remap[tex->pixels[i]];
            if (to >= 0 && to != tex->pixels[i]) tex->pixels[i] = (uint8_t)to, changed++;
            else if (to == tex->pixels[i]) changed++;
        }
    }
    free(sp);
    return changed;
}

/* ----- the catalogue paint ----- */

typedef struct {
    CarPaintPlan plan;
} PaintCtx;

static int paint_colour(void *ctx, int entry, const uint8_t in[3], uint8_t out[3])
{
    (void)entry;
    car_paint_shade(&((PaintCtx *)ctx)->plan, in, out);
    return 1;
}

int car_repaint_paint(const RepaintBank *bank, const uint8_t rgb[3], int floor)
{
    if (bank == NULL || (bank->type != PALETTE_8BIT && bank->type != PALETTE_6BIT) || bank->palette[0] == NULL) return -1;
    Survey *s = malloc(sizeof *s);
    if (s == NULL) return 0;
    survey(bank, floor, s);
    int changed = 0;
    if (s->any) {
        uint8_t colours[ENTRIES][3];
        uint32_t weight[ENTRIES];
        int n = 0;
        for (int e = 0; e < ENTRIES; e++)
            if (s->main[e] + s->end[e] != 0 && in_family(bank, s, e, floor)) {
                colour_of(bank, e, colours[n]);
                weight[n++] = s->main[e] + s->end[e];
            }
        PaintCtx ctx;
        car_paint_plan(&ctx.plan, (const uint8_t (*)[3])colours, weight, n, rgb);
        changed = repaint(bank, floor, paint_colour, &ctx, s);
    }
    free(s);
    return changed;
}

/* ----- the PS1 livery ----- */

typedef struct {
    const CarLiveryPair *pairs;
    int count;
} PairsCtx;

static uint16_t colour5(const uint8_t c[3])
{
    return (uint16_t)((c[0] >> 3) | (c[1] >> 3) << 5 | (c[2] >> 3) << 10);
}

static void colour8(uint16_t c, uint8_t out[3])
{
    out[0] = (uint8_t)((c & 31) << 3), out[1] = (uint8_t)(((c >> 5) & 31) << 3), out[2] = (uint8_t)(((c >> 10) & 31) << 3);
}

/* a texel with a pair of its own takes its colour; one between two pairs takes the colour between theirs, by lightness (the
 * nearer pair's whole colour would stripe a ramp); one beyond the ends takes the end pair's, darker as it is, if below */
static int pairs_colour(void *ctx, int entry, const uint8_t in[3], uint8_t out[3])
{
    const PairsCtx *p = ctx;
    (void)entry;
    const uint16_t c = colour5(in);
    const float y = luma_of(in);
    int lo = -1, hi = -1;
    float lo_y = 0, hi_y = 0;
    for (int k = 0; k < p->count; k++) {
        if (p->pairs[k].from == c) {
            colour8(p->pairs[k].to, out);
            return 1;
        }
        uint8_t from[3];
        colour8(p->pairs[k].from, from);
        const float fy = luma_of(from);
        if (fy <= y && (lo < 0 || fy > lo_y)) lo = k, lo_y = fy;
        if (fy > y && (hi < 0 || fy < hi_y)) hi = k, hi_y = fy;
    }
    if (lo < 0 && hi < 0) return 0;
    uint8_t a[3], b[3];
    if (lo >= 0) colour8(p->pairs[lo].to, a);
    if (hi >= 0) colour8(p->pairs[hi].to, b);
    if (lo >= 0 && hi >= 0) {
        const float f = (y - lo_y) / (hi_y - lo_y);
        for (int i = 0; i < 3; i++) out[i] = (uint8_t)((float)a[i] + ((float)b[i] - (float)a[i]) * f + 0.5f);
    } else if (lo >= 0) {
        memcpy(out, a, 3);
    } else {
        const float f = y / hi_y;
        for (int i = 0; i < 3; i++) out[i] = (uint8_t)((float)b[i] * f + 0.5f);
    }
    return 1;
}

int car_repaint_pairs(const RepaintBank *bank, const CarLiveryPair *pairs, int count, int floor)
{
    if (bank == NULL || (bank->type != PALETTE_8BIT && bank->type != PALETTE_6BIT) || bank->palette[0] == NULL) return -1;
    Survey *s = malloc(sizeof *s);
    if (s == NULL) return 0;
    survey(bank, floor, s);
    int changed = 0;
    if (s->any && count > 0) {
        PairsCtx ctx = { pairs, count };
        changed = repaint(bank, floor, pairs_colour, &ctx, s);
    }
    free(s);
    return changed;
}
