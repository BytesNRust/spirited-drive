/* dep59 step 3: an added car's card in Control Central's palette (game/frontend/picture_palette.h): each pixel the nearest of the
 * entries the screen lets it use, by squared distance, alpha ignored, never an entry it may not use. */
#include "game/frontend/picture_palette.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

/* dep59 (the owner's): the card dithered, Floyd-Steinberg in the palette's 8 bits a channel. A colour the palette has stays that entry
 * everywhere; a grey between black and white comes out as a mix of the two whose mean is the grey; never an entry it may not use; a
 * colour outside what the entries reach does not run away (the error is kept within a channel's 0..255) */
static void test_dither(void)
{
    enum { W = 32, H = 32 };
    uint8_t palette[768] = { 0 }, usable[256] = { 0 };
    memcpy(palette + 3 * 20, (const uint8_t[]){ 0, 0, 0 }, 3), usable[20] = 1;
    memcpy(palette + 3 * 21, (const uint8_t[]){ 255, 255, 255 }, 3), usable[21] = 1;
    memcpy(palette + 3 * 22, (const uint8_t[]){ 200, 40, 30 }, 3), usable[22] = 1;
    memcpy(palette + 3 * 200, (const uint8_t[]){ 128, 128, 128 }, 3);   /* the grey itself, but not usable */
    static uint8_t rgba[W * H * 4], out[W * H];
    for (int i = 0; i < W * H; i++) memcpy(rgba + 4 * i, (const uint8_t[]){ 200, 40, 30, 255 }, 4);
    picture_palette_dither(rgba, W, H, palette, usable, out);
    int same = 1;
    for (int i = 0; i < W * H; i++) same &= out[i] == 22;
    CHECK(same);
    /* the grey with black and white usable (the red would be nearer than either: 22532 against 48387) */
    uint8_t bw[256] = { 0 };
    bw[20] = bw[21] = 1;
    for (int i = 0; i < W * H; i++) memcpy(rgba + 4 * i, (const uint8_t[]){ 128, 128, 128, 255 }, 4);
    picture_palette_dither(rgba, W, H, palette, bw, out);
    long sum = 0;
    int whites = 0, ok = 1;
    for (int i = 0; i < W * H; i++) ok &= bw[out[i]] != 0, sum += palette[3 * out[i]], whites += out[i] == 21;
    CHECK(ok);
    CHECK(labs(sum / (W * H) - 128) <= 4);
    CHECK(whites > W * H / 3 && whites < 2 * W * H / 3);   /* a mix, not all one: nearest gives every pixel white */
    uint8_t nearest[W * H];
    picture_palette_nearest(rgba, W * H, palette, bw, nearest);
    CHECK_EQ_INT(nearest[0], 21);
    /* only black usable, the picture white: every pixel black, nothing past it */
    uint8_t one[256] = { 0 };
    one[20] = 1;
    for (int i = 0; i < W * H; i++) memcpy(rgba + 4 * i, (const uint8_t[]){ 255, 255, 255, 255 }, 4);
    picture_palette_dither(rgba, W, H, palette, one, out);
    same = 1;
    for (int i = 0; i < W * H; i++) same &= out[i] == 20;
    CHECK(same);
    uint8_t none[256] = { 0 };
    picture_palette_dither(rgba, W, H, palette, none, out);
    CHECK_EQ_INT(out[0], 0);
    CHECK_EQ_INT(out[W * H - 1], 0);
}

int main(void)
{
    test_dither();
    uint8_t palette[768] = { 0 }, usable[256] = { 0 };
    static const uint8_t COLOURS[4][3] = { { 0, 0, 0 }, { 255, 0, 0 }, { 0, 255, 0 }, { 250, 250, 250 } };
    for (int i = 0; i < 4; i++) memcpy(palette + 3 * (10 + i), COLOURS[i], 3), usable[10 + i] = 1;
    memcpy(palette + 3 * 200, (const uint8_t[]){ 255, 255, 255 }, 3);   /* pure white, but not usable (a pointer colour) */
    const uint8_t rgba[] = {
        0, 0, 0, 255,        /* black */
        250, 10, 10, 255,    /* near red */
        10, 240, 20, 0,      /* near green, alpha 0: alpha ignored */
        255, 255, 255, 255,  /* white: the usable near white, never 200 */
        128, 128, 128, 255,  /* grey: near white (3 * 122^2) is nearer than black (3 * 128^2) */
    };
    uint8_t out[5];
    picture_palette_nearest(rgba, 5, palette, usable, out);
    CHECK_EQ_INT(out[0], 10);
    CHECK_EQ_INT(out[1], 11);
    CHECK_EQ_INT(out[2], 12);
    CHECK_EQ_INT(out[3], 13);
    CHECK_EQ_INT(out[4], 13);
    for (int i = 0; i < 5; i++) CHECK(usable[out[i]]);
    /* every colour of a cube maps to a usable entry, and each to the nearest */
    for (int r = 0; r < 256; r += 51)
        for (int g = 0; g < 256; g += 51)
            for (int b = 0; b < 256; b += 51) {
                const uint8_t px[4] = { (uint8_t)r, (uint8_t)g, (uint8_t)b, 255 };
                uint8_t got;
                picture_palette_nearest(px, 1, palette, usable, &got);
                CHECK(usable[got]);
                long best = -1;
                for (int k = 0; k < 256; k++) {
                    if (!usable[k]) continue;
                    const long dr = r - palette[3 * k], dg = g - palette[3 * k + 1], db = b - palette[3 * k + 2];
                    const long d = dr * dr + dg * dg + db * db;
                    if (best < 0 || d < best) best = d;
                }
                const long dr = r - palette[3 * got], dg = g - palette[3 * got + 1], db = b - palette[3 * got + 2];
                CHECK(dr * dr + dg * dg + db * db == best);
            }
    uint8_t none[256] = { 0 }, zero = 7;   /* nothing usable: entry 0, never past the palette */
    picture_palette_nearest(rgba, 1, palette, none, &zero);
    CHECK_EQ_INT(zero, 0);
    return TEST_REPORT();
}
