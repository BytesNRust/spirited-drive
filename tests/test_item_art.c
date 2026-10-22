/* The Game Mode items' letters for the port's "Customize game" item (item_art.h): the cut from a dark and a lit shape,
 * the setting, and the CD's own letters. */
#include "game/frontend/item_art.h"
#include "formats/qfs.h"
#include "test.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

enum { SHAPE_W = 272, SHAPE_H = 29, SCREEN_W = 640, SCREEN_H = 480 };

/* fnv-1a 64 of the mask the CD gave when the table was made */
#define CUSTOMIZE_GAME_FNV 0x814df80d1e3644b1ULL

/* the synthetic letters: where each one stands in its source (the table's boxes), and its source */
typedef enum { FROM_SINGLE, FROM_MODEM, FROM_NETWORK, FROM_CENTRAL, FROM_TRMTSTND } From;
static const struct { From from; int x0, x1, y0; } LETTERS[] = {
    { FROM_CENTRAL, 9, 29, 66 },     /* C */
    { FROM_TRMTSTND, 54, 69, 70 },   /* u */
    { FROM_MODEM, 130, 146, 5 },     /* s */
    { FROM_NETWORK, 48, 63, 5 },     /* t */
    { FROM_MODEM, 31, 48, 5 },       /* o */
    { FROM_MODEM, 96, 114, 5 },      /* m */
    { FROM_SINGLE, 27, 32, 5 },      /* i */
    { FROM_SINGLE, 104, 118, 5 },    /* z, from the e after it */
    { FROM_SINGLE, 104, 118, 5 },    /* e */
    { FROM_SINGLE, 61, 78, 5 },      /* g, after the word gap */
    { FROM_SINGLE, 184, 201, 5 },    /* a */
    { FROM_MODEM, 96, 114, 5 },      /* m */
    { FROM_SINGLE, 104, 118, 5 },    /* e */
};
enum { N = sizeof LETTERS / sizeof LETTERS[0], WORD_AT = 9 };
static const int WIDTHS[N] = { 21, 16, 17, 16, 18, 19, 6, 15, 15, 18, 18, 19, 15 };

/* the shapes' palette: 0 the background, 1 cream, 2 yellow, 3 and 4 half of cream and of yellow over it, 5 the
 * background one step lighter (a level off the letters on the CD) */
static void shape_palette(uint8_t pal[768])
{
    memset(pal, 0, 768);
    const uint8_t entries[6][3] = { { 120, 100, 70 }, { 238, 214, 182 }, { 255, 206, 0 }, { 179, 157, 126 },
                                    { 187, 153, 35 }, { 121, 100, 70 } };
    memcpy(pal, entries, sizeof entries);
}

static void fill_box(uint8_t *px, int pw, int x0, int x1, int y0, int y1, uint8_t index)
{
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) px[y * pw + x] = index;
}

/* the cut from two synthetic shapes: a letter pixel is cream in the dark one and yellow in the lit one, the
 * background and the half-covered pixels as the shadow would be in both */
static void test_the_letters_are_the_difference_of_the_two_shapes(void)
{
    uint8_t pal[768], dark[6] = { 0, 1, 3, 0, 0, 0 }, lit[6] = { 0, 2, 4, 0, 0, 5 }, m[6];
    shape_palette(pal);
    item_art_mask_from_pair(dark, lit, 6, pal, 0, 0, 5, 0, m);
    CHECK_EQ_INT(m[0], 0);     /* the same background */
    CHECK_EQ_INT(m[1], 15);    /* cream over yellow: all letter */
    CHECK(m[2] >= 7 && m[2] <= 8);   /* half: the two halves of the fill's difference */
    CHECK_EQ_INT(m[3], 0);
    CHECK_EQ_INT(m[4], 0);
    CHECK_EQ_INT(m[5], 0);     /* a level off the background is noise */
}

