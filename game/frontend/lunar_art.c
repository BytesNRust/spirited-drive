/* port (dep59, Lunar Springs' entry, L4): Lunar Springs' bank from Rusty Springs' (lunar_art.h): every picture in greys by its
 * luminance, the colours kept (the red in its boxes, a lit picture's yellow) in palette entries of their own, the rest a ramp. */
#include "game/frontend/lunar_art.h"
#include "game/frontend/lunar_art_private.h"
#include "formats/shpi_build.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ENTRIES_MAX = 48, PIXELS = LUNAR_ART_W * LUNAR_ART_H,
    POINTER_FIRST = 253,   /* 253..255 the pointer's (MOUSE.FSH), and entry 0: as the CD has them */
    RAMP_FIRST = 1         /* the kept colours from here, then the ramp up to POINTER_FIRST - 1 */
};

/* a letter: TITLE_EDGE over its filled-in box, within TITLE_NEAR of a letter's white */
enum { TITLE_PAD = 3, TITLE_PASSES = 200, TITLE_EDGE = 14, TITLE_NEAR = 2, RUNS_MAX = 32 };

/* the red the screen reads by, kept where it is: the map's racing line, Best Times' column heads (x0, y0, x1, y1) */
static const int RED_BOXES[2][2][4] = { { { 312, 86, 609, 381 }, { 0, 0, -1, -1 } }, { { 0, 94, 639, 114 }, { 0, 364, 639, 384 } } };

void lunar_why(char *why, size_t why_size, const char *format, ...)
{
    if (why == NULL || why_size == 0) return;
    va_list ap;
    va_start(ap, format);
    vsnprintf(why, why_size, format, ap);
    va_end(ap);
}

typedef struct {
    ShpiDescEntry e[ENTRIES_MAX];
    size_t n;
    int bgnd, pal;       /* their places in e[] */
    uint8_t rgb[768];    /* the palette, 8 bits a channel */
} Bank;

static int is_red(const uint8_t *c) { return c[0] > 150 && c[1] < 60 && c[2] < 60 && c[0] > 3 * c[1] && c[0] > 3 * c[2]; }
static int is_yellow(const uint8_t *c) { return c[0] > 170 && c[1] > 120 && c[2] < 100 && c[1] * 10 > c[0] * 6; }
static uint8_t luma(const uint8_t *c) { return (uint8_t)((299 * c[0] + 587 * c[1] + 114 * c[2]) / 1000); }

static int open_bank(const uint8_t *d, size_t size, Bank *b, char *why, size_t why_size)
{
    ExchangeError err;
    memset(b, 0, sizeof *b);
    b->bgnd = b->pal = -1;
    if (d == NULL || !shpi_describe(d, size, b->e, ENTRIES_MAX, &b->n, &err)) return lunar_why(why, why_size, "not a bank"), 0;
    for (size_t i = 0; i < b->n; i++) {
        const ShpiDescEntry *e = &b->e[i];
        if (memcmp(e->name, "bgnd", 4) == 0 && e->type == 0x7b && e->width == LUNAR_ART_W && e->height == LUNAR_ART_H) b->bgnd = (int)i;
        if ((e->type == 0x22 || e->type == 0x24) && e->width == 256) b->pal = (int)i;
    }
    if (b->bgnd < 0 || b->pal < 0) return lunar_why(why, why_size, "no 640 x 480 bgnd or no palette"), 0;
    const ShpiDescEntry *p = &b->e[b->pal];
    for (int k = 0; k < 768; k++) b->rgb[k] = (uint8_t)(p->type == 0x22 ? p->data[k] << 2 : p->data[k]);
    return 1;
}

static void grey_of(const Bank *b, uint8_t *g)
{
    const uint8_t *px = b->e[b->bgnd].data;
    for (int i = 0; i < PIXELS; i++) g[i] = luma(b->rgb + 3 * px[i]);
}

static int in_red_box(LunarArtKind kind, int x, int y)
{
    for (int k = 0; k < 2; k++) {
        const int *r = RED_BOXES[kind][k];
        if (x >= r[0] && y >= r[1] && x <= r[2] && y <= r[3]) return 1;
    }
    return 0;
}

/* the picture's pixels kept red: red in a box, with two red neighbours at least (a speck of the desert is not the line) */
static void red_kept(const Bank *b, LunarArtKind kind, uint8_t *kept)
{
    const uint8_t *px = b->e[b->bgnd].data;
    for (int y = 0; y < LUNAR_ART_H; y++)
        for (int x = 0; x < LUNAR_ART_W; x++) {
            int near = 0;
            kept[y * LUNAR_ART_W + x] = 0;
            if (!in_red_box(kind, x, y) || !is_red(b->rgb + 3 * px[y * LUNAR_ART_W + x])) continue;
            for (int j = -1; j <= 1; j++)
                for (int i = -1; i <= 1; i++) {
                    const int xx = x + i, yy = y + j;
                    if ((i || j) && xx >= 0 && yy >= 0 && xx < LUNAR_ART_W && yy < LUNAR_ART_H) near += is_red(b->rgb + 3 * px[yy * LUNAR_ART_W + xx]);
                }
            kept[y * LUNAR_ART_W + x] = near >= 2;
        }
}

