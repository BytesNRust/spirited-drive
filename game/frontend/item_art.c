/* port: the item-size labels in the letters of Game Mode's own items (item_art.h). */
#include "game/frontend/item_art.h"

#include "formats/fsh.h"

#include <stdlib.h>
#include <string.h>

enum {
    CUT_PAD = 1,              /* the letters cut 1 px wider all round for their anti-aliased edge (title_art.c) */
    ITEM_NOISE = 3,           /* a level under this is none: the two shapes differ by a level or two off the letters */
    SCREEN_W = 640,
    SHAPE_BASELINE = 19,      /* the items' capitals end on the 19th row of their shape, the small ones start on 5th */
    TITLE_BASELINE = 84,      /* the titles' capitals end on row 84 of the screen */
    SMALL_TOP = 5,            /* the small capitals' top row in a shape; the capitals' is 1 */
    TITLE_CAP_TOP = 66, TITLE_SMALL_TOP = 70,
    PIECES = 16,
    WORD = -1,                /* a piece that is a word break */
    SMALL_Z = -2              /* the small z, made from the small e of the piece after it in the table */
};

/* where each letter's source is: the three Game Mode items (their dark and lit shapes) and the two screens whose
 * titles give the capital and the one small letter the items lack. Titles come after the items. */
enum { SRC_SINGLE, SRC_MODEM, SRC_NETWORK, SRC_CENTRAL, SRC_TRMTSTND, SRC_COUNT };

/* a letter: its source and its box on that source (y1 is the source's baseline for all) */
typedef struct {
    int source, x0, x1, y0;
} Piece;

/* port: "Customize game" in the items' letters: the capital C from Control Central's title and the small u from
 * Tournament Standings' (both the items' size, which the items' own letters lack), the rest from the items; the
 * lowercase letters of the label are the items' small capitals */
static const Piece ITEMS[ITEM_ART_COUNT][PIECES] = {
    [ITEM_ART_CUSTOMIZE_GAME] = {
        { SRC_CENTRAL, 9, 29, TITLE_CAP_TOP },      /* C */
        { SRC_TRMTSTND, 54, 69, TITLE_SMALL_TOP },  /* u */
        { SRC_MODEM, 130, 146, SMALL_TOP },         /* s: SERIAL's */
        { SRC_NETWORK, 48, 63, SMALL_TOP },         /* t: ETWORK's */
        { SRC_MODEM, 31, 48, SMALL_TOP },           /* o: ODEM's */
        { SRC_MODEM, 96, 114, SMALL_TOP },          /* m: MODEM's */
        { SRC_SINGLE, 27, 32, SMALL_TOP },          /* i: INGLE's */
        { SMALL_Z, 0, 0, 0 },                       /* z, from the e after it */
        { SRC_SINGLE, 104, 118, SMALL_TOP },        /* e: INGLE's */
        { WORD, 0, 0, 0 },
        { SRC_SINGLE, 61, 78, SMALL_TOP },          /* g: INGLE's */
        { SRC_SINGLE, 184, 201, SMALL_TOP },        /* a: PLAYER's */
        { SRC_MODEM, 96, 114, SMALL_TOP },          /* m */
        { SRC_SINGLE, 104, 118, SMALL_TOP },        /* e */
    },
};

static const char *const TEXTS[ITEM_ART_COUNT] = { "Customize game" };

/* the pieces in use: an item's row ends at its first empty one */
static int piece_count(const Piece *pieces)
{
    int n = 0;
    while (n < PIECES && (pieces[n].source < 0 || pieces[n].x1 > 0)) n++;
    return n;
}

static int is_title(int source) { return source >= SRC_CENTRAL; }

static int baseline_of(int source) { return is_title(source) ? TITLE_BASELINE : SHAPE_BASELINE; }

void item_art_mask_from_pair(const uint8_t *dark, const uint8_t *lit, int pw, const uint8_t palette[768], int x0,
                             int y0, int x1, int y1, uint8_t *mask)
{
    /* the cream fill less the yellow one, in RGB; a pixel's dark less lit is that difference times its coverage */
    static const uint8_t CREAM[3] = { 238, 214, 182 }, YELLOW[3] = { 255, 206, 0 };
    int64_t dc[3], den = 0;
    for (int c = 0; c < 3; c++) dc[c] = CREAM[c] - YELLOW[c], den += dc[c] * dc[c];
    const int w = x1 - x0 + 1;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            const uint8_t *const d = palette + 3 * dark[y * pw + x], *const l = palette + 3 * lit[y * pw + x];
            int64_t p = 0;
            for (int c = 0; c < 3; c++) p += (d[c] - l[c]) * dc[c];
            /* 15 * p / den, rounded */
            int64_t level = p <= 0 ? 0 : (2 * 15 * p + den) / (2 * den);
            if (level > 15) level = 15;
            if (level < ITEM_NOISE) level = 0;
            mask[(y - y0) * w + x - x0] = (uint8_t)level;
        }
}

