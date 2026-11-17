/* dep59 (Lunar Springs' entry): Lunar Springs' card in Control Central names the place (game/frontend/lunar_card.h): on made cards, the
 * refusals; and on the CD's own cards (English and German) when original/ is there: "SPRINGS" is after "LUNAR", in the moon label's
 * own colours and on its rows, nothing else of the card changed, and the same cards make the same bytes. */
#include "game/frontend/lunar_card.h"
#include "formats/qfs.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { HDR = 16, W = 60, H = 40, INK = 8, INK_RUSTY = 9, SHADOW = 249, BG = 100 };

static uint8_t pal[768];

static void make_palette(void)
{
    for (int i = 0; i < 256; i++) pal[3 * i] = pal[3 * i + 1] = pal[3 * i + 2] = (uint8_t)(i % 100 + 60);   /* greys, nothing red */
    pal[3 * INK] = 190, pal[3 * INK + 1] = pal[3 * INK + 2] = 8;
    pal[3 * INK_RUSTY] = 238, pal[3 * INK_RUSTY + 1] = pal[3 * INK_RUSTY + 2] = 0;
    pal[3 * SHADOW] = pal[3 * SHADOW + 1] = pal[3 * SHADOW + 2] = 0;
}

static uint8_t *card(int w, int h)
{
    uint8_t *c = calloc(1, (size_t)(HDR + w * h));
    c[0] = 0x7b, c[4] = (uint8_t)w, c[5] = (uint8_t)(w >> 8), c[6] = (uint8_t)h, c[7] = (uint8_t)(h >> 8);
    memset(c + HDR, BG, (size_t)(w * h));
    return c;
}

/* a block of letters x0..x1 (columns), 8 rows from y0, 2 px wide strokes joined (so each pixel has a neighbour), a shadow under and right */
static void word(uint8_t *c, int w, int x0, int x1, int y0, int ink)
{
    uint8_t *px = c + HDR;
    for (int y = y0; y < y0 + 8; y++)
        for (int x = x0; x <= x1; x++) px[y * w + x] = (uint8_t)ink;
    for (int x = x0 + 1; x <= x1 + 1; x++) px[(y0 + 8) * w + x] = SHADOW;
    for (int y = y0 + 1; y < y0 + 8; y++) px[y * w + x1 + 1] = SHADOW;
}

static void test_made(void)
{
    uint8_t *moon = card(W, H), *rusty = card(W, H);
    word(moon, W, 2, 13, 30, INK);                                  /* LUNAR */
    word(rusty, W, 3, 14, 28, INK_RUSTY), word(rusty, W, 22, 40, 28, INK_RUSTY);   /* RUSTY (12 wide), a gap of 7, SPRINGS (19 wide) */
    uint8_t *out = lunar_card_named(moon, rusty, pal);
    CHECK(out != NULL);
    if (out != NULL) {
        const uint8_t *px = out + HDR;
        for (int y = 30; y < 38; y++) {
            CHECK_EQ_INT(px[y * W + 2], INK);                       /* LUNAR kept */
            CHECK_EQ_INT(px[y * W + 13], INK);
            CHECK_EQ_INT(px[y * W + 14], y == 30 ? BG : SHADOW);    /* its shadow, from the second row */
            CHECK_EQ_INT(px[y * W + 18], BG);                       /* the gap: Rusty's 7 columns after the ink, the shadow's in them */
            CHECK_EQ_INT(px[y * W + 21], INK);                      /* SPRINGS (19 wide): the moon's red, not Rusty's */
            CHECK_EQ_INT(px[y * W + 39], INK);
        }
        CHECK_EQ_INT(px[38 * W + 25], SHADOW);                      /* its shadow under it, on the moon's baseline */
        CHECK_EQ_INT(px[29 * W + 25], BG);                          /* nothing above */
        CHECK_MEM(out, moon, HDR);
        for (int i = 0; i < W * H; i++)
            if (moon[HDR + i] != out[HDR + i]) CHECK(i % W >= 21 && i / W >= 30);   /* only right of the first word, on its rows */
        free(out);
    }
    free(moon), free(rusty);
}

static void test_refusals(void)
{
    uint8_t *moon = card(W, H), *rusty = card(W, H), *small = card(W / 2, H);
    word(moon, W, 2, 13, 30, INK);
    CHECK(lunar_card_named(moon, rusty, pal) == NULL);              /* no label on Rusty's */
    word(rusty, W, 3, 14, 28, INK_RUSTY);
    CHECK(lunar_card_named(moon, rusty, pal) == NULL);              /* one word only: no second word */
    word(rusty, W, 22, 40, 28, INK_RUSTY);
    CHECK(lunar_card_named(NULL, rusty, pal) == NULL && lunar_card_named(moon, NULL, pal) == NULL && lunar_card_named(moon, rusty, NULL) == NULL);
    CHECK(lunar_card_named(moon, small, pal) == NULL);              /* another size */
    rusty[0] = 0x22;
    CHECK(lunar_card_named(moon, rusty, pal) == NULL);              /* not a bitmap */
    rusty[0] = 0x7b;
    uint8_t *none = card(W, H);
    CHECK(lunar_card_named(none, rusty, pal) == NULL);              /* no label on the moon's */
    uint8_t *tight = card(W, H);
    word(tight, W, 2, 50, 30, INK);                                 /* a label that leaves no room for the second word */
    CHECK(lunar_card_named(tight, rusty, pal) == NULL);
    uint8_t *two = card(W, H);
    word(two, W, 2, 13, 30, INK), word(two, W, 30, 40, 30, INK);
    CHECK(lunar_card_named(two, rusty, pal) == NULL);               /* a moon card that already names two words */
    uint8_t *huge = card(W, H);
    word(huge, W, 2, 13, 30, INK);
    huge[4] = huge[5] = huge[6] = huge[7] = 0xff;                   /* a header claiming 65535 x 65535 */
    CHECK(lunar_card_named(huge, rusty, pal) == NULL);
    free(moon), free(rusty), free(small), free(none), free(tight), free(two), free(huge);
}

