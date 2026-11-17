/* dep59 (Lunar Springs' entry, L4): Lunar Springs' track detail and Best Times art made from Rusty Springs' (game/frontend/lunar_art.h):
 * the title's letters found, the fill, the refusals; and on the CD's own banks (English and German) when original/ is there: the title
 * says LUNAR where RUSTY was, the time of day's line is gone, the picture is grey but for its kept colours, the pointer's entries are the
 * CD's, a dim button is grey and a lit one keeps its yellow, and the same banks make the same bytes. */
#include "game/frontend/lunar_art.h"
#include "formats/qfs.h"
#include "formats/shpi_build.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_runs(void)
{
    static uint8_t g[100 * 60];
    memset(g, 90, sizeof g);
    for (int y = 20; y <= 30; y++) {
        for (int x = 10; x <= 14; x++) g[y * 100 + x] = 250;
        for (int x = 20; x <= 22; x++) g[y * 100 + x] = 230;
    }
    for (int x = 0; x < 100; x++) g[56 * 100 + x] = 255;   /* the rule under the title: not a letter */
    g[5 * 100 + 50] = 255;                                 /* above the title's rows */
    LunarRun r[8];
    CHECK_EQ_INT(lunar_title_runs(g, 100, 60, r, 8), 2);
    CHECK(r[0].x0 == 10 && r[0].x1 == 14 && r[1].x0 == 20 && r[1].x1 == 22);
    CHECK_EQ_INT(lunar_title_runs(g, 100, 60, r, 1), 1);   /* at most max */
}

static void test_fill(void)
{
    enum { W = 40, H = 20 };
    static uint8_t g[W * H];
    memset(g, 7, sizeof g);
    for (int y = 2; y <= 12; y++) g[y * W + 5] = 0, g[y * W + 25] = 200;   /* the left and right borders */
    for (int x = 5; x <= 25; x++) g[2 * W + x] = (uint8_t)((x - 5) * 10), g[12 * W + x] = (uint8_t)((x - 5) * 10);
    for (int y = 3; y < 12; y++)
        for (int x = 6; x < 25; x++) g[y * W + x] = 255;   /* the letters */
    lunar_fill(g, W, 5, 2, 25, 12, 400);
    CHECK(g[7 * W + 15] > 90 && g[7 * W + 15] < 110);   /* the middle: between the borders, as they are there */
    CHECK(g[7 * W + 6] < 25 && g[7 * W + 24] > 175);
    CHECK(g[2 * W + 5] == 0 && g[12 * W + 25] == 200);   /* the border kept */
    CHECK(g[0] == 7 && g[19 * W + 39] == 7 && g[7 * W + 30] == 7);   /* outside untouched */
}

static void test_refusals(void)
{
    char why[128] = "";
    uint8_t *out = (uint8_t *)1;
    size_t n = 1;
    static const uint8_t junk[64] = { 'S', 'H', 'P', 'I' };
    CHECK_EQ_INT(lunar_art_make(junk, sizeof junk, junk, sizeof junk, LUNAR_ART_SELECT, &out, &n, why, sizeof why), 0);
    CHECK(out == NULL && n == 0 && why[0] != '\0');
    CHECK_EQ_INT(lunar_art_make(NULL, 0, NULL, 0, LUNAR_ART_BEST, &out, &n, why, sizeof why), 0);
}

/* ---- the CD's banks ---- */

static uint8_t *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *d = malloc(1u << 20);
    *size = d != NULL ? fread(d, 1, 1u << 20, f) : 0;
    fclose(f);
    return d;
}

static uint8_t *bank(const char *dir, const char *file, size_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/original/FRONTEND/%s/TRACKSEL/%s", TNFS_SOURCE_DIR, dir, file);
    size_t packed = 0;
    uint8_t *q = read_all(path, &packed);
    if (q == NULL) return NULL;
    *size = 0;
    uint8_t *d = qfs_decompressed_size(q, packed, size) == QFS_OK ? malloc(*size) : NULL;
    if (d != NULL && qfs_decompress(q, packed, d, *size) != QFS_OK) free(d), d = NULL;
    free(q);
    return d;
}

typedef struct {
    ShpiDescEntry e[32];
    size_t n;
    uint8_t rgb[768];
} Bank;

