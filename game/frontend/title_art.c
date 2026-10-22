/* port: the port's screen titles in the original titles' letters (title_art.h). */
#include "game/frontend/title_art.h"

#include "formats/fsh.h"

#include <stdlib.h>
#include <string.h>

/* the screens the letters are cut from, and their titles' colour (the most common strongly coloured pixel there) */
static const struct {
    const char *name;
    uint8_t face[3];
} SOURCES[TITLE_ART_SOURCES] = {
    [TITLE_ART_GAMEMODE_QFS] = { "control/gamemode.qfs", { 238, 0, 0 } },
    [TITLE_ART_CENTRAL_QFS] = { "control/central.qfs", { 238, 0, 0 } },
    [TITLE_ART_TRMTSTND_QFS] = { "stats/trmtstnd.qfs", { 222, 0, 0 } },
    [TITLE_ART_GRAPHICS_QFS] = { "option/graphics.qfs", { 0, 172, 8 } },
};

enum {
    SMALL_TOP = 70,   /* the small capitals' top row; the capitals' is TITLE_ART_TOP */
    CUT_PAD = 1,      /* the letters cut 1 px wider all round for their anti-aliased edge; no neighbour is nearer
                       * than 3 px on the CD, so no pad takes in another letter's edge */
    PIECES = 12,
    WORD = -1,        /* a piece that is a word break */
    SMALL_Z = -2      /* the small z, made from the small e of the piece after it in the table */
};

/* a letter (or a whole word kept as the title has it): its source and its box on that screen */
typedef struct {
    int source, x0, x1, y0;   /* y1 is TITLE_ART_BASELINE for all */
} Piece;

/* port: "Customize Game" has no screen of its own: C from Control Central, the small letters from Tournament
 * Standings, Game from Game Mode. Graphics Detail and Game Mode are their own screens' titles, uncut. */
static const Piece TITLES[TITLE_ART_COUNT][PIECES] = {
    [TITLE_ART_CUSTOMIZE_GAME] = {
        { TITLE_ART_CENTRAL_QFS, 9, 29, TITLE_ART_TOP },     /* C */
        { TITLE_ART_TRMTSTND_QFS, 54, 69, SMALL_TOP },       /* u */
        { TITLE_ART_TRMTSTND_QFS, 403, 418, SMALL_TOP },     /* s */
        { TITLE_ART_TRMTSTND_QFS, 204, 219, SMALL_TOP },     /* t */
        { TITLE_ART_TRMTSTND_QFS, 31, 48, SMALL_TOP },       /* o */
        { TITLE_ART_TRMTSTND_QFS, 140, 157, SMALL_TOP },     /* m */
        { TITLE_ART_TRMTSTND_QFS, 350, 354, SMALL_TOP },     /* i */
        { SMALL_Z, 0, 0, 0 },                                /* z */
        { TITLE_ART_TRMTSTND_QFS, 164, 178, SMALL_TOP },     /* e */
        { WORD, 0, 0, 0 },
        { TITLE_ART_GAMEMODE_QFS, 9, 95, TITLE_ART_TOP },    /* Game */
    },
    [TITLE_ART_GRAPHICS_DETAIL] = { { TITLE_ART_GRAPHICS_QFS, 9, 301, TITLE_ART_TOP } },
    [TITLE_ART_GAME_MODE] = { { TITLE_ART_GAMEMODE_QFS, 9, 206, TITLE_ART_TOP } },
};

static const char *const TEXTS[TITLE_ART_COUNT] = { "Customize Game", "Graphics Detail", "Game Mode" };

/* the pieces in use: a title's row ends at its first empty one */
static int piece_count(const Piece *pieces)
{
    int n = 0;
    while (n < PIECES && (pieces[n].source < 0 || pieces[n].x1 > 0)) n++;
    return n;
}

