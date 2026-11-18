/* port (dep59 step 5, T7d): see track_page_layout.h. */
#include "game/frontend/track_page_layout.h"

#include "formats/showcase_tint.h"

#include <string.h>

enum { BOX_ALPHA = 215, SHADOW_ALPHA = 150, EDGE_RGB = 0x7a654a, INNER_RGB = 0x3a2c1e, FILL_RGB = 0x0a0704, FRAME_RGB = 0x7a654a };

/* the pixels x0..x1, y0..y1 (ends included, clipped) mixed with `rgb` by alpha/255 */
static void mix(uint8_t *out, int x0, int y0, int x1, int y1, unsigned rgb, int alpha)
{
    const uint8_t c[3] = { (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb };
    for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < TPL_H; y++)
        for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < TPL_W; x++)
            for (int k = 0; k < 3; k++) {
                uint8_t *p = out + 3 * (y * TPL_W + x) + k;
                *p = (uint8_t)((*p * (255 - alpha) + c[k] * alpha) / 255);
            }
}

static void edge(uint8_t *out, int x0, int y0, int x1, int y1, int width, unsigned rgb)
{
    mix(out, x0, y0, x1, y0 + width - 1, rgb, 255), mix(out, x0, y1 - width + 1, x1, y1, rgb, 255);
    mix(out, x0, y0, x0 + width - 1, y1, rgb, 255), mix(out, x1 - width + 1, y0, x1, y1, rgb, 255);
}

void track_page_compose(const uint8_t *bgnd, const uint8_t *card, const uint8_t tint[3], uint8_t *out)
{
    memcpy(out, bgnd, (size_t)TPL_W * TPL_H * 3);
    if (tint != NULL) showcase_tint_apply(out, (size_t)TPL_W * TPL_H, tint);
    for (int y = 0; y < TPL_BAND_H; y++)   /* the band's texture mirrored, so it blends in */
        for (int x = 0; x < TPL_BAND_W; x++)
            memcpy(out + 3 * (y * TPL_W + x), out + 3 * (y * TPL_W + TPL_BAND_FROM_X + TPL_BAND_W - 1 - x), 3);
    mix(out, TPL_BOX_X0 + TPL_SHADOW, TPL_BOX_Y0 + TPL_SHADOW, TPL_BOX_X1 + TPL_SHADOW, TPL_BOX_Y1 + TPL_SHADOW, 0, SHADOW_ALPHA);
    mix(out, TPL_BOX_X0, TPL_BOX_Y0, TPL_BOX_X1, TPL_BOX_Y1, FILL_RGB, BOX_ALPHA);
    edge(out, TPL_BOX_X0, TPL_BOX_Y0, TPL_BOX_X1, TPL_BOX_Y1, 2, EDGE_RGB);
    edge(out, TPL_BOX_X0 + 5, TPL_BOX_Y0 + 5, TPL_BOX_X1 - 5, TPL_BOX_Y1 - 5, 1, INNER_RGB);
    edge(out, TPL_CARD_X - 2, TPL_CARD_Y - 2, TPL_CARD_X + TPL_CARD_W + 1, TPL_CARD_Y + TPL_CARD_H + 1, 2, FRAME_RGB);
    for (int y = 0; y < TPL_CARD_H; y++)
        memcpy(out + 3 * ((TPL_CARD_Y + y) * TPL_W + TPL_CARD_X), card + 3 * y * TPL_CARD_W, (size_t)TPL_CARD_W * 3);
    mix(out, TPL_BOX_X0 + 20, TPL_RULE_Y, TPL_BOX_X1 - 20, TPL_RULE_Y, INNER_RGB, 255);   /* under the length: the description's rule */
}
