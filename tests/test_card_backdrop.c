/* The card photo's backdrop (card_backdrop.h): a plain colour, a top-to-bottom gradient, or the person's own picture fitted to 309 x 166 (covering it, centred),
 * always opaque. A picture the game cannot read, or in a size no card could use, is refused with the reason. */
#include "card_backdrop.h"
#include "formats/png.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { W = 309, H = 166 };

static uint8_t px[W * H * 4];

static int write_png(const char *path, const uint8_t *rgba, uint32_t w, uint32_t h)
{
    uint8_t *png = NULL;
    size_t len = 0;
    if (png_write_rgba(rgba, w, h, &png, &len) != PNG_OK) return 0;
    FILE *f = fopen(path, "wb");
    const int ok = f != NULL && fwrite(png, 1, len, f) == len;
    if (f != NULL) fclose(f);
    png_free(png);
    return ok;
}

static void test_plain_and_gradient(void)
{
    char why[200];
    CardBackdrop b = card_backdrop_default();
    CHECK(b.kind == CARD_BACKDROP_PLAIN);
    CHECK(card_backdrop_fill(&b, W, H, px, why, sizeof why));
    CHECK(px[0] == b.top[0] && px[1] == b.top[1] && px[2] == b.top[2] && px[3] == 255);
    CHECK(memcmp(px, px + 4 * (W * H - 1), 4) == 0);   /* one colour everywhere */
    b.kind = CARD_BACKDROP_GRADIENT;
    memcpy(b.top, (uint8_t[3]){ 200, 0, 0 }, 3), memcpy(b.bottom, (uint8_t[3]){ 0, 0, 200 }, 3);
    CHECK(card_backdrop_fill(&b, W, H, px, why, sizeof why));
    CHECK(px[0] == 200 && px[2] == 0);   /* the top row is the top colour */
    const uint8_t *last = px + 4 * (size_t)W * (H - 1);
    CHECK(last[0] == 0 && last[2] == 200);   /* the bottom row the bottom one */
    const uint8_t *mid = px + 4 * (size_t)W * (H / 2);
    CHECK(mid[0] > 60 && mid[0] < 140 && mid[2] > 60 && mid[2] < 140);   /* between */
    CHECK(memcmp(px, px + 4 * (W - 1), 4) == 0);   /* a row is one colour */
    for (int i = 0; i < W * H; ++i) CHECK_EQ_INT(px[4 * i + 3], 255);
}

static void test_a_picture(const char *dir)
{
    char why[300], path[600];
    CardBackdrop b = card_backdrop_default();
    b.kind = CARD_BACKDROP_FILE;
    /* a 2:1 picture, red left half, blue right half, 618 x 200: it covers 309 x 166 (scaled to 332 x 166 and the sides cut), the middle is the seam */
    static uint8_t src[618 * 200 * 4];
    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 618; ++x) {
            uint8_t *p = src + 4 * ((size_t)y * 618 + x);
            p[0] = x < 309 ? 255 : 0, p[1] = 0, p[2] = x < 309 ? 0 : 255, p[3] = 255;
        }
    snprintf(path, sizeof path, "%s/back.png", dir);
    CHECK(write_png(path, src, 618, 200));
    snprintf(b.file, sizeof b.file, "%s", path);
    CHECK(card_backdrop_fill(&b, W, H, px, why, sizeof why));
    CHECK(px[0] > 200 && px[2] < 50);   /* the left is red */
    const uint8_t *right = px + 4 * (W - 1);
    CHECK(right[0] < 50 && right[2] > 200);   /* the right blue */
    for (int i = 0; i < W * H; ++i) CHECK_EQ_INT(px[4 * i + 3], 255);
    /* a picture of the card's own size comes through as it is */
    static uint8_t own[W * H * 4];
    for (int i = 0; i < W * H; ++i) own[4 * i] = (uint8_t)i, own[4 * i + 1] = (uint8_t)(i >> 3), own[4 * i + 2] = (uint8_t)(i >> 5), own[4 * i + 3] = 255;
    CHECK(write_png(path, own, W, H));
    CHECK(card_backdrop_fill(&b, W, H, px, why, sizeof why));
    CHECK(memcmp(px, own, sizeof own) == 0);
    /* transparency is composed over the plain colour, never left through */
    for (int i = 0; i < W * H; ++i) own[4 * i + 3] = 0;
    CHECK(write_png(path, own, W, H));
    CHECK(card_backdrop_fill(&b, W, H, px, why, sizeof why));
    CHECK(px[0] == b.top[0] && px[3] == 255);
    /* refused: no file, not a picture, no name */
    snprintf(b.file, sizeof b.file, "%s/nothing.png", dir);
    CHECK(!card_backdrop_fill(&b, W, H, px, why, sizeof why) && strstr(why, "nothing.png") != NULL);
    snprintf(b.file, sizeof b.file, "%s/notpng.png", dir);
    FILE *f = fopen(b.file, "wb");
    if (f != NULL) fputs("not a picture", f), fclose(f);
    CHECK(!card_backdrop_fill(&b, W, H, px, why, sizeof why) && why[0] != '\0');
    b.file[0] = '\0';
    CHECK(!card_backdrop_fill(&b, W, H, px, why, sizeof why));
}

int main(void)
{
    char dir[400];
    snprintf(dir, sizeof dir, "%s/card_backdrop_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(dir) == NULL) return 2;
    test_plain_and_gradient();
    test_a_picture(dir);
    return TEST_REPORT();
}