void title_art_mask_from_screen(const uint8_t *pixels, int pw, const uint8_t palette[768], const uint8_t face[3],
                                int x0, int y0, int x1, int y1, uint8_t *mask)
{
    /* in thirds, so the greys stay whole: f = 3F - sum(F), p = 3P - sum(P); the part is p.f / f.f */
    const int fs = face[0] + face[1] + face[2];
    int64_t f[3], ff = 0;
    for (int c = 0; c < 3; c++) f[c] = 3 * face[c] - fs, ff += f[c] * f[c];
    const int w = x1 - x0 + 1;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            const uint8_t *p = palette + 3 * pixels[(size_t)y * pw + x];
            const int ps = p[0] + p[1] + p[2];
            int64_t pf = 0;
            for (int c = 0; c < 3; c++) pf += (3 * p[c] - ps) * f[c];
            /* 15 * (pf / ff - LOW) / (HIGH - LOW), rounded */
            const int64_t num = 15 * (1000 * pf - TITLE_ART_LOW * ff), den = (TITLE_ART_HIGH - TITLE_ART_LOW) * ff;
            int64_t level = num <= 0 || ff == 0 ? 0 : (2 * num + den) / (2 * den);
            mask[(y - y0) * w + x - x0] = (uint8_t)(level > 15 ? 15 : level);
        }
}

/* port: the z's diagonal: a stroke STROKE px wide across, from the top right corner to the bottom left, each pixel
 * covered as much as the stroke crosses it (in 16ths across the pixel's width, sampled at the row's middle) */
enum { Z_STROKE = 5, Z_BAR_ROWS = 4 };

static int diagonal(int x, int y, int lw, int lh)
{
    /* the stroke's right side runs from lw at the top to Z_STROKE at the bottom, in 1/32 px */
    const int yc = 2 * y + 1;   /* the row's middle, in halves */
    const int right = (32 * lw * 2 * lh - 32 * (lw - Z_STROKE) * yc) / (2 * lh), left = right - 32 * Z_STROKE;
    const int lo = 32 * x > left ? 32 * x : left, hi = 32 * (x + 1) < right ? 32 * (x + 1) : right;
    return hi <= lo ? 0 : ((hi - lo) * 15 + 16) / 32;
}

void title_art_small_z(const uint8_t *e, int ew, int eh, uint8_t *z)
{
    /* the e as cut: CUT_PAD all round its lw x lh letter */
    const int lw = ew - 2 * CUT_PAD, lh = eh - 2 * CUT_PAD;
    memset(z, 0, (size_t)ew * eh);
    for (int y = 0; y < eh; y++) {
        const int ly = y - CUT_PAD;
        for (int x = 0; x < ew; x++) {
            int c = 0;
            if (ly < Z_BAR_ROWS) c = e[y * ew + (ew - 1 - x)];           /* the top bar, its serif to the left */
            else if (ly >= lh - Z_BAR_ROWS) c = e[y * ew + x];           /* the bottom bar as the e's */
            const int lx = x - CUT_PAD;
            if (ly >= 0 && ly < lh && lx >= 0 && lx < lw) {
                const int d = diagonal(lx, ly, lw, lh);
                if (d > c) c = d;
            }
            z[y * ew + x] = (uint8_t)c;
        }
    }
}

/* the piece's cut (its box with CUT_PAD all round) into `cut`; its size in *cw, *ch */
static void cut_piece(const TitleArtScreen *s, const uint8_t face[3], const Piece *p, uint8_t *cut, int *cw, int *ch)
{
    *cw = p->x1 - p->x0 + 1 + 2 * CUT_PAD, *ch = TITLE_ART_BASELINE - p->y0 + 1 + 2 * CUT_PAD;
    title_art_mask_from_screen(s->pixels, TITLE_ART_SCREEN_W, s->palette, face, p->x0 - CUT_PAD, p->y0 - CUT_PAD,
                               p->x1 + CUT_PAD, TITLE_ART_BASELINE + CUT_PAD, cut);
}

/* the cut cw x ch into the mask, its letter's left column at x, its bottom row on the baseline; the higher coverage
 * where two cuts' pads meet */
static void place(uint8_t *mask, const uint8_t *cut, int cw, int ch, int x)
{
    const int top = TITLE_ART_H - TITLE_ART_PAD - (ch - CUT_PAD);
    for (int y = 0; y < ch; y++)
        for (int i = 0; i < cw; i++) {
            const int mx = x - CUT_PAD + i, my = top + y;
            if (mx < 0 || mx >= TITLE_ART_MAX_W || my < 0 || my >= TITLE_ART_H) continue;
            uint8_t *const at = &mask[my * TITLE_ART_MAX_W + mx];
            if (cut[y * cw + i] > *at) *at = cut[y * cw + i];
        }
}

