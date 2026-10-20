/* The race's 2D layer (640 x 480, LAYER_KEY where nothing was drawn) put over the wide 3D view (layer_compose.h). */
#include "viewer/layer_compose.h"
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
    const LayerPiece pieces[1] = { { { 520, 306, 633, 446 }, 854 - 113 - 6, 480 - 140 - 6, 0 } };
    const LayerCompose c = { LW, H, W, SCALE_854, 80, 1, pieces, 1 };
    layer_compose(&c, layer, wide);
    /* the 3D stays where the layer drew nothing */
    CHECK_EQ_INT(wide[10 * W + 10], 3);
    /* the dash enlarged by 854 / 640 and cropped 80 lines at the top: layer (320, 410) -> wide (427, 467) */
    const int wx = 320 * 854 / 640, wy = (int)(((int64_t)410 * SCALE_854 >> 16) - 80);
    CHECK_EQ_INT(wy, 467);
    CHECK_EQ_INT(wide[wy * W + wx], 7);
    /* the piece unscaled at its anchor, its old place in the enlarged dash not drawn */
    CHECK_EQ_INT(wide[(480 - 140 - 6) * W + (854 - 113 - 6)], 9);
    CHECK_EQ_INT(wide[(480 - 6 - 1) * W + (854 - 6 - 1)], 9);
    CHECK_EQ_INT(wide[(480 - 140 - 6 - 1) * W + (854 - 113 - 6 - 1)], 3);

    /* outside the car: no scaling, what is not a piece lands centred */
    memset(wide, 3, sizeof wide);
    const LayerCompose out = { LW, H, W, 1 << 16, 0, 0, pieces, 1 };
    layer_compose(&out, layer, wide);
    CHECK_EQ_INT(wide[420 * W + 320 + (W - LW) / 2], 7);
    CHECK_EQ_INT(wide[420 * W + 320], 3);
    CHECK_EQ_INT(wide[(480 - 140 - 6) * W + (854 - 113 - 6)], 9);

    /* a scaled piece (the mirror): its pixels enlarged at the anchor */
    memset(layer, LAYER_KEY, sizeof layer);
    box(543, 53, 633, 104, 5);
    memset(wide, 3, sizeof wide);
    const LayerPiece mirror[1] = { { { 543, 53, 633, 104 }, 700, 38, 1 } };
    const LayerCompose m = { LW, H, W, SCALE_854, 80, 1, mirror, 1 };
    layer_compose(&m, layer, wide);
    CHECK_EQ_INT(wide[38 * W + 700], 5);
    const int mw = (633 - 543) * 854 / 640;
    CHECK_EQ_INT(wide[38 * W + 700 + mw - 1], 5);
    CHECK_EQ_INT(wide[38 * W + 700 + mw + 1], 3);

    /* at 640 with scale 1 and the pieces where they are, the layer over the view as it is */
    static uint8_t same[LW * H];
    memset(same, 3, sizeof same);
    memset(layer, LAYER_KEY, sizeof layer);
    box(300, 400, 340, 440, 7);
    const LayerPiece here[1] = { { { 520, 306, 633, 446 }, 520, 306, 0 } };
    const LayerCompose id = { LW, H, LW, 1 << 16, 0, 1, here, 1 };
    layer_compose(&id, layer, same);
    CHECK_EQ_INT(same[420 * LW + 320], 7);
    CHECK_EQ_INT(same[10 * LW + 10], 3);
    return TEST_REPORT();
}