/* the letters' boxes in the mask's columns: a run per letter, its start and end in runs_out[][2] */
static int runs(const uint8_t *mask, int w, int runs_out[][2], int max)
{
    int n = 0;
    for (int x = 0; x < w; x++) {
        int any = 0;
        for (int y = 0; y < ITEM_ART_H; y++) any |= mask[y * w + x] >= 8;
        if (!any) continue;
        if (n > 0 && runs_out[n - 1][1] == x - 1) runs_out[n - 1][1] = x;
        else if (n < max) runs_out[n][0] = runs_out[n][1] = x, n++;
    }
    return n;
}

/* the synthetic sources: every letter's box filled in its source, cream (dark shape), yellow (lit) or red (title) */
static uint8_t dark_px[3][SHAPE_W * SHAPE_H], lit_px[3][SHAPE_W * SHAPE_H];
/* each title its own screen */
static uint8_t title_px[2][SCREEN_W * SCREEN_H], shape_pal[768], red_pal[768];

static void build_sources(ItemArtSources *s)
{
    memset(dark_px, 0, sizeof dark_px);
    memset(lit_px, 0, sizeof lit_px);
    memset(title_px, 0, sizeof title_px);
    TitleArtScreen *const titles[2] = { &s->central, &s->trmtstnd };
    shape_palette(shape_pal);
    memset(red_pal, 0, sizeof red_pal);
    memcpy(red_pal + 3, (const uint8_t[3]){ 238, 0, 0 }, 3);
    for (int i = 0; i < N; i++) {
        const From f = LETTERS[i].from;
        const int y1 = f >= FROM_CENTRAL ? 84 : 19;
        if (f >= FROM_CENTRAL) {
            fill_box(title_px[f - FROM_CENTRAL], SCREEN_W, LETTERS[i].x0, LETTERS[i].x1, LETTERS[i].y0, y1, 1);
        } else {
            fill_box(dark_px[f], SHAPE_W, LETTERS[i].x0, LETTERS[i].x1, LETTERS[i].y0, y1, 1);
            fill_box(lit_px[f], SHAPE_W, LETTERS[i].x0, LETTERS[i].x1, LETTERS[i].y0, y1, 2);
        }
    }
    for (int f = 0; f < 3; f++) {
        ItemArtShape *const sh = f == 0 ? &s->single : f == 1 ? &s->modem : &s->network;
        sh->dark = dark_px[f], sh->lit = lit_px[f], sh->w = SHAPE_W, sh->h = SHAPE_H, sh->palette = shape_pal;
    }
    for (int t = 0; t < 2; t++) titles[t]->pixels = title_px[t], titles[t]->palette = red_pal;
}

/* Customize game from synthetic letters: 13 letters, each a box of its width, 5 columns apart, the word 26 apart,
 * on the cap rows (the baseline 18 rows down) */
static void test_the_letters_are_set_5_apart_the_word_26(void)
{
    ItemArtSources s;
    build_sources(&s);
    uint8_t *mask = calloc(ITEM_ART_MAX_W * ITEM_ART_H, 1);
    int w = 0, r[N + 1][2];
    CHECK(mask != NULL);
    if (mask == NULL) return;
    CHECK_EQ_INT(item_art_compose(ITEM_ART_CUSTOMIZE_GAME, &s, mask, &w), 1);
    CHECK_EQ_INT(runs(mask, w, r, N + 1), N);
    CHECK_EQ_INT(r[0][0], ITEM_ART_PAD);
    for (int i = 0; i < N; i++) {
        CHECK_EQ_INT(r[i][1] - r[i][0] + 1, WIDTHS[i]);
        if (i > 0) CHECK_EQ_INT(r[i][0] - r[i - 1][1] - 1, i == WORD_AT ? ITEM_ART_WORD_GAP : ITEM_ART_LETTER_GAP);
    }
    CHECK_EQ_INT(w, r[N - 1][1] + 1 + ITEM_ART_PAD);
    CHECK_EQ_INT(w, 300);
    /* the C's column: its cap rows from the top pad, the baseline on the 19th row */
    const int c = r[0][0] + 2;
    CHECK_EQ_INT(mask[(ITEM_ART_PAD - 1) * w + c], 0);
    CHECK_EQ_INT(mask[ITEM_ART_PAD * w + c], 15);
    CHECK_EQ_INT(mask[(ITEM_ART_PAD + ITEM_ART_CAP_H - 1) * w + c], 15);
    CHECK_EQ_INT(mask[(ITEM_ART_PAD + ITEM_ART_CAP_H) * w + c], 0);
    /* a small letter (the u) starts 4 rows under the cap top */
    CHECK_EQ_INT(mask[(ITEM_ART_PAD + 3) * w + r[1][0] + 4], 0);
    CHECK_EQ_INT(mask[(ITEM_ART_PAD + 4) * w + r[1][0] + 4], 15);
    free(mask);
}