/* a lit picture: its name's last letter (DONE's donl, dond) */
static int is_lit(const ShpiDescEntry *e) { return e->name[3] == 'l'; }

typedef struct {
    int slot[256];       /* a kept colour's entry, by its entry in Rusty's palette; 0 for none */
    int kept, ramp;      /* how many kept; the grey ramp's entries (after the red's) */
    uint8_t out[768];    /* the new palette, 8 bits a channel */
} Palette;

static int keep(Palette *p, const Bank *b, int index)
{
    if (p->slot[index] != 0) return 1;
    if (p->kept >= LUNAR_ART_KEPT_MAX) return 0;
    p->slot[index] = RAMP_FIRST + p->kept++;
    memcpy(p->out + 3 * p->slot[index], b->rgb + 3 * index, 3);
    return 1;
}

static uint8_t ramp(const Palette *p, uint8_t grey)
{
    return (uint8_t)(RAMP_FIRST + p->kept + LUNAR_ART_RED_RAMP + (grey * (p->ramp - 1) + 127) / 255);
}

/* the title's red for a letter's grey */
static uint8_t red_ramp(const Palette *p, uint8_t grey) { return (uint8_t)(RAMP_FIRST + p->kept + (grey * (LUNAR_ART_RED_RAMP - 1) + 127) / 255); }

/* the CD's red: the colour most of the kept red pixels are (the racing line's); a plain red when none is kept */
static void cd_red(const Bank *b, const uint8_t *red, uint8_t rgb[3])
{
    static int count[256];
    memset(count, 0, sizeof count);
    const uint8_t *px = b->e[b->bgnd].data;
    for (int i = 0; i < PIXELS; i++) count[px[i]] += red[i];
    int best = -1;
    for (int k = 0; k < 256; k++)
        if (count[k] > 0 && (best < 0 || count[k] > count[best])) best = k;
    static const uint8_t PLAIN[3] = { 200, 24, 24 };
    memcpy(rgb, best >= 0 ? b->rgb + 3 * best : PLAIN, 3);
}

/* the colours kept, by the entry's order in Rusty's palette (the same palette every run), then the ramp */
static int palette_make(Palette *p, const Bank *b, const uint8_t *red)
{
    memset(p, 0, sizeof *p);
    memcpy(p->out, b->rgb, 3);
    memcpy(p->out + 3 * POINTER_FIRST, b->rgb + 3 * POINTER_FIRST, 3 * (256 - POINTER_FIRST));
    uint8_t wanted[256] = { 0 };
    const uint8_t *px = b->e[b->bgnd].data;
    for (int i = 0; i < PIXELS; i++) wanted[px[i]] |= red[i];
    for (size_t k = 0; k < b->n; k++) {
        const ShpiDescEntry *e = &b->e[k];
        if (e->type != 0x7b || (int)k == b->bgnd || !is_lit(e)) continue;
        for (int i = 0; i < e->width * e->height; i++) wanted[e->data[i]] |= is_yellow(b->rgb + 3 * e->data[i]);
    }
    for (int i = 0; i < 256; i++)
        if (wanted[i] && !keep(p, b, i)) return 0;
    uint8_t r[3];
    cd_red(b, red, r);
    const int top = r[0] > r[1] ? (r[0] > r[2] ? r[0] : r[2]) : (r[1] > r[2] ? r[1] : r[2]);
    for (int s = 0; s < LUNAR_ART_RED_RAMP; s++) {   /* each level's brightest channel is the grey it stands for */
        const int v = s * 255 / (LUNAR_ART_RED_RAMP - 1);
        for (int c = 0; c < 3; c++) p->out[3 * (RAMP_FIRST + p->kept + s) + c] = (uint8_t)(top > 0 ? r[c] * v / top : v);
    }
    p->ramp = POINTER_FIRST - RAMP_FIRST - p->kept - LUNAR_ART_RED_RAMP;
    for (int s = 0; s < p->ramp; s++) memset(p->out + 3 * (RAMP_FIRST + p->kept + LUNAR_ART_RED_RAMP + s), s * 255 / (p->ramp - 1), 3);
    return 1;
}

/* a picture other than bgnd: its greys, a lit one's yellow kept */
static uint8_t *other_make(const ShpiDescEntry *e, const Bank *b, const Palette *p)
{
    const size_t n = (size_t)e->width * e->height;
    uint8_t *out = malloc(n);
    for (size_t i = 0; out != NULL && i < n; i++) {
        const uint8_t *c = b->rgb + 3 * e->data[i];
        out[i] = is_lit(e) && is_yellow(c) ? (uint8_t)p->slot[e->data[i]] : ramp(p, luma(c));
    }
    return out;
}