static int open_bank(const uint8_t *d, size_t size, Bank *b)
{
    ExchangeError err;
    if (!shpi_describe(d, size, b->e, 32, &b->n, &err)) return 0;
    for (size_t i = 0; i < b->n; i++)
        if (b->e[i].type == 0x22 || b->e[i].type == 0x24)
            for (int k = 0; k < 768; k++) b->rgb[k] = (uint8_t)(b->e[i].type == 0x22 ? b->e[i].data[k] << 2 : b->e[i].data[k]);
    return 1;
}

static const ShpiDescEntry *entry(const Bank *b, const char *name)
{
    for (size_t i = 0; i < b->n; i++)
        if (memcmp(b->e[i].name, name, 4) == 0) return &b->e[i];
    return NULL;
}

static int is_grey(const uint8_t *c) { return c[0] == c[1] && c[1] == c[2]; }
static int is_yellow(const uint8_t *c) { return c[0] > 170 && c[1] > 120 && c[2] < 100 && c[1] * 10 > c[0] * 6; }

/* the grey picture of a bank's bgnd, its luminance; `bright` (the made bank's): its brightest channel, the grey the title's red stands for */
static void grey_of(const Bank *b, uint8_t *g, int bright)
{
    const ShpiDescEntry *bg = entry(b, "bgnd");
    for (int i = 0; i < LUNAR_ART_W * LUNAR_ART_H; i++) {
        const uint8_t *c = b->rgb + 3 * bg->data[i];
        const uint8_t top = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]);
        g[i] = bright ? top : (uint8_t)((299 * c[0] + 587 * c[1] + 114 * c[2]) / 1000);
    }
}

static int is_reddish(const uint8_t *c) { return c[0] > 2 * c[1] && c[0] > 2 * c[2] && c[0] > 20; }

