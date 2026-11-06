/* The race's 2D layer (640 x 480, LAYER_KEY where nothing was drawn) put over the wide 3D view (layer_compose.h). */
#include "viewer/layer_compose.h"
#include "viewer/screen_layout.h"
#include "test.h"

#include <string.h>

enum { W = 854, H = 480, LW = 640, SCALE_854 = (854 << 16) / 640 };

static uint8_t layer[LW * H], wide[W * H];

static void box(int x0, int y0, int x1, int y1, uint8_t c)
{
    for (int y = y0; y < y1; y++) memset(layer + y * LW + x0, c, (size_t)(x1 - x0));
}

/* port (render bands): layer_compose_rows over unequal bands, in any order, is the whole compose, byte for byte */
static void test_bands_make_the_whole(void)
{
    enum { WIDE_MAX = 1120, PIECES = 3 };
    static uint8_t rnd_layer[LW * H], background[WIDE_MAX * H], whole[WIDE_MAX * H], banded[WIDE_MAX * H];
    uint32_t seed = 12345;
    for (int i = 0; i < LW * H; i++) {
        seed = seed * 1664525u + 1013904223u;
        rnd_layer[i] = (seed >> 24) % 5 == 0 ? LAYER_KEY : (uint8_t)(seed >> 16);
    }
    for (int i = 0; i < WIDE_MAX * H; i++) seed = seed * 1664525u + 1013904223u, background[i] = (uint8_t)(seed >> 8);
    const LayerPiece pieces[PIECES] = { { { 520, 306, 633, 446 }, 700, 330, 0, 0 },
                                        { { 200, 20, 440, 60 }, 150, 10, 1, 0 },
                                        { { 10, 400, 90, 470 }, 30, 390, 0, 90 } };
    static const int widths[3] = { 640, 854, 1120 };
    for (int w = 0; w < 3; w++)
        for (int in_car = 0; in_car < 2; in_car++) {
            const int32_t scale = (int32_t)(((int64_t)widths[w] << 16) / LW);
            const LayerCompose c = { LW, H, widths[w], scale, in_car, pieces, PIECES };
            memcpy(whole, background, sizeof whole);
            layer_compose(&c, rnd_layer, whole);
            memcpy(banded, background, sizeof banded);
            static const int cuts[] = { 0, 7, 8, 150, 151, 333, 480 };
            for (int b = (int)(sizeof cuts / sizeof cuts[0]) - 2; b >= 0; b--)   /* last band first */
                layer_compose_rows(&c, rnd_layer, banded, cuts[b], cuts[b + 1]);
            CHECK_MEM(whole, banded, sizeof whole);
            CHECK(memcmp(whole, background, sizeof whole) != 0);   /* it drew something */
        }
}


/* port (perf): the pieces' mask is set only over the rows a band reads. Pieces reaching past the layer's right and bottom edges (a piece's own draw reads only inside it), bands cut anywhere, a second frame with other pieces after the
 * first on the same thread (a mask row left set would show): every banding is the whole compose, byte for byte */
static void test_mask_rows_follow_the_band(void)
{
    enum { WIDE_MAX = 1120, PIECES = 5 };
    static uint8_t rnd_layer[LW * H], background[WIDE_MAX * H], whole[WIDE_MAX * H], banded[WIDE_MAX * H];
    uint32_t seed = 777;
    for (int i = 0; i < LW * H; i++) {
        seed = seed * 1664525u + 1013904223u;
        rnd_layer[i] = (seed >> 24) % 7 == 0 ? LAYER_KEY : (uint8_t)(seed >> 16);
    }
    for (int i = 0; i < WIDE_MAX * H; i++) seed = seed * 1664525u + 1013904223u, background[i] = (uint8_t)(seed >> 8);
    const LayerPiece sets[2][PIECES] = {
        { { { 0, 0, 30, 10 }, 0, 0, 0, 0 }, { { 600, 460, 700, 500 }, 500, 440, 0, 0 }, { { 100, 100, 100, 200 }, 0, 0, 0, 0 }, { { 0, 0, 640, 12 }, 0, 0, 0, 0 }, { { 300, 470, 340, 480 }, 300, 470, 0, 0 } },
        { { { 20, 150, 220, 151 }, 20, 150, 0, 0 }, { { 400, 0, 410, 480 }, 400, 0, 0, 0 }, { { 7, 7, 8, 8 }, 7, 7, 0, 0 }, { { 630, 300, 700, 400 }, 630, 300, 0, 0 }, { { 0, 479, 640, 700 }, 0, 479, 0, 0 } } };
    static const int widths[3] = { 640, 854, 1120 };
    for (int frame = 0; frame < 2; frame++)
        for (int w = 0; w < 3; w++)
            for (int in_car = 0; in_car < 2; in_car++) {
                const int32_t scale = (int32_t)(((int64_t)widths[w] << 16) / LW);
                const LayerCompose c = { LW, H, widths[w], scale, in_car, sets[frame], PIECES };
                memcpy(whole, background, sizeof whole);
                layer_compose(&c, rnd_layer, whole);
                for (int run = 0; run < 6; run++) {
                    memcpy(banded, background, sizeof banded);
                    int cut = 0;
                    while (cut < H) {   /* bands of 1 .. 120 rows, any size */
                        seed = seed * 1664525u + 1013904223u;
                        const int next = cut + 1 + (int)((seed >> 16) % 120);
                        layer_compose_rows(&c, rnd_layer, banded, cut, next < H ? next : H);
                        cut = next;
                    }
                    CHECK_MEM(whole, banded, sizeof whole);
                }
            }
}