/* a source the CD does not give: no item */
static void test_a_missing_source_gives_no_item(void)
{
    ItemArtSources s;
    build_sources(&s);
    s.modem.dark = NULL;
    uint8_t *mask = calloc(ITEM_ART_MAX_W * ITEM_ART_H, 1);
    int w = 0;
    CHECK(mask != NULL);
    if (mask == NULL) return;
    CHECK_EQ_INT(item_art_compose(ITEM_ART_CUSTOMIZE_GAME, &s, mask, &w), 0);
    free(mask);
}

static void test_the_items_by_their_text(void)
{
    CHECK_EQ_INT(item_art_for("Customize game"), ITEM_ART_CUSTOMIZE_GAME);
    CHECK_EQ_INT(item_art_for("Customize Game"), ITEM_ART_COUNT);
    CHECK_EQ_INT(item_art_for("Modem/serial"), ITEM_ART_COUNT);
    CHECK_EQ_INT(item_art_for(NULL), ITEM_ART_COUNT);
}

/* the CD's art file, upper-cased as ISO 9660 has it */
static uint8_t *read_cd(const char *name, size_t *size)
{
    char path[512];
    int n = snprintf(path, sizeof path, "%s/original/FRONTEND/ART/", TNFS_SOURCE_DIR);
    for (const char *c = name; *c && n < (int)sizeof path - 1; c++) path[n++] = (char)toupper((unsigned char)*c);
    path[n] = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    static uint8_t raw[1 << 20];
    const size_t len = fread(raw, 1, sizeof raw, f);
    fclose(f);
    uint8_t *out = NULL;
    if (qfs_decompressed_size(raw, len, size) == QFS_OK && (out = malloc(*size)) != NULL &&
        qfs_decompress(raw, len, out, *size) != QFS_OK)
        free(out), out = NULL;
    return out;
}

static uint64_t fnv(const uint8_t *p, size_t n)
{
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}

/* with the CD at hand: the item's mask as the table was made from it, and its letters */
static void test_the_cd_gives_the_same_item(void)
{
    uint8_t *mask = calloc(ITEM_ART_MAX_W * ITEM_ART_H, 1);
    CHECK(mask != NULL);
    if (mask == NULL) return;
    int w = 0;
    if (!item_art_load(read_cd, ITEM_ART_CUSTOMIZE_GAME, mask, &w)) {
        fprintf(stderr, "test_item_art: no CD data, skipping the CD check\n");
        free(mask);
        return;
    }
    const uint64_t h = fnv(mask, (size_t)w * ITEM_ART_H);
    fprintf(stderr, "test_item_art: w %d fnv %016llx\n", w, (unsigned long long)h);
    CHECK_EQ_INT(w, 300);
    CHECK(h == CUSTOMIZE_GAME_FNV);
    int letter = 0;
    for (int k = 0; k < w * ITEM_ART_H; k++) letter += mask[k] >= 8;
    CHECK(letter > 500);   /* letters, not an empty box */
    free(mask);
}

int main(void)
{
    test_the_letters_are_the_difference_of_the_two_shapes();
    test_the_letters_are_set_5_apart_the_word_26();
    test_a_missing_source_gives_no_item();
    test_the_items_by_their_text();
    test_the_cd_gives_the_same_item();
    return TEST_REPORT();
}
