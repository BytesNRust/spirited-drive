/* dep59 step 3: an added car's card as a shape bank (game/frontend/added_card.h): the card at the CD card's place and size, the rival
 * card at its own, card.png's middle when there is no rival.png; RGBA (alpha ignored) and indexed pictures alike in the screen's palette,
 * never an entry it may not use; a picture of another size refused with its size in the reason (Review Focus 4). */
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

int main(void)
{
    static const uint8_t COLOURS[4][3] = { { 0, 0, 0 }, { 250, 0, 0 }, { 0, 250, 0 }, { 0, 0, 250 } };
    for (int i = 0; i < 4; i++) memcpy(palette + 3 * (40 + i), COLOURS[i], 3), usable[40 + i] = 1;
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
    return TEST_REPORT();
}
