#include "game/race/car_repaint.h"
#include "game/race/car_mask.h"
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
    FIRST_RESERVED = 0xfa,      /* 0xfa..0xff: the police slots and the transparent index */
    TRANSPARENT = 0xff          /* the index a texture's cut-out texels have: not drawn, so not paint */
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
    uint8_t forced[ENTRIES];  /* the entries a masked texture shows under a texel its mask marks as paint: paint whatever their colour */
    int body_chromatic;
    float hue, spread_median, spread_max, luma_max;
    int any;
} Survey;

/* 1 when texel `i` of `tex` is in one of the rectangles left as they were (the plate, the black trim) */
static int kept(const RepaintTexture *tex, size_t i)
{
    if (tex->width <= 0) return 0;
    const int x = (int)(i % (size_t)tex->width), y = (int)(i / (size_t)tex->width);
    for (int k = 0; k < REPAINT_KEEPS; k++) {
        const int *r = tex->keep[k];
        if (r[2] > 0 && r[3] > 0 && x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3]) return 1;
    }
    return 0;
}

static void survey_rules(const RepaintBank *bank, int floor, Survey *s)
{
    memset(s, 0, sizeof *s);
    for (int t = 0; t < bank->texture_count; t++) {
        const RepaintTexture *tex = &bank->textures[t];
        uint32_t *into = tex->body == REPAINT_MAIN ? s->main : tex->body == REPAINT_END ? s->end : s->other;
        const int masked = tex->mask != NULL && tex->body != REPAINT_NONE;
        for (size_t i = 0; i < tex->count; i++) {
            if (tex->pixels[i] == TRANSPARENT) continue;
            const int keeps = masked ? tex->mask[i] != 0 : kept(tex, i);
            (keeps ? s->other : into)[tex->pixels[i]]++;
            if (masked && !keeps) s->forced[tex->pixels[i]] = 1;
        }
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

/* the paint family: the rules' (survey_rules), and every entry a mask paints, whatever its colour */
static void survey(const RepaintBank *bank, int floor, Survey *s)
{
    survey_rules(bank, floor, s);
    for (int e = 0; e < ENTRIES; e++)
        if (s->forced[e]) s->paint[e] = 1, s->any = 1;
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

/* a texel of a coloured body with at least 2 of the 4 texels beside it not paint (glass dithered with the body's own colour, as the ZR-1's hatch is) is left as it
 * was: painting its paint half only would speckle the glass */
enum { LAMP_SLOT = 0xfe, LAMP_REACH = 1, DITHER_NEIGHBOURS = 2 };   /* of the 4 beside it */

static int dithered(const RepaintTexture *tex, size_t i, const uint8_t *family, int chromatic)
{
    if (tex->width <= 0 || !chromatic) return 0;   /* a grey body's ramp has edges of its own everywhere */
    const int w = tex->width, x = (int)(i % (size_t)w), y = (int)(i / (size_t)w), h = (int)(tex->count / (size_t)w);
    static const int STEP[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
    int others = 0;
    for (int k = 0; k < 4; k++) {
        const int nx = x + STEP[k][0], ny = y + STEP[k][1];
        if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
        others += !family[tex->pixels[(size_t)ny * (size_t)w + (size_t)nx]];
    }
    return others >= DITHER_NEIGHBOURS;
}

/* a colour a body shows that is not paint but shines in the draft of a mask: the glass (a neutral darker than the floor) and a lamp (a very
 * saturated colour); the rest of what the rules leave (the plate, the trim, the badge, the vents) is matte */
enum { LAMP_SPREAD = 90 };

static int shines(const RepaintBank *bank, int entry, int floor)
{
    uint8_t c[3];
    colour_of(bank, entry, c);
    const int spread = spread_of(c);
    return (spread <= NEUTRAL_SPREAD && luma_of(c) < (float)floor) || spread >= LAMP_SPREAD;
}

/* Recolours the family (fn) and rewrites the body's texels to the colours. With `draft` set (one array of `count` bytes a texture) nothing is
 * changed: the texels it would not touch are set to 1, the others to 0, and the number of the others is returned. */
static int repaint(const RepaintBank *bank, int floor, ColourFn fn, void *ctx, const Survey *s, uint8_t *const *draft)
{
    int remap[ENTRIES], changed = 0;
    Spares *sp = calloc(1, sizeof *sp);
    if (sp == NULL) return 0;
    uint8_t family[ENTRIES], shared[ENTRIES];   /* the paint entries; the ones a dithered texel shows (not recoloured in place) */
    for (int e = 0; e < ENTRIES; e++) {
        remap[e] = -1;
        sp->used[e] = (uint8_t)(s->main[e] + s->end[e] + s->other[e] != 0 || e >= FIRST_RESERVED);
        family[e] = (uint8_t)(s->main[e] + s->end[e] != 0 && in_family(bank, s, e, floor));
        shared[e] = 0;
    }
    /* which texels are dithered, before any texel is rewritten (a rewritten neighbour is not paint any more) */
    uint8_t **guard = calloc((size_t)bank->texture_count + 1, sizeof *guard);
    if (guard == NULL) {
        free(sp);
        return 0;
    }
    for (int t = 0; t < bank->texture_count; t++) {
        const RepaintTexture *tex = &bank->textures[t];
        if (tex->body == REPAINT_NONE || tex->width <= 0 || tex->mask != NULL) continue;
        guard[t] = calloc(tex->count, 1);
        if (guard[t] == NULL) continue;
        const int w = tex->width, h = (int)(tex->count / (size_t)w);
        for (size_t i = 0; i < tex->count; i++) {
            if (tex->pixels[i] == LAMP_SLOT) {   /* a lamp's light slot: the lens and rim round it stay */
                const int x = (int)(i % (size_t)w), y = (int)(i / (size_t)w);
                for (int ny = y - LAMP_REACH; ny <= y + LAMP_REACH; ny++)
                    for (int nx = x - LAMP_REACH; nx <= x + LAMP_REACH; nx++)
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h) guard[t][(size_t)ny * (size_t)w + (size_t)nx] = 1;
            }
        }
        for (size_t i = 0; i < tex->count; i++) {
            if (!guard[t][i] && family[tex->pixels[i]] && !kept(tex, i) && dithered(tex, i, family, s->body_chromatic)) guard[t][i] = 1;
            if (guard[t][i] && family[tex->pixels[i]]) shared[tex->pixels[i]] = 1;
        }
    }
    for (int e = 0; e < ENTRIES; e++) {
        if (s->main[e] + s->end[e] == 0 || !in_family(bank, s, e, floor)) continue;
        if (draft != NULL) {
            remap[e] = e;
            continue;
        }
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
        if (s->other[e] == 0 && !shared[e]) {   /* only the body textures show it: recoloured where it is */
            set_colour(bank, e, out);
            if (second_valid(bank)) set_second(bank, e, second);
            remap[e] = e;
        } else {
            remap[e] = spare_for(bank, sp, out, second);
        }
    }
    for (int t = 0; t < bank->texture_count; t++) {
        const RepaintTexture *tex = &bank->textures[t];
        if (draft != NULL) memset(draft[t], REPAINT_DRAFT_MATTE, tex->count);
        if (tex->body == REPAINT_NONE) continue;
        for (size_t i = 0; i < tex->count; i++) {
            if (draft != NULL) {
                const int to = remap[tex->pixels[i]];
                if (tex->mask != NULL ? tex->mask[i] == 0 || tex->mask[i] == CAR_MASK_BLACK : !kept(tex, i) && !(guard[t] != NULL && guard[t][i]) && to >= 0) {
                    draft[t][i] = REPAINT_DRAFT_PAINT, changed++;
                } else if (tex->mask != NULL) {   /* the mask's class: glass and the lamp classes (240..248, emissive in low light) shine as glass, a body texel as the body, the rest is matte */
                    draft[t][i] = tex->mask[i] == CAR_MASK_SHINE || (tex->mask[i] >= CAR_MASK_BRAKE && tex->mask[i] <= CAR_MASK_REVERSE) ? REPAINT_DRAFT_SHINE
                                    : tex->mask[i] == CAR_MASK_BODY ? REPAINT_DRAFT_BODY
                                                                    : REPAINT_DRAFT_MATTE;
                } else if (!kept(tex, i)) {
                    draft[t][i] = (uint8_t)((guard[t] != NULL && guard[t][i]) || shines(bank, tex->pixels[i], floor) ? REPAINT_DRAFT_SHINE
                                                                                                                  : REPAINT_DRAFT_MATTE);
                }
                continue;
            }
            if (tex->mask != NULL ? tex->mask[i] != 0 : (kept(tex, i) || (guard[t] != NULL && guard[t][i]))) continue;
            const int to = remap[tex->pixels[i]];
            if (to >= 0 && to != tex->pixels[i]) tex->pixels[i] = (uint8_t)to, changed++;
            else if (to == tex->pixels[i]) changed++;
        }
    }
    for (int t = 0; t < bank->texture_count; t++) free(guard[t]);
    free(guard);
    free(sp);
    return changed;
}

typedef struct {
    CarPaintPlan plan;
} PaintCtx;

/* ----- the fixed black ----- */

/* The body's tones under the black plan, darkest first, each with the pixels the paint texels have of it. */
typedef struct {
    float luma;
    uint32_t weight;
    uint8_t out[3], second[3];
} BodyTone;

/* Each entry the fixed black texels use takes the black of the body's tone at the same rank: the fixed texels sorted by lightness, their place in that order (by
 * pixels) read off the body's tones in theirs. A fixed texel then has the distribution of tones a black body has, as a texel of the paint would; run through the
 * plan on its own colour it kept the darkness of the texture it is in (a frame's shadow) and came out flatter and darker than the body. */
static void fixed_black_remap(const RepaintBank *bank, const Survey *s, PaintCtx *ctx, int planned, const uint8_t black[3], int floor, Spares *sp, int remap[ENTRIES])
{
    BodyTone tone[ENTRIES];
    int tones = 0;
    uint64_t total_weight = 0;
    for (int e = 0; e < ENTRIES; e++) {
        remap[e] = -2;
        if (!planned || s->main[e] + s->end[e] == 0 || !in_family(bank, s, e, floor)) continue;
        BodyTone b = { 0, s->main[e] + s->end[e], { 0 }, { 0 } };
        uint8_t in[3];
        colour_of(bank, e, in);
        b.luma = luma_of(in);
        car_paint_shade(&ctx->plan, in, b.out);
        memcpy(b.second, b.out, 3);
        if (second_valid(bank)) {
            uint8_t own[3];
            second_colour_of(bank, e, own);
            car_paint_shade(&ctx->plan, own, b.second);
        }
        int k = tones++;
        while (k > 0 && tone[k - 1].luma > b.luma) tone[k] = tone[k - 1], k--;
        tone[k] = b;
        total_weight += b.weight;
    }
    uint32_t used[ENTRIES] = { 0 };
    uint64_t total_used = 0;
    for (int t = 0; t < bank->texture_count; t++) {
        const RepaintTexture *tex = &bank->textures[t];
        if (tex->body == REPAINT_NONE || tex->mask == NULL) continue;
        for (size_t i = 0; i < tex->count; i++)
            if (tex->mask[i] == CAR_MASK_BLACK && tex->pixels[i] != TRANSPARENT) used[tex->pixels[i]]++, total_used++;
    }
    int order[ENTRIES], n = 0;
    for (int e = 0; e < ENTRIES; e++) {
        if (used[e] == 0) continue;
        uint8_t c[3];
        colour_of(bank, e, c);
        int k = n++;
        while (k > 0) {
            uint8_t o[3];
            colour_of(bank, order[k - 1], o);
            if (luma_of(o) <= luma_of(c)) break;
            order[k] = order[k - 1], k--;
        }
        order[k] = e;
    }
    uint64_t before = 0;
    for (int k = 0; k < n; k++) {
        const int e = order[k];
        uint8_t out[3], second[3];
        if (tones == 0) {
            memcpy(out, black, 3), memcpy(second, black, 3);
        } else {
            /* the middle of this entry's share of the fixed texels, as a share of the body's pixels */
            const uint64_t at = (2 * before + used[e]) * total_weight / (2 * total_used);
            uint64_t sum = 0;
            int j = 0;
            while (j + 1 < tones && sum + tone[j].weight <= at) sum += tone[j].weight, j++;
            memcpy(out, tone[j].out, 3), memcpy(second, tone[j].second, 3);
        }
        before += used[e];
        remap[e] = spare_for(bank, sp, out, second);
    }
}

int car_repaint_fixed_black(const RepaintBank *bank, const uint8_t black[3], int floor)
{
    if (bank == NULL || (bank->type != PALETTE_8BIT && bank->type != PALETTE_6BIT) || bank->palette[0] == NULL) return -1;
    int wanted = 0;
    for (int t = 0; t < bank->texture_count && !wanted; t++) {
        const RepaintTexture *tex = &bank->textures[t];
        if (tex->body == REPAINT_NONE || tex->mask == NULL) continue;
        for (size_t i = 0; i < tex->count && !wanted; i++) wanted = tex->mask[i] == CAR_MASK_BLACK;
    }
    if (!wanted) return 0;
    Survey *s = malloc(sizeof *s);
    Spares *sp = calloc(1, sizeof *sp);
    PaintCtx *ctx = malloc(sizeof *ctx);
    if (s == NULL || sp == NULL || ctx == NULL) {
        free(s), free(sp), free(ctx);
        return 0;
    }
    survey(bank, floor, s);
    /* the black takes the same shades as a black the player chose: the plan of the family's colours (the paint texels') towards the black */
    uint8_t colours[ENTRIES][3];
    uint32_t weight[ENTRIES];
    int n = 0;
    for (int e = 0; e < ENTRIES; e++) {
        sp->used[e] = (uint8_t)(s->main[e] + s->end[e] + s->other[e] != 0 || e >= FIRST_RESERVED);
        if (s->any && s->main[e] + s->end[e] != 0 && in_family(bank, s, e, floor)) colour_of(bank, e, colours[n]), weight[n++] = s->main[e] + s->end[e];
    }
    if (n > 0) car_paint_plan(&ctx->plan, (const uint8_t (*)[3])colours, weight, n, black);
    int remap[ENTRIES], changed = 0;
    fixed_black_remap(bank, s, ctx, n > 0, black, floor, sp, remap);
    for (int t = 0; t < bank->texture_count; t++) {
        const RepaintTexture *tex = &bank->textures[t];
        if (tex->body == REPAINT_NONE || tex->mask == NULL) continue;
        for (size_t i = 0; i < tex->count; i++) {
            const int e = tex->pixels[i];
            if (tex->mask[i] != CAR_MASK_BLACK || e == TRANSPARENT) continue;
            if (remap[e] >= 0) tex->pixels[i] = (uint8_t)remap[e], changed++;
        }
    }
    free(s), free(sp), free(ctx);
    return changed;
}

/* ----- the catalogue paint ----- */

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
        changed = repaint(bank, floor, paint_colour, &ctx, s, NULL);
    }
    free(s);
    return changed;
}

/* ----- the PS1 livery ----- */

/* a colour darker than this fraction of the lowest pair's lightness is not mapped */
static const float BELOW_LOWEST = 0.75f;

typedef struct {
    const CarLiveryPair *pairs;
    int count;
    float soft;           /* 1: the pairs as they are; below: pulled towards `anchor` by this share of the distance kept */
    uint8_t anchor[3];    /* the colour the body's main tone maps to */
    const uint8_t *forced;   /* the entries a mask paints: mapped even when far darker than the darkest pair */
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
 * nearer pair's whole colour would stripe a ramp); one beyond the ends takes the end pair's (darker as it is when below, and left when well below) */
static int pairs_map(const PairsCtx *p, const uint8_t in[3], uint8_t out[3], int force)
{
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
        if (!force && y < BELOW_LOWEST * hi_y) return 0;   /* darker than any body tone: a slat or a shadow, left */
        const float f = y / hi_y;
        for (int i = 0; i < 3; i++) out[i] = (uint8_t)((float)b[i] * f + 0.5f);
    }
    return 1;
}

static int pairs_colour(void *ctx, int entry, const uint8_t in[3], uint8_t out[3])
{
    const PairsCtx *p = ctx;
    if (!pairs_map(p, in, out, p->forced != NULL && p->forced[entry])) return 0;
    if (p->soft < 1.0f)
        for (int i = 0; i < 3; i++) {
            const float v = (float)p->anchor[i] + ((float)out[i] - (float)p->anchor[i]) * p->soft + 0.5f;
            out[i] = (uint8_t)(v < 0.0f ? 0.0f : v > 255.0f ? 255.0f : v);
        }
    return 1;
}

/* a near-black body (the paints' rank case) keeps only part of the contrast between its tones' colours */
static const float DARK_SOFT = 0.6f;

static void soften(const RepaintBank *bank, const Survey *s, int floor, PairsCtx *ctx)
{
    uint8_t colours[ENTRIES][3];
    uint32_t weight[ENTRIES];
    int n = 0;
    for (int e = 0; e < ENTRIES; e++)
        if (s->main[e] + s->end[e] != 0 && in_family(bank, s, e, floor)) {
            colour_of(bank, e, colours[n]);
            weight[n++] = s->main[e] + s->end[e];
        }
    if (n == 0) return;
    const uint8_t none[3] = { 0, 0, 0 };
    CarPaintPlan plan;
    car_paint_plan(&plan, (const uint8_t (*)[3])colours, weight, n, none);
    if (!plan.ranked) return;
    int nearest = 0;
    for (int i = 1; i < n; i++)
        if (fabsf(luma_of(colours[i]) - plan.main) < fabsf(luma_of(colours[nearest]) - plan.main)) nearest = i;
    if (pairs_map(ctx, colours[nearest], ctx->anchor, 0)) ctx->soft = DARK_SOFT;
}

int car_repaint_pairs(const RepaintBank *bank, const CarLiveryPair *pairs, int count, int floor)
{
    if (bank == NULL || (bank->type != PALETTE_8BIT && bank->type != PALETTE_6BIT) || bank->palette[0] == NULL) return -1;
    Survey *s = malloc(sizeof *s);
    if (s == NULL) return 0;
    survey(bank, floor, s);
    int changed = 0;
    if (s->any && count > 0) {
        PairsCtx ctx = { pairs, count, 1.0f, { 0, 0, 0 }, s->forced };
        soften(bank, s, floor, &ctx);
        changed = repaint(bank, floor, pairs_colour, &ctx, s, NULL);
    }
    free(s);
    return changed;
}

/* ----- the body textures ----- */

static int named(const char name[4], const char *const names[], size_t count)
{
    for (size_t k = 0; k < count; k++)
        if (memcmp(name, names[k], 4) == 0) return 1;
    return 0;
}

int car_repaint_body_class(const char name[4], int level)
{
    /* (the 512 TR has no topv: its top is J000 and J001) */
    static const char *const MAIN[] = { "topv", "rsid", "bott", "wing", "J000", "J001" }, *const END[] = { "frnt", "rear" };
    /* the low detail bank's side and back: the sides and the rear, as the high bank's rsid and rear are (no high texture has these names) */
    static const char *const LOW_MAIN[] = { "side" }, *const LOW_END[] = { "back" };
    if (named(name, MAIN, sizeof MAIN / sizeof *MAIN)) return REPAINT_MAIN;
    if (level != 0 && named(name, LOW_MAIN, sizeof LOW_MAIN / sizeof *LOW_MAIN)) return REPAINT_MAIN;
    if (named(name, END, sizeof END / sizeof *END)) return REPAINT_END;
    if (level != 0 && named(name, LOW_END, sizeof LOW_END / sizeof *LOW_END)) return REPAINT_END;
    return REPAINT_NONE;
}

/* ----- the drafts of the hand-made masks ----- */

int car_repaint_draft(const RepaintBank *bank, int floor, uint8_t *const keep[])
{
    if (bank == NULL || (bank->type != PALETTE_8BIT && bank->type != PALETTE_6BIT) || bank->palette[0] == NULL) return -1;
    Survey *s = malloc(sizeof *s);
    if (s == NULL) return 0;
    survey(bank, floor, s);
    int paint = 0;
    if (s->any) {
        paint = repaint(bank, floor, NULL, NULL, s, keep);
    } else {
        for (int t = 0; t < bank->texture_count; t++) memset(keep[t], REPAINT_DRAFT_MATTE, bank->textures[t].count);
    }
    free(s);
    return paint;
}
