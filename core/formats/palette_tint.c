/* See palette_tint.h. */
#include "formats/palette_tint.h"

#include "formats/fixed_math.h"

enum {
    TINT_SHIFT_DIVIDE = 0x100,   /* the channel scales are 8.8 */
    SATURATION_MAX = 0x10000,
    CHANNEL_MAX = 0x3f
};

static int32_t scaled(uint8_t v, int32_t *scale)
{
    if (*scale < 0)
        *scale = 0;
    return (int32_t)((uint32_t)v * (uint32_t)*scale) / TINT_SHIFT_DIVIDE;
}

static int32_t channel(int32_t v)
{
    return v < 0 ? 0 : v > CHANNEL_MAX ? CHANNEL_MAX : v;
}

void palette_tint_colours(PaletteTint *t, int32_t first, int32_t end, uint8_t *colours)
{
    for (int32_t i = first; i < end; i++, colours += 3) {
        int32_t c[3];
        c[0] = scaled(colours[0], &t->red);
        c[1] = scaled(colours[1], &t->green);
        c[2] = scaled(colours[2], &t->blue);
        if (t->saturation > SATURATION_MAX)
            t->saturation = SATURATION_MAX;
        const int32_t red_green = c[0] > c[1] ? c[0] : c[1];
        const int32_t top = red_green > c[2] ? red_green : c[2];
        for (int k = 0; k < 3; k++)
            if (top - c[k] != 0)
                c[k] += fx_mul_round(top - c[k], t->saturation);
        for (int k = 0; k < 3; k++)
            colours[k] = (uint8_t)channel(fx_mul_round(c[k], t->brightness));
    }
}