int title_art_compose(TitleArtId id, const TitleArtScreen screens[TITLE_ART_SOURCES], uint8_t *mask, int *w)
{
    enum { CUT_MAX = TITLE_ART_MAX_W * TITLE_ART_H };
    if ((unsigned)id >= TITLE_ART_COUNT) return 0;
    const Piece *const pieces = TITLES[id];
    const int n = piece_count(pieces);
    for (int i = 0; i < n; i++)
        if (pieces[i].source >= 0 && (screens[pieces[i].source].pixels == NULL || screens[pieces[i].source].palette == NULL))
            return 0;
    uint8_t *const cut = malloc(CUT_MAX), *const z = malloc(CUT_MAX);
    if (cut == NULL || z == NULL) {
        free(cut), free(z);
        return 0;
    }
    memset(mask, 0, CUT_MAX);
    int x = TITLE_ART_PAD, end = x - 1, cw = 0, ch = 0;
    for (int i = 0; i < n; i++) {
        const Piece *const p = &pieces[i];
        if (p->source == WORD) {
            x = end + 1 + TITLE_ART_WORD_GAP;
            continue;
        }
        const Piece *const from = p->source == SMALL_Z ? &pieces[i + 1] : p;   /* the z from the e after it */
        cut_piece(&screens[from->source], SOURCES[from->source].face, from, cut, &cw, &ch);
        if (p->source == SMALL_Z) title_art_small_z(cut, cw, ch, z);
        place(mask, p->source == SMALL_Z ? z : cut, cw, ch, x);
        end = x + cw - 2 * CUT_PAD - 1;
        x = end + 1 + TITLE_ART_LETTER_GAP;
    }
    free(cut), free(z);
    *w = end + 1 + TITLE_ART_PAD;
    /* packed w wide */
    for (int y = 1; y < TITLE_ART_H; y++) memmove(mask + y * *w, mask + y * TITLE_ART_MAX_W, (size_t)*w);
    return 1;
}

int title_art_screen(uint8_t *data, size_t size, uint8_t palette[768], TitleArtScreen *s)
{
    static const char BGND[4] = { 'b', 'g', 'n', 'd' }, PAL[4] = { '!', 'p', 'a', 'l' };
    Fsh f;
    FshEntry bg, pal;
    uint32_t j, k;
    if (data == NULL || fsh_open(&f, data, size) != FSH_OK || fsh_find(&f, BGND, &j) != FSH_OK ||
        fsh_entry(&f, j, &bg) != FSH_OK || bg.type != FSH_TYPE_BITMAP || bg.width != TITLE_ART_SCREEN_W ||
        bg.height != TITLE_ART_SCREEN_H || bg.payload_size < TITLE_ART_SCREEN_W * TITLE_ART_SCREEN_H ||
        fsh_find(&f, PAL, &k) != FSH_OK || fsh_entry(&f, k, &pal) != FSH_OK || fsh_palette_rgb(&pal, palette) != FSH_OK)
        return 0;
    s->pixels = bg.payload, s->palette = palette;
    return 1;
}

int title_art_load(ConfigBgndRead read, TitleArtId id, uint8_t *mask, int *w)
{
    if ((unsigned)id >= TITLE_ART_COUNT) return 0;
    uint8_t *data[TITLE_ART_SOURCES] = { NULL }, palettes[TITLE_ART_SOURCES][768];
    TitleArtScreen screens[TITLE_ART_SOURCES] = { { NULL, NULL } };
    const int n = piece_count(TITLES[id]);
    int ok = 1;
    for (int i = 0; i < n && ok; i++) {
        const int s = TITLES[id][i].source;
        if (s < 0 || data[s] != NULL) continue;
        size_t size = 0;
        data[s] = read(SOURCES[s].name, &size);
        ok = title_art_screen(data[s], size, palettes[s], &screens[s]);
    }
    if (ok) ok = title_art_compose(id, screens, mask, w);
    for (int s = 0; s < TITLE_ART_SOURCES; s++) free(data[s]);
    return ok;
}

TitleArtId title_art_for(const char *text)
{
    for (int i = 0; text != NULL && i < TITLE_ART_COUNT; i++)
        if (strcmp(text, TEXTS[i]) == 0) return (TitleArtId)i;
    return TITLE_ART_COUNT;
}
