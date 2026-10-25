/* port: EXTRAS' Texture Filter on the flat horizon (horizon_smooth.h): the bitmap's texels blended between their
 * centres as it is magnified, each blend the palette's nearest colour; a texel's own colour kept where all four
 * neighbours are it, a 4x4 ordered dither on the blends. Without it the exe's band (horizon.c). */
#include "viewer/horizon_smooth.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { W = 640, H = 480, BW = 640, BH = 8 };

static uint8_t screen[W * H];
static uint8_t texels[BW * BH];
static uint8_t palette[768];

/* colour 0 black, 1 white, 2..33 the greys between them (every 8th level), the rest a far red */
static void make_palette(void)
{
    for (int i = 0; i < 256; i++) palette[3 * i] = 200, palette[3 * i + 1] = 0, palette[3 * i + 2] = 0;
    palette[0] = palette[1] = palette[2] = 0;
    palette[3] = palette[4] = palette[5] = 255;
    for (int g = 0; g < 32; g++) palette[3 * (2 + g)] = palette[3 * (2 + g) + 1] = palette[3 * (2 + g) + 2] = (uint8_t)(g * 8 + 4);
}

static int is_grey_between(uint8_t c) { return c >= 2 && c < 34; }

static const FshEntry *bitmap(void)
{
    static FshEntry e;
    e = (FshEntry){ { 0 }, FSH_TYPE_BITMAP, 0, BW, BH, { 0, 4, 0, 0 }, texels, sizeof texels };
    return &e;
}

static void draw(const HorizonSmooth *s)
{
    static HorizonTables t;
    horizon_tables(&t, W, 90, 0x10000);
    const HorizonTarget target = { screen, W };
    const ClipRect clip = { 0, 0, W, H };
    memset(screen, 0xee, sizeof screen);
    horizon_draw_flat_smooth(s, &t, &target, &clip, bitmap(), 0, 0, 0, 200);
}

static void test_nearest(void)
{
    make_palette();
    static HorizonSmooth s;
    horizon_smooth_palette(&s, palette);
    CHECK_EQ_INT(horizon_smooth_nearest(&s, 20, 20, 20), 2 + 2);       /* grey 20 */
    CHECK_EQ_INT(horizon_smooth_nearest(&s, 130, 130, 130), 2 + 16);   /* grey 132 */
    CHECK_EQ_INT(horizon_smooth_nearest(&s, 200, 0, 0), 34);           /* the first of the reds */
}

/* the top half black, the bottom white: a run of greys where they meet, each row the same across (the dither's
 * 4-pixel pattern) */
static void test_rows_blend(void)
{
    make_palette();
    static HorizonSmooth s;
    horizon_smooth_palette(&s, palette);
    for (int y = 0; y < BH; y++) memset(texels + y * BW, y < BH / 2 ? 0 : 1, BW);
    draw(&s);
    int greys = 0, black = 0, white = 0, last = -1, rising = 1;
    for (int y = 0; y < H; y++) {
        const uint8_t c = screen[y * W + W / 2];
        if (c == 0xee) continue;
        greys += is_grey_between(c), black += c == 0, white += c == 1;
        const int level = c == 0 ? 0 : c == 1 ? 1000 : is_grey_between(c) ? c : -1;
        CHECK(level >= 0);
        if (level < last) rising = 0;
        last = level;
        for (int x = 4; x < W; x++) CHECK(screen[y * W + x] == screen[y * W + x - 4]);   /* the dither's period */
    }
    CHECK(greys >= 2);   /* the exe's band: none */
    CHECK(black > 0 && white > 0);
    CHECK(rising);       /* black, the greys lighter and lighter, white */
}

/* a texel whose neighbours are all it: its own colour, though the palette holds a nearer one at 5 bits */
static void test_uniform_kept(void)
{
    make_palette();
    palette[3 * 40] = 101, palette[3 * 40 + 1] = 102, palette[3 * 40 + 2] = 103;
    palette[3 * 41] = 100, palette[3 * 41 + 1] = 100, palette[3 * 41 + 2] = 100;
    static HorizonSmooth s;
    horizon_smooth_palette(&s, palette);
    memset(texels, 40, sizeof texels);
    draw(&s);
    int other = 0;
    for (int i = 0; i < W * H; i++) other += screen[i] != 40 && screen[i] != 0xee;
    CHECK_EQ_INT(other, 0);
}

/* the panorama wraps: columns left and right of the bitmap read its other end on the same row (the exe reads the
 * row beside it), so a bitmap of one colour per row draws the same pattern across the seam */
static void test_wrap_same_row(void)
{
    make_palette();
    static HorizonSmooth s;
    horizon_smooth_palette(&s, palette);
    for (int y = 0; y < BH; y++) memset(texels + y * BW, 2 + 4 * y, BW);
    static HorizonTables t;
    horizon_tables(&t, W, 90, 0x10000);
    const HorizonTarget target = { screen, W };
    const ClipRect clip = { 0, 0, W, H };
    memset(screen, 0xee, sizeof screen);
    horizon_draw_flat_smooth(&s, &t, &target, &clip, bitmap(), 0x780000, 0, 0, 200);   /* x offset 600: the seam in view */
    for (int y = 0; y < H; y++)
        for (int x = 4; x < W; x++) CHECK(screen[y * W + x] == screen[y * W + x - 4]);
}

/* the PS1 sky: texels index a palette of their own, the screen gets the target palette's nearest colours */
static void test_sky_palettes(void)
{
    make_palette();
    uint8_t decode[768];
    memset(decode, 0, sizeof decode);
    decode[0] = decode[1] = decode[2] = 100;   /* the bitmap's colour 0: grey 100, which the target has as colour 2 + 12 */
    static HorizonSmooth s;
    horizon_smooth_sky(&s, decode, palette);
    memset(texels, 0, sizeof texels);
    CHECK_EQ_INT(horizon_smooth_texel(&s, bitmap(), 5 << 16, 3 << 16, 7, 9), 2 + 12);
    draw(&s);
    CHECK_EQ_INT(screen[200 * W + 100], 2 + 12);   /* a texel's own colour, through the map */
    CHECK_EQ_INT(screen[200 * W + 640 - 1], 2 + 12);
}

int main(void)
{
    test_nearest();
    test_rows_blend();
    test_uniform_kept();
    test_wrap_same_row();
    test_sky_palettes();
    return TEST_REPORT();
}