/* 1 when a letter's white (LUNAR_LETTER_WHITE) is within TITLE_NEAR pixels of (x, y): a bright speck of the texture is not a letter */
static int near_white(const uint8_t *g, int x, int y)
{
    for (int j = -TITLE_NEAR; j <= TITLE_NEAR; j++)
        for (int i = -TITLE_NEAR; i <= TITLE_NEAR; i++) {
            const int xx = x + i, yy = y + j;
            if (xx >= 0 && yy >= 0 && xx < LUNAR_ART_W && yy < LUNAR_ART_H && g[yy * LUNAR_ART_W + xx] > LUNAR_LETTER_WHITE) return 1;
        }
    return 0;
}

/* the title's letters on the finished grey picture: in each run of letter columns, what stands TITLE_EDGE brighter than the run's box
 * filled in from its border (a letter's white and its edge; the shadow and the texture stay grey) */
static void title_letters(const uint8_t *g, uint8_t *letter)
{
    static uint8_t bg[PIXELS];
    LunarRun runs[RUNS_MAX];
    const int n = lunar_title_runs(g, LUNAR_ART_W, LUNAR_ART_H, runs, RUNS_MAX);
    memcpy(bg, g, PIXELS);
    memset(letter, 0, PIXELS);
    for (int k = 0; k < n; k++) {
        const int x0 = runs[k].x0 - TITLE_PAD < 0 ? 0 : runs[k].x0 - TITLE_PAD;
        const int x1 = runs[k].x1 + TITLE_PAD >= LUNAR_ART_W ? LUNAR_ART_W - 1 : runs[k].x1 + TITLE_PAD;
        lunar_fill(bg, LUNAR_ART_W, x0, LUNAR_TITLE_Y0 - 1, x1, LUNAR_TITLE_Y1, TITLE_PASSES);
        for (int y = LUNAR_TITLE_Y0; y < LUNAR_TITLE_Y1; y++)
            for (int x = x0 + 1; x < x1; x++)
                letter[y * LUNAR_ART_W + x] = g[y * LUNAR_ART_W + x] >= bg[y * LUNAR_ART_W + x] + TITLE_EDGE && near_white(g, x, y);
    }
}

static int build(Bank *b, const uint8_t *g, const uint8_t *red, const uint8_t *letter, const Palette *p, uint8_t **out, size_t *out_size,
                 char *why, size_t why_size)
{
    uint8_t *made[ENTRIES_MAX] = { NULL };
    static uint8_t pal[768];   /* the bank (static in the caller) keeps pointing at it */
    int ok = 1;
    const ShpiDescEntry *pe = &b->e[b->pal];
    for (int k = 0; k < 768; k++) pal[k] = (uint8_t)(pe->type == 0x22 ? p->out[k] >> 2 : p->out[k]);
    b->e[b->pal].data = pal;
    for (size_t k = 0; ok && k < b->n; k++) {
        ShpiDescEntry *e = &b->e[k];
        if (e->type != 0x7b) continue;
        if ((int)k == b->bgnd) {
            made[k] = malloc(PIXELS);
            for (int i = 0; made[k] != NULL && i < PIXELS; i++)
                made[k][i] = red[i] ? (uint8_t)p->slot[e->data[i]] : letter[i] ? red_ramp(p, g[i]) : ramp(p, g[i]);
        } else {
            made[k] = other_make(e, b, p);
        }
        ok = made[k] != NULL;
        if (ok) e->data = made[k];
    }
    ExchangeError err;
    if (ok && !shpi_build(b->e, b->n, out, out_size, &err)) ok = 0, lunar_why(why, why_size, "the bank is not built: %s", err.message);
    else if (!ok) lunar_why(why, why_size, "no room");
    for (size_t k = 0; k < b->n; k++) free(made[k]);
    return ok;
}

int lunar_art_make(const uint8_t *rust, size_t rust_size, const uint8_t *lost, size_t lost_size, LunarArtKind kind, uint8_t **out,
                   size_t *out_size, char *why, size_t why_size)
{
    *out = NULL, *out_size = 0;
    static Bank r, l;
    static uint8_t g[PIXELS], lg[PIXELS], red[PIXELS], letter[PIXELS];
    static Palette p;
    if (!open_bank(rust, rust_size, &r, why, why_size) || !open_bank(lost, lost_size, &l, why, why_size)) return 0;
    grey_of(&r, g), grey_of(&l, lg);
    if (!lunar_title(g, lg, why, why_size)) return 0;
    if (kind == LUNAR_ART_SELECT) lunar_drop_time_line(g);
    red_kept(&r, kind, red);
    title_letters(g, letter);
    if (!palette_make(&p, &r, red)) return lunar_why(why, why_size, "more than %d colours to keep", LUNAR_ART_KEPT_MAX), 0;
    return build(&r, g, red, letter, &p, out, out_size, why, why_size);
}