/* the piece's source is present in `s` */
static int source_ok(const ItemArtSources *s, int source)
{
    switch (source) {
    case SRC_SINGLE: return s->single.dark != NULL && s->single.lit != NULL;
    case SRC_MODEM: return s->modem.dark != NULL && s->modem.lit != NULL;
    case SRC_NETWORK: return s->network.dark != NULL && s->network.lit != NULL;
    case SRC_CENTRAL: return s->central.pixels != NULL && s->central.palette != NULL;
    case SRC_TRMTSTND: return s->trmtstnd.pixels != NULL && s->trmtstnd.palette != NULL;
    default: return 0;
    }
}

/* the box x0..x1, y0..y1 of a shape pair (or of a title's screen, its face) cut into `cut` */
static void cut_shapes(const ItemArtShape *sh, int x0, int y0, int x1, int y1, uint8_t *cut)
{
    item_art_mask_from_pair(sh->dark, sh->lit, sh->w, sh->palette, x0, y0, x1, y1, cut);
}

static void cut_title(const TitleArtScreen *screen, const uint8_t face[3], int x0, int y0, int x1, int y1,
                      uint8_t *cut)
{
    title_art_mask_from_screen(screen->pixels, SCREEN_W, screen->palette, face, x0, y0, x1, y1, cut);
}

/* the piece's box with CUT_PAD all round, cut into `cut`; its size in *cw, *ch */
static void cut_piece(const ItemArtSources *s, const Piece *p, uint8_t *cut, int *cw, int *ch)
{
    /* port: the titles' faces (title_art.c): Control Central's red, Tournament Standings' darker */
    static const uint8_t RED[3] = { 238, 0, 0 }, TRMTSTND_RED[3] = { 222, 0, 0 };
    const int base = baseline_of(p->source);
    const int x0 = p->x0 - CUT_PAD, y0 = p->y0 - CUT_PAD, x1 = p->x1 + CUT_PAD, y1 = base + CUT_PAD;
    *cw = p->x1 - p->x0 + 1 + 2 * CUT_PAD, *ch = base - p->y0 + 1 + 2 * CUT_PAD;
    switch (p->source) {
    case SRC_SINGLE: cut_shapes(&s->single, x0, y0, x1, y1, cut); break;
    case SRC_MODEM: cut_shapes(&s->modem, x0, y0, x1, y1, cut); break;
    case SRC_NETWORK: cut_shapes(&s->network, x0, y0, x1, y1, cut); break;
    case SRC_CENTRAL: cut_title(&s->central, RED, x0, y0, x1, y1, cut); break;
    case SRC_TRMTSTND: cut_title(&s->trmtstnd, TRMTSTND_RED, x0, y0, x1, y1, cut); break;
    default: break;
    }
}

/* the cut cw x ch into the mask, its letter's left column at x, its bottom row on the baseline; the higher coverage
 * where two cuts' pads meet */
static void place(uint8_t *mask, const uint8_t *cut, int cw, int ch, int x)
{
    const int top = ITEM_ART_H - ITEM_ART_PAD - (ch - CUT_PAD);
    for (int y = 0; y < ch; y++)
        for (int i = 0; i < cw; i++) {
            const int mx = x - CUT_PAD + i, my = top + y;
            if (mx < 0 || mx >= ITEM_ART_MAX_W || my < 0 || my >= ITEM_ART_H) continue;
            uint8_t *const at = &mask[my * ITEM_ART_MAX_W + mx];
            if (cut[y * cw + i] > *at) *at = cut[y * cw + i];
        }
}

