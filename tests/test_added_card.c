/* dep59 step 3: an added car's card as a shape bank (game/frontend/added_card.h): the card at the CD card's place and size, the rival
 * card at its own, card.png's middle when there is no rival.png; RGBA (alpha ignored) and indexed pictures alike in the screen's palette,
 * never an entry it may not use; a picture of another size refused with its size in the reason (Review Focus 4). Section 1 of
 * notes/dep59-added-gallery-showcase-plan.md: the car's name drawn over the card (red, a black shadow 1 px down-right, its capitals
 * ending on the CD cards' row, from x 4), on the rival card too, cut with "..." when it does not fit. */
#include "game/frontend/added_card.h"
#include "game/sys/shape_bank.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static uint8_t palette[768], usable[256];

static PngImage rgba(int w, int h, uint8_t alpha)
{
    PngImage img;
    memset(&img, 0, sizeof img);
    img.width = (uint32_t)w, img.height = (uint32_t)h;
    img.pixels = malloc((size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) {   /* left half red, right half green; the middle row band blue */
        const int x = i % w, y = i / w;
        uint8_t *p = img.pixels + 4 * i;
        p[0] = x < w / 2 ? 250 : 0, p[1] = x < w / 2 ? 0 : 250, p[2] = (y >= h / 2 - 2 && y < h / 2 + 2) ? 250 : 0, p[3] = alpha;
        if (p[2]) p[0] = p[1] = 0;
    }
    return img;
}

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* the bank's "0000": its size, place and pixels */
static const uint8_t *card_of(uint8_t *bank, size_t size, int *w, int *h, int *x, int *y)
{
    if (bank == NULL || !shape_bank_valid(bank, size)) return NULL;
    const uint8_t *s = shape_bank_find(bank, (const uint8_t *)"0000");
    if (s == NULL) return NULL;
    *w = le16(s + 4), *h = le16(s + 6), *x = (int16_t)le16(s + 12), *y = (int16_t)le16(s + 14);
    return s + 16;
}

/* ---- the card's name ---- */

static int measure_10(void *ctx, const char *s)
{
    (void)ctx;
    return 10 * (int)strlen(s);   /* every character 10 px */
}

static void test_name_fit(void)
{
    char out[64];
    CHECK(added_card_name_fit("ABCDEFGHIJ", 100, measure_10, NULL, out, sizeof out) && strcmp(out, "ABCDEFGHIJ") == 0);
    CHECK(added_card_name_fit("ABCDEFGHIJ", 80, measure_10, NULL, out, sizeof out) && strcmp(out, "ABCDE...") == 0);
    CHECK(added_card_name_fit("ABC DEFGH", 70, measure_10, NULL, out, sizeof out) && strcmp(out, "ABC...") == 0);   /* no space before it */
    CHECK(added_card_name_fit("  Viper  ", 300, measure_10, NULL, out, sizeof out) && strcmp(out, "Viper") == 0);     /* trimmed */
    CHECK(added_card_name_fit("Caf\xc3\xa9 Racer", 300, measure_10, NULL, out, sizeof out) && strcmp(out, "Caf Racer") == 0);   /* text_words */
    CHECK(!added_card_name_fit("", 300, measure_10, NULL, out, sizeof out) && out[0] == '\0');
    CHECK(!added_card_name_fit("\xc3\xa9", 300, measure_10, NULL, out, sizeof out));   /* nothing the face draws */
    CHECK(!added_card_name_fit("ABCDEFGH", 20, measure_10, NULL, out, sizeof out));    /* not even "..." fits */
    CHECK(added_card_name_fit("ABCDEFGHIJ", 1000, measure_10, NULL, out, 6) && strcmp(out, "AB...") == 0);   /* out's size cuts too */
}

static int index_of(const uint8_t *rgb)
{
    for (int i = 0; i < 256; i++)
        if (usable[i] && memcmp(palette + 3 * i, rgb, 3) == 0) return i;
    return -1;
}

/* a mask of 10 x 6 whole letters, its capitals' last row 5, on a grey card: red where it covers, black 1 px down-right of it and
 * not 2, the grey elsewhere; the capitals' last row on `base`, from x 4 */
static void check_name_on(int rival, int from_card)
{
    static uint8_t mask[10 * 6];
    memset(mask, 15, sizeof mask);
    const AddedCardName name = { mask, 10, 6, 5 };
    const int w = from_card || !rival ? ADDED_CARD_W : ADDED_RIVAL_W, h = from_card || !rival ? ADDED_CARD_H : ADDED_RIVAL_H;
    PngImage img;
    memset(&img, 0, sizeof img);
    img.width = (uint32_t)w, img.height = (uint32_t)h;
    img.pixels = malloc((size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) img.pixels[4 * i] = img.pixels[4 * i + 1] = img.pixels[4 * i + 2] = 128, img.pixels[4 * i + 3] = 255;
    char why[160];
    size_t size = 0;
    int cw, ch, cx, cy;
    uint8_t *bank = added_card_bank_named(&img, rival, from_card, &name, palette, usable, &size, why, sizeof why);
    const uint8_t *px = card_of(bank, size, &cw, &ch, &cx, &cy);
    CHECK(px != NULL);
    if (px != NULL) {
        const int base = rival ? ADDED_RIVAL_NAME_BASE : ADDED_CARD_NAME_BASE, x0 = ADDED_CARD_NAME_X, y0 = base - 5;
        const int red = index_of((const uint8_t[]){ 238, 0, 0 }), black = index_of((const uint8_t[]){ 0, 0, 0 }),
                  grey = index_of((const uint8_t[]){ 128, 128, 128 });
        CHECK(red >= 0 && black >= 0 && grey >= 0);
        CHECK_EQ_INT(px[y0 * cw + x0], red);
        CHECK_EQ_INT(px[base * cw + x0 + 9], red);
        CHECK_EQ_INT(px[(base + 1) * cw + x0 + 10], black);   /* the shadow, 1 px down-right */
        CHECK_EQ_INT(px[(base + 1) * cw + x0 + 1], black);
        CHECK_EQ_INT(px[(base + 2) * cw + x0 + 11], grey);    /* not 2 */
        CHECK_EQ_INT(px[(y0 - 1) * cw + x0], grey);           /* nothing above, nor left */
        CHECK_EQ_INT(px[y0 * cw + x0 - 1], grey);
        CHECK_EQ_INT(px[10 * cw + 100], grey);
    }
    free(bank), free(img.pixels);
}

static void test_name_drawn(void)
{
    check_name_on(0, 0);
    check_name_on(1, 0);
    check_name_on(1, 1);   /* card.png's middle as the rival card: the name at the rival card's place, not cut off */
    /* coverage 8 of 15: between the picture and the red */
    static uint8_t half[4 * 4];
    memset(half, 8, sizeof half);
    const AddedCardName name = { half, 4, 4, 3 };
    static uint8_t rgba_px[ADDED_CARD_W * ADDED_CARD_H * 4];
    for (int i = 0; i < ADDED_CARD_W * ADDED_CARD_H; i++) rgba_px[4 * i] = rgba_px[4 * i + 1] = rgba_px[4 * i + 2] = 128, rgba_px[4 * i + 3] = 255;
    added_card_draw_name(rgba_px, ADDED_CARD_W, ADDED_CARD_H, &name, ADDED_CARD_NAME_BASE);
    const uint8_t *p = rgba_px + 4 * ((ADDED_CARD_NAME_BASE - 3) * ADDED_CARD_W + ADDED_CARD_NAME_X + 1);
    CHECK(p[0] > 150 && p[0] < 230 && p[1] > 20 && p[1] < 110 && p[2] == p[1]);
    /* a mask past the card's right edge: cut, nothing written outside */
    static uint8_t wide[400 * 2];
    memset(wide, 15, sizeof wide);
    const AddedCardName long_name = { wide, 400, 2, 1 };
    added_card_draw_name(rgba_px, ADDED_CARD_W, ADDED_CARD_H, &long_name, ADDED_CARD_NAME_BASE);
    CHECK(rgba_px[4 * (ADDED_CARD_NAME_BASE * ADDED_CARD_W + ADDED_CARD_W - 1)] == 238);
}

int main(void)
{
    static const uint8_t COLOURS[4][3] = { { 0, 0, 0 }, { 250, 0, 0 }, { 0, 250, 0 }, { 0, 0, 250 } };
    for (int i = 0; i < 4; i++) memcpy(palette + 3 * (40 + i), COLOURS[i], 3), usable[40 + i] = 1;
    memcpy(palette + 3 * 50, (const uint8_t[]){ 238, 0, 0 }, 3), usable[50] = 1;      /* the names' red */
    memcpy(palette + 3 * 51, (const uint8_t[]){ 128, 128, 128 }, 3), usable[51] = 1;  /* a grey card */
    memcpy(palette + 3 * 0xfe, (const uint8_t[]){ 250, 0, 0 }, 3);   /* the pointer's: the same red, never taken */
    char why[160];
    size_t size = 0;
    int w, h, x, y;

    PngImage card = rgba(ADDED_CARD_W, ADDED_CARD_H, 255);
    uint8_t *bank = added_card_bank(&card, 0, 0, palette, usable, &size, why, sizeof why);
    const uint8_t *px = card_of(bank, size, &w, &h, &x, &y);
    CHECK(px != NULL);
    if (px != NULL) {
        CHECK(w == ADDED_CARD_W && h == ADDED_CARD_H && x == ADDED_CARD_X && y == ADDED_CARD_Y);
        CHECK_EQ_INT(px[0], 41);                                     /* red, from 41 and never 0xfe */
        CHECK_EQ_INT(px[ADDED_CARD_W - 1], 42);                      /* green */
        CHECK_EQ_INT(px[(ADDED_CARD_H / 2) * ADDED_CARD_W + 5], 43); /* blue */
    }
    free(bank);

    bank = added_card_bank(&card, 1, 1, palette, usable, &size, why, sizeof why);   /* no rival.png: card.png's middle */
    px = card_of(bank, size, &w, &h, &x, &y);
    CHECK(px != NULL);
    if (px != NULL) {
        CHECK(w == ADDED_RIVAL_W && h == ADDED_RIVAL_H && x == ADDED_RIVAL_X && y == ADDED_RIVAL_Y);
        const int band = ADDED_RIVAL_H / 2;   /* the card's middle band is in the middle of the crop */
        CHECK_EQ_INT(px[band * ADDED_RIVAL_W + 3], 43);
        CHECK_EQ_INT(px[0], 41);
    }
    free(bank);
    free(card.pixels);

    PngImage rival = rgba(ADDED_RIVAL_W, ADDED_RIVAL_H, 0);   /* alpha 0 everywhere: ignored, drawn */
    bank = added_card_bank(&rival, 1, 0, palette, usable, &size, why, sizeof why);
    px = card_of(bank, size, &w, &h, &x, &y);
    CHECK(px != NULL && px[0] == 41);
    free(bank);
    free(rival.pixels);

    PngImage indexed;   /* an indexed picture through its own palette */
    memset(&indexed, 0, sizeof indexed);
    indexed.indexed = 1, indexed.width = ADDED_CARD_W, indexed.height = ADDED_CARD_H;
    indexed.pixels = calloc((size_t)ADDED_CARD_W * ADDED_CARD_H, 1);
    memcpy(indexed.palette + 3 * 7, (const uint8_t[]){ 0, 240, 10 }, 3);
    memset(indexed.pixels, 7, (size_t)ADDED_CARD_W * ADDED_CARD_H);
    bank = added_card_bank(&indexed, 0, 0, palette, usable, &size, why, sizeof why);
    px = card_of(bank, size, &w, &h, &x, &y);
    CHECK(px != NULL && px[0] == 42);
    free(bank);
    free(indexed.pixels);

    PngImage wide = rgba(ADDED_CARD_W + 1, ADDED_CARD_H, 255);   /* Review Focus 4: 310x166 refused, its size said */
    why[0] = '\0';
    CHECK(added_card_bank(&wide, 0, 0, palette, usable, &size, why, sizeof why) == NULL);
    CHECK(strstr(why, "310x166") != NULL && strstr(why, "309x166") != NULL);
    CHECK(added_card_bank(&wide, 1, 1, palette, usable, &size, why, sizeof why) == NULL);   /* nor its middle */
    free(wide.pixels);
    PngImage small = rgba(ADDED_RIVAL_W, ADDED_RIVAL_H + 1, 255);
    CHECK(added_card_bank(&small, 1, 0, palette, usable, &size, why, sizeof why) == NULL);
    CHECK(strstr(why, "284x118") != NULL);
    free(small.pixels);
    test_name_fit();
    test_name_drawn();
    return TEST_REPORT();
}