/* port (render scale): the upscale of the layer is every drawn pixel as a scale x scale block, LAYER_KEY leaving the 3D
 * as it was; against the per-pixel definition, for layers of runs of both, odd widths, every scale */
static uint32_t rnd_state = 12345;
static uint32_t rnd(void)
{
    rnd_state = rnd_state * 1664525u + 1013904223u;
    return rnd_state >> 8;
}

static void test_upscale(void)
{
    enum { UW = 203, UH = 60 };
    static uint8_t src[UW * UH], got[UW * UH * 16], want[UW * UH * 16];
    for (int s = 1; s <= 4; s++) {
        for (int y = 0; y < UH; y++) {
            const int mode = (int)(rnd() % 5);   /* all key, all drawn, runs of both, scattered */
            for (int x = 0; x < UW;) {
                const int run = mode == 4 ? 1 : 1 + (int)(rnd() % 40);
                const int key = mode == 0 || (mode != 1 && (rnd() & 1));
                for (int i = 0; i < run && x < UW; i++, x++) src[y * UW + x] = key ? LAYER_KEY : (uint8_t)(rnd() % 255);
            }
        }
        const size_t n = (size_t)UW * UH * s * s;
        for (size_t i = 0; i < n; i++) got[i] = want[i] = (uint8_t)rnd();
        for (int y = 0; y < UH; y++)
            for (int x = 0; x < UW; x++)
                if (src[y * UW + x] != LAYER_KEY)
                    for (int j = 0; j < s; j++)
                        memset(want + ((size_t)(y * s + j) * UW + x) * s, src[y * UW + x], (size_t)s);
        layer_upscale_rows(src, UW, s, got, 0, 25);   /* in two bands */
        layer_upscale_rows(src, UW, s, got, 25, UH);
        CHECK(memcmp(got, want, n) == 0);
    }
}