int item_art_compose(ItemArtId id, const ItemArtSources *s, uint8_t *mask, int *w)
{
    enum { CUT_MAX = ITEM_ART_MAX_W * ITEM_ART_H };
    if ((unsigned)id >= ITEM_ART_COUNT) return 0;
    const Piece *const pieces = ITEMS[id];
    const int n = piece_count(pieces);
    for (int i = 0; i < n; i++)
        if (pieces[i].source >= 0 && !source_ok(s, pieces[i].source)) return 0;
    uint8_t *const cut = malloc(CUT_MAX), *const z = malloc(CUT_MAX);
    if (cut == NULL || z == NULL) {
        free(cut), free(z);
        return 0;
    }
    memset(mask, 0, CUT_MAX);
    int x = ITEM_ART_PAD, end = x - 1, cw = 0, ch = 0;
    for (int i = 0; i < n; i++) {
        const Piece *const p = &pieces[i];
        if (p->source == WORD) {
            x = end + 1 + ITEM_ART_WORD_GAP;
            continue;
        }
        const Piece *const from = p->source == SMALL_Z ? &pieces[i + 1] : p;   /* the z from the e after it */
        cut_piece(s, from, cut, &cw, &ch);
        if (p->source == SMALL_Z) title_art_small_z(cut, cw, ch, z);
        place(mask, p->source == SMALL_Z ? z : cut, cw, ch, x);
        end = x + cw - 2 * CUT_PAD - 1;
        x = end + 1 + ITEM_ART_LETTER_GAP;
    }
    free(cut), free(z);
    *w = end + 1 + ITEM_ART_PAD;
    /* packed w wide */
    for (int y = 1; y < ITEM_ART_H; y++) memmove(mask + y * *w, mask + y * ITEM_ART_MAX_W, (size_t)*w);
    return 1;
}

/* a Game Mode item's dark and lit shapes out of the file `data` (size bytes), with the palette they are drawn in */
static int shapes_of(uint8_t *data, size_t size, const uint8_t palette[768], const char dark[4], const char lit[4],
                     ItemArtShape *shape)
{
    Fsh f;
    FshEntry d, l;
    uint32_t i, j;
    if (data == NULL || fsh_open(&f, data, size) != FSH_OK || fsh_find(&f, dark, &i) != FSH_OK ||
        fsh_entry(&f, i, &d) != FSH_OK || fsh_find(&f, lit, &j) != FSH_OK || fsh_entry(&f, j, &l) != FSH_OK)
        return 0;
    if (d.type != FSH_TYPE_BITMAP || l.type != FSH_TYPE_BITMAP || d.width != l.width || d.height != l.height ||
        d.payload_size < (size_t)d.width * d.height || l.payload_size < (size_t)l.width * l.height)
        return 0;
    shape->dark = d.payload, shape->lit = l.payload, shape->w = d.width, shape->h = d.height, shape->palette = palette;
    return 1;
}

int item_art_load(ConfigBgndRead read, ItemArtId id, uint8_t *mask, int *w)
{
    if ((unsigned)id >= ITEM_ART_COUNT) return 0;
    uint8_t *gamemode = NULL, *central = NULL, *trmtstnd = NULL;
    uint8_t pal_gamemode[768], pal_central[768], pal_trmtstnd[768];
    ItemArtSources s;
    memset(&s, 0, sizeof s);
    size_t size_gamemode = 0, size_central = 0, size_trmtstnd = 0;
    gamemode = read("control/gamemode.qfs", &size_gamemode);
    central = read("control/central.qfs", &size_central);
    trmtstnd = read("stats/trmtstnd.qfs", &size_trmtstnd);
    /* the shapes are drawn in the palette of the file's "bgnd" (the screen is read for its palette only) */
    TitleArtScreen gamemode_screen;
    int ok = title_art_screen(gamemode, size_gamemode, pal_gamemode, &gamemode_screen) &&
             title_art_screen(central, size_central, pal_central, &s.central) &&
             title_art_screen(trmtstnd, size_trmtstnd, pal_trmtstnd, &s.trmtstnd) &&
             shapes_of(gamemode, size_gamemode, pal_gamemode, "sind", "sinl", &s.single) &&
             shapes_of(gamemode, size_gamemode, pal_gamemode, "modd", "modl", &s.modem) &&
             shapes_of(gamemode, size_gamemode, pal_gamemode, "netd", "netl", &s.network);
    if (ok) ok = item_art_compose(id, &s, mask, w);
    free(gamemode), free(central), free(trmtstnd);
    return ok;
}

ItemArtId item_art_for(const char *text)
{
    for (int i = 0; text != NULL && i < ITEM_ART_COUNT; i++)
        if (strcmp(text, TEXTS[i]) == 0) return (ItemArtId)i;
    return ITEM_ART_COUNT;
}