/* a name that ends at the card's right edge and a label on its last rows: the shadow is looked for past the edge, never read there */
static void test_edges(void)
{
    uint8_t *moon = card(W, H), *rusty = card(W, H);
    word(moon, W, 2, 13, 20, INK);
    word(rusty, W, 3, 14, 31, INK_RUSTY), word(rusty, W, 45, 57, 31, INK_RUSTY);   /* the shadow's row is the last (39), SPRINGS ends 2 from the edge */
    uint8_t *out = lunar_card_named(moon, rusty, pal);
    CHECK(out != NULL);
    free(out), free(moon), free(rusty);
}

#ifdef TNFS_SOURCE_DIR
static uint8_t *unpacked(const char *dir, const char *file, size_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/original/FRONTEND/%s/CONTROL/%s", TNFS_SOURCE_DIR, dir, file);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *q = malloc(1u << 20);
    const size_t packed = q != NULL ? fread(q, 1, 1u << 20, f) : 0;
    fclose(f);
    uint8_t *d = NULL;
    *size = 0;
    if (q != NULL && qfs_decompressed_size(q, packed, size) == QFS_OK && (d = malloc(*size)) != NULL &&
        qfs_decompress(q, packed, d, *size) != QFS_OK)
        free(d), d = NULL;
    free(q);
    return d;
}

/* a bank's first bitmap entry (its 16-byte header at the offset the directory gives) and, of central.qfs, the palette entry's data */
static const uint8_t *first_entry(const uint8_t *bank, size_t size, const char *name)
{
    const uint32_t n = (uint32_t)(bank[8] | bank[9] << 8 | bank[10] << 16 | (uint32_t)bank[11] << 24);
    for (uint32_t i = 0; i < n && 16 + 8 * i + 8 <= size; i++)
        if (name == NULL || memcmp(bank + 16 + 8 * i, name, 4) == 0) {
            const uint32_t at = (uint32_t)(bank[20 + 8 * i] | bank[21 + 8 * i] << 8 | bank[22 + 8 * i] << 16 | (uint32_t)bank[23 + 8 * i] << 24);
            return at < size ? bank + at : NULL;
        }
    return NULL;
}

static void check_cd(const char *lang)
{
    size_t cs = 0, ms = 0, rs = 0;
    uint8_t *central = unpacked("GART", "CENTRAL.QFS", &cs), *moon = unpacked("GART", "MOON.QFS", &ms), *rusty = unpacked(lang, "SPRINGSR.QFS", &rs);
    if (central == NULL || moon == NULL || rusty == NULL) {
        fprintf(stderr, "test_lunar_card: no CD (%s): its cards are not checked\n", lang);
        free(central), free(moon), free(rusty);
        return;
    }
    const uint8_t *palette = first_entry(central, cs, "!pal"), *m = first_entry(moon, ms, NULL), *r = first_entry(rusty, rs, NULL);
    CHECK(palette != NULL && m != NULL && r != NULL);
    if (palette != NULL && m != NULL && r != NULL) {
        uint8_t *out = lunar_card_named(m, r, palette + HDR);
        uint8_t *again = lunar_card_named(m, r, palette + HDR);
        CHECK(out != NULL && again != NULL);
        if (out != NULL && again != NULL) {
            const int w = m[4] | m[5] << 8, h = m[6] | m[7] << 8;
            CHECK_MEM(out, again, (size_t)(HDR + w * h));
            int changed = 0, minx = w, miny = h, maxy = 0;
            for (int i = 0; i < w * h; i++)
                if (m[HDR + i] != out[HDR + i]) {
                    changed++;
                    if (i % w < minx) minx = i % w;
                    if (i / w < miny) miny = i / w;
                    if (i / w > maxy) maxy = i / w;
                    CHECK(out[HDR + i] == 8 || out[HDR + i] == 249);        /* the moon label's red and shadow */
                }
            CHECK(changed > 150);                                         /* SPRINGS: seven letters */
            CHECK(minx > 40 && miny >= 110 && maxy <= 124);               /* right of LUNAR (ends at 52), in the label's rows (113..122) */
            CHECK(maxy <= 123);                                           /* SPRINGS' shadow ends where LUNAR's does, a row under its letters (122) */
            fprintf(stderr, "test_lunar_card %s: %d texels, columns from %d, rows %d..%d\n", lang, changed, minx, miny, maxy);
        }
        free(out), free(again);
    }
    free(central), free(moon), free(rusty);
}
#endif

int main(void)
{
    make_palette();
    test_made();
    test_refusals();
    test_edges();
#ifdef TNFS_SOURCE_DIR
    check_cd("ART");
    check_cd("GART");
#endif
    return TEST_REPORT();
}
