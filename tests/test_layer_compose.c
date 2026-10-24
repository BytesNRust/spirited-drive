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

int main(void)
{
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
    return TEST_REPORT();
}