int main(void)
{
    test_upscale();
    /* a dash pixel, a piece, and nothing drawn elsewhere */
    memset(layer, LAYER_KEY, sizeof layer);
    box(300, 400, 340, 440, 7);         /* the dash */
    box(520, 306, 633, 446, 9);         /* a piece (the map) */
    memset(wide, 3, sizeof wide);       /* the 3D view */
    const LayerPiece pieces[1] = { { { 520, 306, 633, 446 }, 854 - 113 - 6, 480 - 140 - 6, 0, 0 } };
    box(100, HUD_BAR_HEIGHT, 101, HUD_BAR_HEIGHT + 1, 4);   /* the roof's first line, under the scoreboard */
    const LayerCompose c = { LW, H, W, SCALE_854, 1, pieces, 1 };
    layer_compose(&c, layer, wide);
    /* the 3D stays where the layer drew nothing */
    CHECK_EQ_INT(wide[100 * W + 10], 3);
    /* the dash at its own size in the middle (screen_dash_x / _y): layer (320, 410) -> wide (427, 410 + DASH_DROP) */
    CHECK_EQ_INT(wide[(410 + DASH_DROP) * W + 427], 7);
    CHECK_EQ_INT(wide[(400 + DASH_DROP - 1) * W + 427], 3);
    /* the roof's first line over the lines above the drop, where its column lands stretched */
    int roof_x = 0;
    while (screen_dash_x(W, roof_x) < 100) roof_x++;
    CHECK_EQ_INT(wide[0 * W + roof_x], 4);
    CHECK_EQ_INT(wide[(HUD_BAR_HEIGHT + DASH_DROP) * W + roof_x], 4);
    CHECK_EQ_INT(wide[(HUD_BAR_HEIGHT + DASH_DROP + 1) * W + roof_x], 3);
    /* the piece unscaled at its anchor, its old place in the enlarged dash not drawn */
    CHECK_EQ_INT(wide[(480 - 140 - 6) * W + (854 - 113 - 6)], 9);
    CHECK_EQ_INT(wide[(480 - 6 - 1) * W + (854 - 6 - 1)], 9);
    CHECK_EQ_INT(wide[(480 - 140 - 6 - 1) * W + (854 - 113 - 6 - 1)], 3);

    /* outside the car: no scaling, what is not a piece lands centred */
    memset(wide, 3, sizeof wide);
    const LayerCompose out = { LW, H, W, 1 << 16, 0, pieces, 1 };
    layer_compose(&out, layer, wide);
    CHECK_EQ_INT(wide[420 * W + 320 + (W - LW) / 2], 7);
    CHECK_EQ_INT(wide[420 * W + 320], 3);
    CHECK_EQ_INT(wide[(480 - 140 - 6) * W + (854 - 113 - 6)], 9);

    /* a scaled piece (the mirror): its pixels enlarged at the anchor */
    memset(layer, LAYER_KEY, sizeof layer);
    box(543, 53, 633, 104, 5);
    memset(wide, 3, sizeof wide);
    const LayerPiece mirror[1] = { { { 543, 53, 633, 104 }, 700, 38, 1, 0 } };
    const LayerCompose m = { LW, H, W, SCALE_854, 1, mirror, 1 };
    layer_compose(&m, layer, wide);
    CHECK_EQ_INT(wide[38 * W + 700], 5);
    const int mw = (633 - 543) * 854 / 640;
    CHECK_EQ_INT(wide[38 * W + 700 + mw - 1], 5);
    CHECK_EQ_INT(wide[38 * W + 700 + mw + 1], 3);

    /* a scaled piece told its width (the mirror's right half, to the mirror's end): one column past its own scaled
     * width, that column its last pixel's, not the view behind */
    memset(wide, 3, sizeof wide);
    const LayerPiece to_end[1] = { { { 543, 53, 633, 104 }, 700, 38, 1, mw + 1 } };
    const LayerCompose e = { LW, H, W, SCALE_854, 1, to_end, 1 };
    layer_compose(&e, layer, wide);
    CHECK_EQ_INT(wide[38 * W + 700 + mw], 5);
    CHECK_EQ_INT(wide[38 * W + 700 + mw + 1], 3);

    /* at 640 with scale 1 and the pieces where they are, the layer over the view as it is */
    static uint8_t same[LW * H];
    memset(same, 3, sizeof same);
    memset(layer, LAYER_KEY, sizeof layer);
    box(300, 400, 340, 440, 7);
    const LayerPiece here[1] = { { { 520, 306, 633, 446 }, 520, 306, 0, 0 } };
    const LayerCompose id = { LW, H, LW, 1 << 16, 1, here, 1 };
    layer_compose(&id, layer, same);
    CHECK_EQ_INT(same[420 * LW + 320], 7);
    CHECK_EQ_INT(same[10 * LW + 10], 3);

    /* the map's see-through back grabbed at its wide anchor: what the in-car frame shows there, the windshield
     * (LAYER_KEY) left as it was */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < LW; x++) layer[y * LW + x] = (uint8_t)(10 + (x * 7 + y * 13) % 120);
    box(560, 300, 600, 330, LAYER_KEY);
    memset(wide, 3, sizeof wide);
    const LayerCompose dash = { LW, H, W, SCALE_854, 1, NULL, 0 };
    layer_compose(&dash, layer, wide);
    enum { BW = 113, BH = 140, BX = 854 - 113 - 6 + 2 - 40, BY = 480 - 140 - 6 + 2 - 60 };
    static uint8_t back[BW * BH];
    memset(back, 200, sizeof back);
    layer_dash_grab(layer, W, BX, BY, back, BW, BH);
    int same_as_frame = 1, holes = 0;
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++) {
            const uint8_t shown = wide[(BY + y) * W + BX + x], got = back[y * BW + x];
            if (shown == 3) holes += got == 200;
            else same_as_frame &= got == shown;
        }
    CHECK(same_as_frame);
    CHECK(holes > 0);
    test_bands_make_the_whole();
    test_mask_rows_follow_the_band();
    return TEST_REPORT();
}
