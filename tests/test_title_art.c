/* The port's screen titles in the original titles' letters (title_art.h): the cut-out, the setting, the small z. */
#include "game/frontend/title_art.h"
#include "formats/qfs.h"
#include "test.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

enum { W = TITLE_ART_SCREEN_W, H = TITLE_ART_SCREEN_H };

/* fnv-1a 64 of the two titles' masks the CD gave when the table was made */
#define CUSTOMIZE_GAME_FNV 0xdafba61af5ab30d0ULL
#define GRAPHICS_DETAIL_FNV 0x9f13941b31291516ULL
/* "Controls Setup" cut from option/cntrlset.qfs: its packed width and the mask's hash when the table was made */
#define CONTROLS_SETUP_W (3 + 286 + 3)
#define CONTROLS_SETUP_FNV 0x406d4df5344cd64eULL

static const uint8_t RED[3] = { 238, 0, 0 }, GREEN[3] = { 0, 172, 8 };

/* the palette's colours: grey + k/100 of the face's colour less its grey, rounded */
static void tint(uint8_t pal[768], int index, const uint8_t face[3], int grey, int hundredths)
{
    const int sum = face[0] + face[1] + face[2];
    for (int c = 0; c < 3; c++) {
        const int d = 3 * face[c] - sum, v = grey + (d * hundredths + (d * hundredths >= 0 ? 150 : -150)) / 300;
        pal[3 * index + c] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
}

static void rgb(uint8_t pal[768], int index, int r, int g, int b)
{
    pal[3 * index] = (uint8_t)r, pal[3 * index + 1] = (uint8_t)g, pal[3 * index + 2] = (uint8_t)b;
}

/* the title's parts as the CD has them: the face and its dark edge are letter, the rest is not */
static void test_the_letters_are_cut_from_what_is_round_them(void)
{
    uint8_t pal[768] = { 0 }, px[8], m[8];
    rgb(pal, 0, 238, 0, 0);      /* the face */
    rgb(pal, 1, 121, 0, 0);      /* its darker right edge: about half */
    rgb(pal, 2, 149, 133, 113);  /* the textured background */
    rgb(pal, 3, 16, 16, 8);      /* the hard shadow */
    rgb(pal, 4, 56, 8, 8);       /* the shadow over red */
    rgb(pal, 5, 230, 222, 214);  /* the pale rim */
    rgb(pal, 6, 246, 80, 80);    /* the face blended with the rim */
    rgb(pal, 7, 174, 165, 149);
    for (int i = 0; i < 8; i++) px[i] = (uint8_t)i;
    title_art_mask_from_screen(px, 8, pal, RED, 0, 0, 7, 0, m);
    CHECK_EQ_INT(m[0], 15);
    CHECK_EQ_INT(m[1], 8);
    CHECK_EQ_INT(m[2], 0);
    CHECK_EQ_INT(m[3], 0);
    CHECK_EQ_INT(m[4], 0);
    CHECK_EQ_INT(m[5], 0);
    CHECK_EQ_INT(m[6], 15);
    CHECK_EQ_INT(m[7], 0);
}

/* only the box is cut, row by row */
static void test_the_box_is_cut(void)
{
    uint8_t pal[768] = { 0 }, px[4 * 3] = { 0 }, m[2 * 2];
    rgb(pal, 1, 238, 0, 0);
    px[1 * 4 + 2] = 1;   /* (2, 1) */
    title_art_mask_from_screen(px, 4, pal, RED, 1, 1, 2, 2, m);
    CHECK_EQ_INT(m[0], 0);
    CHECK_EQ_INT(m[1], 15);
    CHECK_EQ_INT(m[2], 0);
    CHECK_EQ_INT(m[3], 0);
}

/* a green title and a red one with the same parts give the same mask */
static void test_the_cut_does_not_depend_on_the_hue(void)
{
    static const int hundredths[] = { 0, 20, 43, 60, 80, 100, -30 };
    enum { N = sizeof hundredths / sizeof hundredths[0] };
    uint8_t red_pal[768] = { 0 }, green_pal[768] = { 0 }, px[N], red[N], green[N];
    for (int i = 0; i < N; i++) {
        tint(red_pal, i, RED, 80, hundredths[i]);
        tint(green_pal, i, GREEN, 80, hundredths[i]);
        px[i] = (uint8_t)i;
    }
    title_art_mask_from_screen(px, N, red_pal, RED, 0, 0, N - 1, 0, red);
    title_art_mask_from_screen(px, N, green_pal, GREEN, 0, 0, N - 1, 0, green);
    for (int i = 0; i < N; i++) CHECK_EQ_INT(green[i], red[i]);
    CHECK_EQ_INT(red[0], 0);
    CHECK_EQ_INT(red[1], 0);    /* 0.2: under the ramp */
    CHECK_EQ_INT(red[2], 5);    /* 0.43: (0.43 - 0.3) / 0.4 * 15 = 4.875 */
    CHECK_EQ_INT(red[3], 11);   /* 0.6: 11.25 */
    CHECK_EQ_INT(red[4], 15);
    CHECK_EQ_INT(red[5], 15);
    CHECK_EQ_INT(red[6], 0);    /* the opposite hue */
}

/* the column runs of mask rows that hold letter: starts and ends in runs[][2], their count returned */
static int runs(const uint8_t *mask, int w, int runs_out[][2], int max)
{
    int n = 0;
    for (int x = 0; x < w; x++) {
        int any = 0;
        for (int y = 0; y < TITLE_ART_H; y++) any |= mask[y * w + x] != 0;
        if (!any) continue;
        if (n > 0 && runs_out[n - 1][1] == x - 1) runs_out[n - 1][1] = x;
        else if (n < max) runs_out[n][0] = runs_out[n][1] = x, n++;
    }
    return n;
}

/* every source screen in its title's colour over the titles' rows: each letter is its box, cut 1 px wider all round */
static uint8_t *screen_px;
static uint8_t red_pal[768], green_pal[768];

static void solid_screens(TitleArtScreen screens[TITLE_ART_SOURCES])
{
    memset(screen_px, 0, (size_t)W * H);
    memset(screen_px + (size_t)TITLE_ART_TOP * W, 1, (size_t)(TITLE_ART_BASELINE - TITLE_ART_TOP + 1) * W);
    rgb(red_pal, 1, 238, 0, 0);
    rgb(green_pal, 1, 0, 172, 8);
    for (int s = 0; s < TITLE_ART_SOURCES; s++)
        screens[s].pixels = screen_px, screens[s].palette = s == TITLE_ART_GRAPHICS_QFS ? green_pal : red_pal;
}

/* Customize Game: the letters 4 columns apart, the words 23, from x 9 */
static void test_the_letters_are_set_4_apart_the_words_23(void)
{
    /* C u s t o m i z e, Game: their boxes' widths */
    static const int widths[] = { 21, 16, 16, 16, 18, 18, 5, 15, 15, 87 };
    enum { N = sizeof widths / sizeof widths[0] };
    TitleArtScreen screens[TITLE_ART_SOURCES];
    solid_screens(screens);
    uint8_t *mask = calloc(TITLE_ART_MAX_W * TITLE_ART_H, 1);
    int w = 0, r[N + 1][2];
    CHECK(mask != NULL);
    if (mask == NULL) return;
    CHECK_EQ_INT(title_art_compose(TITLE_ART_CUSTOMIZE_GAME, screens, mask, &w), 1);
    /* one run of columns per letter, its box with the cut's 1 px pad either side */
    CHECK_EQ_INT(runs(mask, w, r, N + 1), N);
    CHECK_EQ_INT(r[0][0] + 1, TITLE_ART_PAD);   /* the first letter at x 9 */
    for (int i = 0; i < N; i++) {
        CHECK_EQ_INT(r[i][1] - r[i][0] - 1, widths[i]);
        if (i > 0) CHECK_EQ_INT((r[i][0] + 1) - (r[i - 1][1] - 1) - 1, i == N - 1 ? TITLE_ART_WORD_GAP : TITLE_ART_LETTER_GAP);
    }
    CHECK_EQ_INT(w, r[N - 1][1] + TITLE_ART_PAD);
    /* the letters' rows: the capitals' height, the pad rows empty */
    CHECK_EQ_INT(mask[(TITLE_ART_PAD - 1) * w + TITLE_ART_PAD], 0);
    CHECK_EQ_INT(mask[TITLE_ART_PAD * w + TITLE_ART_PAD], 15);
    CHECK_EQ_INT(mask[(TITLE_ART_H - TITLE_ART_PAD - 1) * w + TITLE_ART_PAD], 15);
    CHECK_EQ_INT(mask[(TITLE_ART_H - TITLE_ART_PAD) * w + TITLE_ART_PAD], 0);
    free(mask);
}

/* Graphics Detail keeps its own letters where they stand */
static void test_graphics_detail_is_the_whole_title(void)
{
    TitleArtScreen screens[TITLE_ART_SOURCES];
    solid_screens(screens);
    uint8_t *mask = calloc(TITLE_ART_MAX_W * TITLE_ART_H, 1);
    int w = 0, r[4][2];
    CHECK(mask != NULL);
    if (mask == NULL) return;
    CHECK_EQ_INT(title_art_compose(TITLE_ART_GRAPHICS_DETAIL, screens, mask, &w), 1);
    CHECK_EQ_INT(w, 301 - 9 + 1 + 2 * TITLE_ART_PAD);
    CHECK_EQ_INT(runs(mask, w, r, 4), 1);
    CHECK_EQ_INT(r[0][0], TITLE_ART_PAD - 1);
    CHECK_EQ_INT(r[0][1], w - TITLE_ART_PAD);
    free(mask);
}

/* a source that is not there: no title */
static void test_a_missing_source_gives_no_title(void)
{
    TitleArtScreen screens[TITLE_ART_SOURCES];
    solid_screens(screens);
    screens[TITLE_ART_TRMTSTND_QFS].pixels = NULL;
    uint8_t *mask = calloc(TITLE_ART_MAX_W * TITLE_ART_H, 1);
    int w = 0;
    CHECK(mask != NULL);
    if (mask == NULL) return;
    CHECK_EQ_INT(title_art_compose(TITLE_ART_CUSTOMIZE_GAME, screens, mask, &w), 0);
    CHECK_EQ_INT(title_art_compose(TITLE_ART_GRAPHICS_DETAIL, screens, mask, &w), 1);
    free(mask);
}

/* the leftmost and rightmost letter column of row y, -1 when empty */
static void row_span(const uint8_t *m, int w, int y, int *left, int *right)
{
    *left = *right = -1;
    for (int x = 0; x < w; x++)
        if (m[y * w + x] >= 8) {
            if (*left < 0) *left = x;
            *right = x;
        }
}

/* the small z: the e's size, its bars full width, a diagonal from top right to bottom left in between */
static void test_the_small_z(void)
{
    enum { EW = 17, EH = 17 };   /* a 15 x 15 e cut with 1 px all round */
    uint8_t e[EW * EH] = { 0 }, z[EW * EH];
    for (int y = 1; y <= 15; y++)
        for (int x = 1; x <= 15; x++)
            if (y <= 4 || y >= 12 || x <= 4 || (y >= 7 && y <= 9 && x <= 11)) e[y * EW + x] = 15;   /* an E */
    title_art_small_z(e, EW, EH, z);
    int left, right, top = -1, bottom = -1, l0 = EW, r0 = -1;
    for (int y = 0; y < EH; y++) {
        row_span(z, EW, y, &left, &right);
        if (left < 0) continue;
        if (top < 0) top = y;
        bottom = y;
        if (left < l0) l0 = left;
        if (right > r0) r0 = right;
    }
    CHECK_EQ_INT(top, 1);
    CHECK_EQ_INT(bottom, 15);
    CHECK_EQ_INT(l0, 1);
    CHECK_EQ_INT(r0, 15);
    row_span(z, EW, 2, &left, &right);
    CHECK(left == 1 && right == 15);   /* the top bar */
    row_span(z, EW, 14, &left, &right);
    CHECK(left == 1 && right == 15);   /* the bottom bar */
    int l5, r5, l11, r11;
    row_span(z, EW, 5, &l5, &r5);
    row_span(z, EW, 11, &l11, &r11);
    CHECK(r5 - l5 >= 3 && r5 - l5 <= 6);     /* a stroke about the e's stem wide */
    CHECK(r11 - l11 >= 3 && r11 - l11 <= 6);
    CHECK(l5 > l11 + 2);                     /* falling to the left */
    int partial = 0;
    for (int x = 0; x < EW; x++) partial += z[7 * EW + x] > 0 && z[7 * EW + x] < 15;
    CHECK(partial > 0);                      /* its edges anti-aliased */
    /* nothing of the E's middle bar */
    CHECK_EQ_INT(z[8 * EW + 2], 0);
}

static void test_the_titles_by_their_text(void)
{
    CHECK_EQ_INT(title_art_for("Customize Game"), TITLE_ART_CUSTOMIZE_GAME);
    CHECK_EQ_INT(title_art_for("Graphics Detail"), TITLE_ART_GRAPHICS_DETAIL);
    CHECK_EQ_INT(title_art_for("Game Mode"), TITLE_ART_GAME_MODE);
    CHECK_EQ_INT(title_art_for("Controls Setup"), TITLE_ART_CONTROLS_SETUP);
    CHECK_EQ_INT(title_art_for("Audio"), TITLE_ART_COUNT);
    CHECK_EQ_INT(title_art_for(NULL), TITLE_ART_COUNT);
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

/* with the CD at hand: both titles, their sizes and the same masks as when the table was made */
static void test_the_cd_gives_the_same_titles(void)
{
    static const struct { TitleArtId id; int w; uint64_t fnv; } titles[] = {
        { TITLE_ART_CUSTOMIZE_GAME, 3 + 282 + 3, CUSTOMIZE_GAME_FNV },
        { TITLE_ART_GRAPHICS_DETAIL, 3 + 293 + 3, GRAPHICS_DETAIL_FNV },
        { TITLE_ART_CONTROLS_SETUP, CONTROLS_SETUP_W, CONTROLS_SETUP_FNV },
    };
    uint8_t *mask = calloc(TITLE_ART_MAX_W * TITLE_ART_H, 1);
    CHECK(mask != NULL);
    if (mask == NULL) return;
    for (size_t i = 0; i < sizeof titles / sizeof titles[0]; i++) {
        int w = 0;
        if (!title_art_load(read_cd, titles[i].id, mask, &w)) {
            fprintf(stderr, "test_title_art: no CD data, skipping the CD check\n");
            break;
        }
        const uint64_t h = fnv(mask, (size_t)w * TITLE_ART_H);
        fprintf(stderr, "test_title_art: title %d w %d fnv %016llx\n", (int)titles[i].id, w, (unsigned long long)h);
        CHECK_EQ_INT(w, titles[i].w);
        CHECK(h == titles[i].fnv);
        int letter = 0;
        for (int k = 0; k < w * TITLE_ART_H; k++) letter += mask[k] >= 8;
        CHECK(letter > 1000);   /* letters, not an empty box */
    }
    free(mask);
}

int main(void)
{
    screen_px = malloc((size_t)W * H);
    if (screen_px == NULL) return 1;
    test_the_letters_are_cut_from_what_is_round_them();
    test_the_box_is_cut();
    test_the_cut_does_not_depend_on_the_hue();
    test_the_letters_are_set_4_apart_the_words_23();
    test_graphics_detail_is_the_whole_title();
    test_a_missing_source_gives_no_title();
    test_the_small_z();
    test_the_titles_by_their_text();
    test_the_cd_gives_the_same_titles();
    free(screen_px);
    return TEST_REPORT();
}