static void check_bank(const char *dir, const char *file, LunarArtKind kind)
{
    size_t rn = 0, ln = 0;
    uint8_t *rust = bank(dir, file, &rn), *lost = bank(dir, "LOSTSEL.QFS", &ln);
    if (rust == NULL || lost == NULL) {
        printf("skip the CD's %s/%s (no original/)\n", dir, file);
        free(rust), free(lost);
        return;
    }
    uint8_t *out = NULL, *again = NULL;
    size_t on = 0, an = 0;
    char why[128] = "";
    const int ok = lunar_art_make(rust, rn, lost, ln, kind, &out, &on, why, sizeof why);
    CHECK(ok);
    if (!ok) {
        printf("%s/%s: %s\n", dir, file, why);
        free(rust), free(lost);
        return;
    }
    CHECK(lunar_art_make(rust, rn, lost, ln, kind, &again, &an, why, sizeof why) && an == on && memcmp(out, again, on) == 0);
    CHECK(on == rn);   /* the same entries, the same sizes: Rusty's bank's size is its (the cache reads it by its size) */
    static Bank was, now;
    CHECK(open_bank(rust, rn, &was) && open_bank(out, on, &now));
    CHECK_EQ_INT((int)now.n, (int)was.n);
    for (size_t i = 0; i < was.n && i < now.n; i++)   /* the same pictures, the same sizes and places */
        CHECK(memcmp(was.e[i].name, now.e[i].name, 4) == 0 && was.e[i].width == now.e[i].width && was.e[i].height == now.e[i].height
              && memcmp(was.e[i].misc, now.e[i].misc, sizeof was.e[i].misc) == 0);
    CHECK(memcmp(was.rgb, now.rgb, 3) == 0 && memcmp(was.rgb + 3 * 253, now.rgb + 3 * 253, 9) == 0);   /* 0 and the pointer's */
    static uint8_t gw[LUNAR_ART_W * LUNAR_ART_H], gn[LUNAR_ART_W * LUNAR_ART_H];
    grey_of(&was, gw, 0), grey_of(&now, gn, 1);
    LunarRun rw[32], rn2[32];
    const int nw = lunar_title_runs(gw, LUNAR_ART_W, LUNAR_ART_H, rw, 32), nn = lunar_title_runs(gn, LUNAR_ART_W, LUNAR_ART_H, rn2, 32);
    CHECK_EQ_INT(nn, nw);   /* five letters for five: LUNAR for RUSTY */
    CHECK(nn >= 6 && abs(rn2[0].x0 - rw[0].x0) <= 2 && abs(rn2[4].x1 - rw[4].x1) <= 6);   /* where RUSTY was */
    CHECK(nn >= 6 && rn2[5].x0 == rw[5].x0 && rn2[nn - 1].x1 == rw[nw - 1].x1);   /* SPRINGS RACEWAY where they were */
    CHECK(nn >= 2 && rn2[1].x0 - rn2[0].x1 > 3);   /* L's foot is short: the gap after it is wider than after R */
    int changed = 0;
    for (int y = LUNAR_TITLE_Y0; y < LUNAR_TITLE_Y1; y++)
        for (int x = rw[0].x0; x <= rw[4].x1; x++) changed += gn[y * LUNAR_ART_W + x] != gw[y * LUNAR_ART_W + x];
    CHECK(changed > 2000);
    if (kind == LUNAR_ART_SELECT) {   /* the time of day's line: no letter's white left in its rows */
        int white = 0;
        for (int y = 393; y <= 409; y++)
            for (int x = 0; x < 300; x++) white += gn[y * LUNAR_ART_W + x] > 190;
        CHECK_EQ_INT(white, 0);
        int base = 0, worst = 0;   /* nor its shadow: no row of the band darker than the ground under it (German's shadow goes lower) */
        for (int x = 10; x < 280; x++) base += gn[425 * LUNAR_ART_W + x];
        for (int y = 386; y <= 418; y++) {
            int row = 0;
            for (int x = 10; x < 280; x++) row += gn[y * LUNAR_ART_W + x];
            if (abs(row - base) / 270 > worst) worst = abs(row - base) / 270;
        }
        /* measured on the made picture, so in its palette's steps: 5 at version 1 (at the limit), 6 since the title's red ramp took 32
         * of the grey ramp's entries (version 2); the band's fill itself is the same */
        CHECK(worst <= 6);
        if (worst > 6) printf("%s/%s: a row of the time of day's band is %d off the ground\n", dir, file, worst);
    }
    const ShpiDescEntry *bg = entry(&now, "bgnd");
    int coloured = 0, red = 0, letters = 0, letters_red = 0, title_odd = 0;
    for (int i = 0; bg != NULL && i < LUNAR_ART_W * LUNAR_ART_H; i++) {
        const uint8_t *c = now.rgb + 3 * bg->data[i];
        const int y = i / LUNAR_ART_W;
        if (y >= LUNAR_TITLE_Y0 && y < LUNAR_TITLE_Y1) {   /* the title (option A): its letters' white is red, nothing else coloured */
            if (gn[i] > LUNAR_LETTER_WHITE) letters++, letters_red += is_reddish(c);
            title_odd += !is_grey(c) && !is_reddish(c);
            continue;
        }
        if (!is_grey(c)) coloured++, red += c[0] > 150 && c[1] < 60;
    }
    CHECK(coloured > 500 && red == coloured);   /* below the title the only colour is the kept red */
    CHECK(letters > 1000 && letters_red == letters);
    CHECK_EQ_INT(title_odd, 0);
    if (letters_red != letters) printf("%s/%s: %d of the title's %d white pixels are red\n", dir, file, letters_red, letters);
    const ShpiDescEntry *dim = entry(&now, "dond"), *lit = entry(&now, "donl");
    int dim_coloured = 0, lit_yellow = 0;
    for (int i = 0; dim != NULL && i < dim->width * dim->height; i++) dim_coloured += !is_grey(now.rgb + 3 * dim->data[i]);
    for (int i = 0; lit != NULL && i < lit->width * lit->height; i++) lit_yellow += is_yellow(now.rgb + 3 * lit->data[i]);
    CHECK(dim != NULL && lit != NULL && dim_coloured == 0 && lit_yellow > 50);
    printf("the CD's %s/%s: %d letters, %d kept red pixels, DONE lit %d yellow\n", dir, file, nn, red, lit_yellow);
    free(out), free(again), free(rust), free(lost);
}

int main(void)
{
    test_runs();
    test_fill();
    test_refusals();
    check_bank("ART", "RUSTSEL.QFS", LUNAR_ART_SELECT);
    check_bank("ART", "RUSTBST.QFS", LUNAR_ART_BEST);
    check_bank("GART", "RUSTSEL.QFS", LUNAR_ART_SELECT);
    check_bank("GART", "RUSTBST.QFS", LUNAR_ART_BEST);
    return TEST_REPORT();
}
