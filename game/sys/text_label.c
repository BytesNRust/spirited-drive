/* port: see text_label.h. */
#include "game/sys/text_label.h"

#include "game/sys/palette.h"
#include "game/sys/text.h"
#include "game/sys/text_soft.h"

#include <stdlib.h>
#include <string.h>

enum { COLOURS = 256, LEVELS = 16, PAD = 2 };

/* the ramps of one colour over the pixels it meets, made when first met */
typedef struct {
    uint8_t known[COLOURS];
    uint8_t ramp[COLOURS][LEVELS];
} Ramps;

static uint8_t mixed(Ramps *r, const uint8_t *palette, int colour, int under, int coverage)
{
    if (coverage >= LEVELS - 1) return (uint8_t)colour;
    if (!r->known[under]) {
        palette_text_ramp(palette, palette_usable(), colour, under, r->ramp[under]);
        r->known[under] = 1;
    }
    return r->ramp[under][coverage];
}

/* the entry nearest the colour `index` times n/16 */
static int shaded(const uint8_t *palette, int index, int n)
{
    const uint8_t *const c = palette + 3 * index;
    return palette_nearest(palette, palette_usable(), c[0] * n / 16, c[1] * n / 16, c[2] * n / 16);
}

void text_label_blit_mask(uint8_t *to, int tw, int th, const uint8_t palette[768], const uint8_t *mask, int w, int h, int x, int y,
                          const TextLabelLook *look)
{
    Ramps *const ramps = calloc(4, sizeof *ramps);   /* the fill's, the grain's, the edge's and the shadow's */
    if (ramps == NULL) return;
    const int grain = look->relief ? shaded(palette, look->fill, 15) : look->fill;
    const int edge = look->relief ? shaded(palette, look->fill, 14) : look->fill;
    for (int pass = look->shadow >= 0 && look->shadow_px > 0 ? 0 : 1; pass < 2; pass++)
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++) {
                const int coverage = mask[j * w + i];
                const int px = x + i + (pass == 0 ? look->shadow_px : 0), py = y + j + (pass == 0 ? look->shadow_px : 0);
                if (coverage == 0 || px < 0 || px >= tw || py < 0 || py >= th) continue;
                uint8_t *const at = &to[py * tw + px];
                if (pass == 0) {
                    *at = mixed(&ramps[3], palette, look->shadow, *at, coverage);
                } else if (look->relief && text_soft_edge(mask, w, h, i, j)) {
                    *at = mixed(&ramps[2], palette, edge, *at, coverage);
                } else if (look->relief && text_soft_grain(px, py)) {
                    *at = mixed(&ramps[1], palette, grain, *at, coverage);
                } else {
                    *at = mixed(&ramps[0], palette, look->fill, *at, coverage);
                }
            }
    free(ramps);
}

int text_label_draw(uint8_t *to, int tw, int th, const uint8_t palette[768], const char *text, int x, int y, int num, int den,
                    const TextLabelLook *look)
{
    const int w = text_width_scaled(text, num, den) + PAD, h = text_mask_rows(text, num, den) + PAD;   /* the pen at the mask's (0, 0): room right and under it for the shadow */
    uint8_t *const mask = h > PAD ? malloc((size_t)w * h) : NULL;
    int ok = 0;
    if (mask != NULL && text_mask_scaled(text, num, den, mask, w, h)) {
        text_label_blit_mask(to, tw, th, palette, mask, w, h, x, y, look);
        ok = 1;
    }
    free(mask);
    return ok;
}
