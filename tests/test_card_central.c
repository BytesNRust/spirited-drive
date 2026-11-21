/* «As on the card»: what the game does to a car's card.png on Control Central (added_card_glue.c): the picture dithered into the screen's palette (the "!pal" of
 * FRONTEND/ART/CONTROL/CENTRAL.QFS through palette_put), every entry but the pointer's 0xfe and the see-through 0xff. Synthetic palettes need no game data; with the
 * CD in original/ (skipped without) the real palette is read. */
#include "card_central.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { W = 16, H = 8 };

static void test_the_prediction(void)
{
    uint8_t pal[768] = { 0 };
    for (int i = 0; i < 256; ++i) pal[3 * i] = (uint8_t)i, pal[3 * i + 1] = (uint8_t)(255 - i), pal[3 * i + 2] = (uint8_t)(i / 2);
    uint8_t in[W * H * 4], out[W * H * 4];
    for (int i = 0; i < W * H; ++i) {   /* exactly the colours of palette entries 10..: they come back as themselves, opaque */
        const int e = 10 + i % 7;
        in[4 * i] = pal[3 * e], in[4 * i + 1] = pal[3 * e + 1], in[4 * i + 2] = pal[3 * e + 2], in[4 * i + 3] = 255;
    }
    card_central_as_drawn(in, W, H, pal, out);
    int same = 1;
    for (int i = 0; i < W * H; ++i) same = same && memcmp(in + 4 * i, out + 4 * i, 4) == 0;
    CHECK(same);
    /* the pointer's colour (0xfe) and the see-through one (0xff) are never used: a picture of exactly those comes out as other entries */
    for (int i = 0; i < W * H; ++i) memcpy(in + 4 * i, (i & 1) ? pal + 3 * 0xfe : pal + 3 * 0xff, 3), in[4 * i + 3] = 255;
    card_central_as_drawn(in, W, H, pal, out);
    int used = 0;
    for (int i = 0; i < W * H; ++i)
        for (int e = 0xfe; e <= 0xff; ++e) used += out[4 * i] == pal[3 * e] && out[4 * i + 1] == pal[3 * e + 1] && out[4 * i + 2] == pal[3 * e + 2];
    CHECK_EQ_INT(used, 0);
    /* a colour the palette lacks is a mix of the ones it has (dithered), and the picture stays opaque */
    for (int i = 0; i < W * H; ++i) in[4 * i] = 128, in[4 * i + 1] = 128, in[4 * i + 2] = 40, in[4 * i + 3] = 255;
    card_central_as_drawn(in, W, H, pal, out);
    int distinct = 0;
    for (int i = 1; i < W * H; ++i) distinct += memcmp(out, out + 4 * i, 3) != 0;
    CHECK(distinct > 0);
    for (int i = 0; i < W * H; ++i) CHECK_EQ_INT(out[4 * i + 3], 255);
}

static void test_the_cds_palette(void)
{
    const char *cd = getenv("TNFS_CD");
    char dir[600];
    snprintf(dir, sizeof dir, "%s", cd != NULL ? cd : TNFS_SOURCE_DIR "/original");
    uint8_t pal[768];
    char why[300] = "";
    if (!card_central_palette(dir, pal, why, sizeof why)) {
        fprintf(stderr, "skip: no Control Central art in %s (%s)\n", dir, why);
        return;
    }
    int lit = 0;
    for (int i = 0; i < 768; ++i) lit += pal[i] != 0;
    CHECK(lit > 300);   /* a real palette, 8 bits a channel */
    int bright = 0;
    for (int i = 0; i < 768; ++i) bright += pal[i] > 63;
    CHECK(bright > 0);   /* shifted from the file's 6 bits when its type says so (palette_put) */
    CHECK(!card_central_palette("/nonexistent-cd", pal, why, sizeof why) && why[0] != '\0');
}

int main(void)
{
    test_the_prediction();
    test_the_cds_palette();
    return TEST_REPORT();
}
