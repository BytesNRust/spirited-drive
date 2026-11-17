/* port (dep59, Lunar Springs' entry): see lunar_card.h. */
#include "game/frontend/lunar_card.h"

#include <stdlib.h>
#include <string.h>

enum {
    SHAPE_HEADER = 16, TYPE_BITMAP = 0x7b,
    SEARCH_ROWS = 40,         /* the label is in the card's bottom rows */
    ROW_SOLID_MIN = 3,        /* a label's row has at least this many letter texels; the picture's own reds are lone texels */
    WORD_GAP = 5,             /* columns without a letter that end a word (the letters of a word are at most 4 apart) */
    SHADOW_REACH = 3,         /* columns after a word's last letter its shadow reaches */
    TOP_REACH = 2, BOTTOM_REACH = 2,   /* rows round the label's that a letter or its shadow can reach */
    WORDS_MAX = 8,
    SIZE_MAX_SIDE = 2048      /* a card is 208 x 127; a header claiming more is not one (the entry's own length is not known here) */
};

typedef struct {
    const uint8_t *px;
    const uint8_t *pal;
    int w, h;
} Card;

typedef struct {
    int x0, x1;   /* first and last column with a letter, inclusive */
} Word;

typedef struct {
    int top, bottom;   /* the letters' first and last row */
    int words;
    Word word[WORDS_MAX];
    int ink, shadow;   /* the commonest texel of the letters and of the shadow */
} Label;

/* the red the labels are written in and the black of their shadow, as the screen's palette has them */
static int is_ink(const Card *c, int x, int y)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return 0;
    const uint8_t *p = c->pal + 3 * c->px[(size_t)y * (size_t)c->w + (size_t)x];
    return p[0] >= 150 && p[1] <= 48 && p[2] <= 48;
}

static int is_dark(const Card *c, int x, int y)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return 0;
    const uint8_t *p = c->pal + 3 * c->px[(size_t)y * (size_t)c->w + (size_t)x];
    return p[0] + p[1] + p[2] <= 24;
}

/* a letter's texel: red, with red beside or above or under it (a lone red texel is the picture's) */
static int is_letter(const Card *c, int x, int y)
{
    return is_ink(c, x, y) && (is_ink(c, x - 1, y) || is_ink(c, x + 1, y) || is_ink(c, x, y - 1) || is_ink(c, x, y + 1));
}

/* the shadow's texel: black with a letter's red to its left, above it or above-left */
static int is_shadow(const Card *c, int x, int y)
{
    return is_dark(c, x, y) && (is_ink(c, x - 1, y) || is_ink(c, x - 2, y) || is_ink(c, x, y - 1) || is_ink(c, x - 1, y - 1) || is_ink(c, x - 2, y - 1));
}

static int most_common(const Card *c, int y0, int y1, int shadow)
{
    int count[256] = { 0 };
    for (int y = y0; y <= y1 && y < c->h; y++)
        for (int x = 0; x < c->w; x++)
            if (shadow ? is_shadow(c, x, y) : is_letter(c, x, y)) count[c->px[(size_t)y * (size_t)c->w + (size_t)x]]++;
    int best = -1;
    for (int i = 0; i < 256; i++)
        if (count[i] > 0 && (best < 0 || count[i] > count[best])) best = i;
    return best;
}

/* the label: the run of rows in the bottom of the card with the most letter texels, its words, its colours; 0 when there is none */
static int find_label(const Card *c, Label *l)
{
    int row[SEARCH_ROWS] = { 0 };
    const int first = c->h > SEARCH_ROWS ? c->h - SEARCH_ROWS : 0;
    for (int y = first; y < c->h; y++)
        for (int x = 0; x < c->w; x++) row[y - first] += is_letter(c, x, y);
    int best = 0, best_top = -1, best_bottom = -1;
    for (int y = first; y < c->h;) {
        if (row[y - first] < ROW_SOLID_MIN) { y++; continue; }
        int sum = 0, end = y;
        for (; end < c->h && row[end - first] >= ROW_SOLID_MIN; end++) sum += row[end - first];
        if (sum > best) best = sum, best_top = y, best_bottom = end - 1;
        y = end;
    }
    if (best_top < 0) return 0;
    l->top = best_top, l->bottom = best_bottom, l->words = 0;
    int last = -1000;
    for (int x = 0; x < c->w; x++) {
        int letter = 0;
        for (int y = l->top; y <= l->bottom && !letter; y++) letter = is_letter(c, x, y);
        if (!letter) continue;
        if (x - last > WORD_GAP) {
            if (l->words == WORDS_MAX) return 0;
            l->word[l->words].x0 = x, l->words++;
        }
        l->word[l->words - 1].x1 = x, last = x;
    }
    l->ink = most_common(c, l->top, l->bottom, 0), l->shadow = most_common(c, l->top, l->bottom + 1, 1);
    return l->words > 0 && l->ink >= 0 && l->shadow >= 0;
}

static int is_bitmap(const uint8_t *s, int *w, int *h)
{
    if (s == NULL || s[0] != TYPE_BITMAP) return 0;
    *w = s[4] | s[5] << 8, *h = s[6] | s[7] << 8;
    return *w > 0 && *h > 0 && *w <= SIZE_MAX_SIDE && *h <= SIZE_MAX_SIDE;
}

uint8_t *lunar_card_named(const uint8_t *moon, const uint8_t *rusty, const uint8_t *pal)
{
    int mw, mh, rw, rh;
    if (pal == NULL || !is_bitmap(moon, &mw, &mh) || !is_bitmap(rusty, &rw, &rh) || mw != rw || mh != rh) return NULL;
    const Card m = { moon + SHAPE_HEADER, pal, mw, mh }, r = { rusty + SHAPE_HEADER, pal, rw, rh };
    Label ml, rl;
    if (!find_label(&m, &ml) || ml.words != 1 || !find_label(&r, &rl) || rl.words < 2) return NULL;   /* a moon card that names more is not the CD's */
    const Word name = rl.word[rl.words - 1];
    const int gap = name.x0 - rl.word[rl.words - 2].x1 - 1;   /* the two words' space on Rusty's card */
    const int to = ml.word[ml.words - 1].x1 + 1 + gap, dy = ml.bottom - rl.bottom;   /* the baselines: a capital's top can fall in a row of few texels */
    if (to + (name.x1 - name.x0) + SHADOW_REACH >= mw) return NULL;
    const size_t size = SHAPE_HEADER + (size_t)mw * (size_t)mh;
    uint8_t *out = malloc(size);
    if (out == NULL) return NULL;
    memcpy(out, moon, size);
    for (int y = rl.top - TOP_REACH; y <= rl.bottom + BOTTOM_REACH; y++) {
        if (y < 0 || y >= rh) continue;
        const int ty = y + dy;
        if (ty < 0 || ty >= mh) continue;
        for (int x = name.x0; x <= name.x1 + SHADOW_REACH; x++) {
            const int tx = to + x - name.x0;
            if (tx >= mw) continue;
            if (is_letter(&r, x, y)) out[SHAPE_HEADER + (size_t)ty * (size_t)mw + (size_t)tx] = (uint8_t)ml.ink;
            else if (is_shadow(&r, x, y)) out[SHAPE_HEADER + (size_t)ty * (size_t)mw + (size_t)tx] = (uint8_t)ml.shadow;
        }
    }
    return out;
}
