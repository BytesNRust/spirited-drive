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

static int distance(const uint8_t *palette, int index, const int want[3])
{
    const uint8_t *const c = palette + 3 * index;
    return (c[0] - want[0]) * (c[0] - want[0]) + (c[1] - want[1]) * (c[1] - want[1]) + (c[2] - want[2]) * (c[2] - want[2]);
}

/* the pixel `under` with `colour` over it at `coverage` (0..15): the ramp's step, unless the palette has no step near the true mix (a photo's
 * quantized palette: its nearest entry may be a dark speck), and then the colour or the pixel, whichever is nearer the mix */
static uint8_t mixed(Ramps *r, const uint8_t *palette, int colour, int under, int coverage)
{
    if (coverage >= LEVELS - 1) return (uint8_t)colour;
    if (!r->known[under]) {
        palette_text_ramp(palette, palette_usable(), colour, under, r->ramp[under]);
        r->known[under] = 1;
    }
    const uint8_t *const f = palette + 3 * colour, *const u = palette + 3 * under;
    const int want[3] = { (f[0] * coverage + u[0] * (LEVELS - 1 - coverage)) / (LEVELS - 1), (f[1] * coverage + u[1] * (LEVELS - 1 - coverage)) / (LEVELS - 1),
                          (f[2] * coverage + u[2] * (LEVELS - 1 - coverage)) / (LEVELS - 1) };
    int best = r->ramp[under][coverage];
    enum { NEAR = 1200 };   /* the largest squared RGB distance still taken as the mix */
    if (distance(palette, best, want) > NEAR) {
        best = distance(palette, colour, want) <= distance(palette, under, want) ? colour : under;
    }
    return (uint8_t)best;
}

/* the entry nearest the colour `index` times n/16, or `index` itself when the palette has nothing near it (a photo's quantized palette
 * may hold no shade of the label's colour, and its nearest entry would be a dark speck in the letters) */
static int shaded(const uint8_t *palette, int index, int n)
{
    enum { NEAR = 1200 };   /* the largest squared RGB distance still taken for the same colour, a little */
    const uint8_t *const c = palette + 3 * index;
    const int r = c[0] * n / 16, g = c[1] * n / 16, b = c[2] * n / 16;
    const int found = palette_nearest(palette, palette_usable(), r, g, b);
    const uint8_t *const f = palette + 3 * found;
    const int d = (f[0] - r) * (f[0] - r) + (f[1] - g) * (f[1] - g) + (f[2] - b) * (f[2] - b);
    return d <= NEAR ? found : index;
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
