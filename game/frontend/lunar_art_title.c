/* port (dep59, Lunar Springs' entry, L4): the title and the time of day's line on the grey picture (lunar_art.h): "RUSTY" filled in
 * from its box's border with the texture's grain, "LUNAR" pasted from the CD's own letters as their difference from their own filled-in
 * box (a letter's white and its shadow; the texture under it left behind), the time of day's line covered by the rows under it. */
#include "game/frontend/lunar_art.h"
#include "game/frontend/lunar_art_private.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    FILL_PASSES = 400,
    WORD_LEFT = 7, WORD_RIGHT = 10,   /* the word's box: from its first letter's left, to the next word's first letter */
    GLYPH_LEFT = 3, GLYPH_PAD = 8,    /* a letter's box: a little before it, its shadow after it (up to the next letter) */
    GLYPH_EDGE = 14, GLYPH_FADE = 4,  /* a difference under GLYPH_EDGE is the texture's; the shadow fades out over the last columns */
    GAP_CAPITAL = 7, GAP_SMALL = 4,   /* the space after a capital and after a small letter, as the title's */
    GRAIN_X0 = 200, GRAIN_X1 = 600, GRAIN_Y0 = 2, GRAIN_Y1 = 12,   /* a strip of the title's texture with no letter in it */
    /* the letters' places among the title's runs: RUSTY SPRINGS RACEWAY, and Lost Vegas' L */
    RUN_U = 1, RUN_R = 7, RUN_N = 9, RUN_A = 13, RUN_SPRINGS = 5, RUNS_NEEDED = 14,
    LABEL_Y0 = 384, LABEL_Y1 = 445, LABEL_X1 = 310, LABEL_WHITE = 190, LABEL_ABOVE = 2, LABEL_RIGHT = 14,
    SHADOW_DARK = 25, SHADOW_PIXELS = 3, SHADOW_MAX = 14, GROUND_BELOW = 20, LABEL_GAP = 3   /* the shadow: darker than the ground under it */
};

int lunar_title_runs(const uint8_t *grey, int w, int h, LunarRun *runs, int max)
{
    int n = 0, in = 0;
    for (int x = 0; x < w && n < max; x++) {
        int white = 0;
        for (int y = LUNAR_TITLE_Y0; y < LUNAR_TITLE_Y1 && y < h && !white; y++) white = grey[y * w + x] > LUNAR_LETTER_WHITE;
        if (white && !in) runs[n].x0 = x, in = 1;
        if (!white && in) runs[n++].x1 = x - 1, in = 0;
    }
    if (in && n < max) runs[n++].x1 = w - 1;
    return n;
}

void lunar_fill(uint8_t *grey, int w, int x0, int y0, int x1, int y1, int passes)
{
    const int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
    if (bw < 3 || bh < 3) return;
    float *f = malloc(sizeof *f * (size_t)bw * (size_t)bh);
    if (f == NULL) return;
    for (int j = 0; j < bh; j++)
        for (int i = 0; i < bw; i++) {   /* a start: each row between its two ends; the border as it is */
            const float l = grey[(y0 + j) * w + x0], r = grey[(y0 + j) * w + x1], t = (float)i / (float)(bw - 1);
            const int edge = i == 0 || j == 0 || i == bw - 1 || j == bh - 1;
            f[j * bw + i] = edge ? grey[(y0 + j) * w + x0 + i] : l + (r - l) * t;
        }
    for (int p = 0; p < passes; p++)
        for (int i = 1; i < bw - 1; i++)
            for (int j = 1; j < bh - 1; j++) f[j * bw + i] = (f[j * bw + i - 1] + f[j * bw + i + 1] + f[(j - 1) * bw + i] + f[(j + 1) * bw + i]) / 4;
    for (int j = 1; j < bh - 1; j++)
        for (int i = 1; i < bw - 1; i++) grey[(y0 + j) * w + x0 + i] = (uint8_t)(f[j * bw + i] + 0.5f);
    free(f);
}

/* the texture's grain: the spread of neighbouring pixels' differences in a strip with no letter, as one pixel's */
static double grain_of(const uint8_t *g)
{
    double sum = 0, sq = 0;
    int n = 0;
    for (int y = GRAIN_Y0; y < GRAIN_Y1; y++)
        for (int x = GRAIN_X0 + 1; x < GRAIN_X1; x++) {
            const double d = (double)g[y * LUNAR_ART_W + x] - g[y * LUNAR_ART_W + x - 1];
            sum += d, sq += d * d, n++;
        }
    const double mean = sum / n;
    return sqrt(sq / n - mean * mean) / 1.4142135623730951;   /* one pixel's, from a difference of two */
}

/* a fixed sequence (the same picture every run, for the cache), normal by Box-Muller */
static double normal(uint32_t *seed)
{
    *seed = *seed * 1664525u + 1013904223u;
    const double u1 = ((*seed >> 8) + 1.0) / 16777217.0;
    *seed = *seed * 1664525u + 1013904223u;
    const double u2 = (*seed >> 8) / 16777216.0;
    return sqrt(-2.0 * log(u1)) * cos(6.283185307179586 * u2);
}

static uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

/* a letter of `g` (the run `r`, its box up to `next` columns on): its difference from its own box filled in */
static int glyph_cut(const uint8_t *g, LunarRun r, int next, Glyph *out)
{
    const int pad = next - r.x1 - 1 < GLYPH_PAD ? next - r.x1 - 1 : GLYPH_PAD;
    out->w = r.x1 + pad - (r.x0 - GLYPH_LEFT) + 1, out->h = LUNAR_TITLE_Y1 - LUNAR_TITLE_Y0, out->width = r.x1 - r.x0;
    if (pad < 0 || r.x0 < GLYPH_LEFT || out->w > GLYPH_MAX_W) return 0;
    uint8_t box[GLYPH_MAX_W * (LUNAR_TITLE_Y1 - LUNAR_TITLE_Y0)];
    for (int j = 0; j < out->h; j++) memcpy(box + j * out->w, g + (LUNAR_TITLE_Y0 + j) * LUNAR_ART_W + r.x0 - GLYPH_LEFT, (size_t)out->w);
    lunar_fill(box, out->w, 0, 0, out->w - 1, out->h - 1, FILL_PASSES);
    for (int j = 0; j < out->h; j++)
        for (int i = 0; i < out->w; i++) {
            int d = g[(LUNAR_TITLE_Y0 + j) * LUNAR_ART_W + r.x0 - GLYPH_LEFT + i] - box[j * out->w + i];
            if (abs(d) <= GLYPH_EDGE) d = 0;
            const int k = out->w - 1 - i;   /* the shadow's last columns, faded */
            if (d < 0 && k < GLYPH_FADE) d = d * k / GLYPH_FADE;
            out->delta[j * out->w + i] = (int16_t)d;
        }
    return 1;
}

static void glyph_put(uint8_t *g, const Glyph *gl, int x)
{
    for (int j = 0; j < gl->h; j++)
        for (int i = 0; i < gl->w; i++) {
            uint8_t *p = g + (LUNAR_TITLE_Y0 + j) * LUNAR_ART_W + x + i;
            if (gl->delta[j * gl->w + i] != 0 && x + i < LUNAR_ART_W) *p = clamp8(*p + gl->delta[j * gl->w + i]);
        }
}

int lunar_title(uint8_t *g, const uint8_t *lost, char *why, size_t why_size)
{
    LunarRun rr[32], lr[4];
    const int rn = lunar_title_runs(g, LUNAR_ART_W, LUNAR_ART_H, rr, 32), ln = lunar_title_runs(lost, LUNAR_ART_W, LUNAR_ART_H, lr, 4);
    if (rn < RUNS_NEEDED || ln < 2) return lunar_why(why, why_size, "the title's letters are not found (%d and %d)", rn, ln), 0;
    static Glyph letters[5];
    const int ok = glyph_cut(lost, lr[0], lr[1].x0, &letters[0]) && glyph_cut(g, rr[RUN_U], rr[RUN_U + 1].x0, &letters[1])
        && glyph_cut(g, rr[RUN_N], rr[RUN_N + 1].x0, &letters[2]) && glyph_cut(g, rr[RUN_A], rr[RUN_A + 1].x0, &letters[3])
        && glyph_cut(g, rr[RUN_R], rr[RUN_R + 1].x0, &letters[4]);
    if (!ok) return lunar_why(why, why_size, "a letter of the title does not fit its box"), 0;
    const int x0 = rr[0].x0 - WORD_LEFT, x1 = rr[RUN_SPRINGS].x0 - WORD_RIGHT;
    const double grain = grain_of(g);
    lunar_fill(g, LUNAR_ART_W, x0, LUNAR_TITLE_Y0, x1, LUNAR_TITLE_Y1, FILL_PASSES);
    uint32_t seed = 7;
    for (int y = LUNAR_TITLE_Y0 + 1; y < LUNAR_TITLE_Y1; y++)
        for (int x = x0 + 1; x < x1; x++) g[y * LUNAR_ART_W + x] = clamp8((int)lround(g[y * LUNAR_ART_W + x] + normal(&seed) * grain));
    int x = rr[0].x0 - GLYPH_LEFT;
    for (int i = 0; i < 5; i++) glyph_put(g, &letters[i], x), x += letters[i].width + (i == 0 ? GAP_CAPITAL : GAP_SMALL);
    return 1;
}

void lunar_drop_time_line(uint8_t *g)
{
    int first = -1, last = -1, right = 0;
    for (int y = LABEL_Y0; y <= LABEL_Y1; y++)
        for (int x = 0; x < LABEL_X1; x++)
            if (g[y * LUNAR_ART_W + x] > LABEL_WHITE) {
                if (first < 0) first = y;
                last = y, right = x > right ? x : right;
            }
    if (first < 0 || last + GROUND_BELOW >= LUNAR_ART_H) return;
    const int w = right + LABEL_RIGHT < LUNAR_ART_W ? right + LABEL_RIGHT : LUNAR_ART_W;
    int ground = 0;   /* the ground's grey, a row under the line */
    for (int x = 0; x < w; x++) ground += g[(last + GROUND_BELOW) * LUNAR_ART_W + x];
    ground /= w;
    int end = last;   /* the shadow's last row: the German line's goes lower than the English one's */
    for (int y = last + 1; y <= last + SHADOW_MAX; y++) {
        int dark = 0;
        for (int x = 0; x < w; x++) dark += g[y * LUNAR_ART_W + x] + SHADOW_DARK < ground;
        if (dark >= SHADOW_PIXELS) end = y;
    }
    const int y0 = first - LABEL_ABOVE, rows = end - y0 + 1, from = end + LABEL_GAP;
    if (from + rows > LUNAR_ART_H || w > LUNAR_ART_W) return;
    for (int j = 0; j < rows; j++) memmove(g + (y0 + j) * LUNAR_ART_W, g + (from + j) * LUNAR_ART_W, (size_t)w);
}
