/* The race's screen palette (race_palette.h). */
#include "game/race/race_palette.h"

#include "game/sys/fixed_math.h"
#include "game/sys/palette.h"

#include <stddef.h>

enum {
    CAR_COLOURS = 0xfa,          /* 0048f220(0, 0xfa, ...): the rest stay as they are */
    LIGHTS_FIRST = 0xfa,         /* the police lights, 0xfa..0xfd */
    LIGHTS = 4,
    OWN_COLOUR = 0xfe,
    POLICE_SLOT = 2,
    BANK_COUNT = 8,              /* SHPI: shapes at +8, then name and offset pairs from +0x10 */
    BANK_DIRECTORY = 0x10,
    BANK_ENTRY = 8,
    SHAPE_WIDTH = 4,             /* words, signed */
    SHAPE_HEIGHT = 6,
    SHAPE_PIXELS = 0x10,
    HIDDEN_MARK = '!',
    PALETTE_SCREEN_BITS = 8,     /* 00476460: 004c6130 <= 8 */
    OPAQUE = 0xff,
    TINT_SHIFT_DIVIDE = 0x100,   /* the channel scales are 8.8 */
    SATURATION_MAX = 0x10000,
    CHANNEL_MAX = 0x3f
};

static uint32_t le32(const uint8_t *b)
{
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

static int32_t le16s(const uint8_t *b)
{
    return (int16_t)(uint16_t)((uint32_t)b[0] | (uint32_t)b[1] << 8);
}

void race_palette_table(RacePalette *p, int first, int count, const uint8_t *rgb)
{
    const PaletteMatch m = { p->inverse, p->screen, p->enabled };
    for (int i = first; i < first + count; i++, rgb += 3) {
        const uint32_t packed = (uint32_t)rgb[2] | (uint32_t)rgb[1] << 8 | (uint32_t)rgb[0] << 16;
        const int colour = palette_match_rgb(&m, packed);
        if (i >= 0 && i < RACE_PALETTE_COLOURS)
            p->table[i] = (uint8_t)colour;
    }
}

void race_palette_remap_bank(RacePalette *p, uint8_t *bank, const uint8_t *palette6, int32_t slot, int32_t police)
{
    uint8_t rgb[RACE_PALETTE_COLOURS * 3];
    palette_6_to_8(RACE_PALETTE_COLOURS, palette6, rgb);
    race_palette_table(p, 0, CAR_COLOURS, rgb);
    const uint8_t own = (uint32_t)slot < RACE_PALETTE_SLOTS ? p->slot_colours[slot] : 0;
    if (own != 0) {
        p->unknown_50ad2e[LIGHTS] = own;
        p->table[OWN_COLOUR] = own;
    }
    if (police != 0 && slot == POLICE_SLOT)
        for (int k = 0; k < LIGHTS; k++) {
            p->unknown_50ad2e[k] = (uint8_t)(LIGHTS_FIRST + k);
            p->table[LIGHTS_FIRST + k] = (uint8_t)(LIGHTS_FIRST + k);
        }
    for (uint32_t i = le32(bank + BANK_COUNT); i-- > 0;) {
        const uint8_t *entry = bank + BANK_DIRECTORY + BANK_ENTRY * i;
        if (entry[0] == HIDDEN_MARK)
            continue;
        uint8_t *shape = bank + le32(entry + 4);
        const uint32_t pixels = (uint32_t)le16s(shape + SHAPE_WIDTH) * (uint32_t)le16s(shape + SHAPE_HEIGHT);
        palette_remap_pixels(shape + SHAPE_PIXELS, p->table, pixels);
    }
}

void race_palette_queue(RacePalette *p, int32_t first, int32_t count, const uint8_t *colours)
{
    if ((uint32_t)p->write_count >= RACE_PALETTE_WRITES)
        return;
    p->writes[p->write_count] = (RacePaletteWrite){ first, count, colours };
    p->write_count++;
}

static uint8_t to_8_bits(uint8_t v)
{
    return (uint8_t)(v << 2 | v >> 4);
}

void race_palette_write(RacePalette *p, int32_t first, int32_t count, const uint8_t *colours)
{
    for (int32_t i = first; i != (int32_t)((uint32_t)first + (uint32_t)count); i++, colours += 3) {
        const uint8_t r = to_8_bits(colours[0]), g = to_8_bits(colours[1]), b = to_8_bits(colours[2]);
        if (i < 0 || i >= RACE_PALETTE_COLOURS)
            continue;
        uint8_t *rgbx = p->screen_rgbx + 4 * i, *bgra = p->screen + 4 * i;
        rgbx[0] = r, rgbx[1] = g, rgbx[2] = b, rgbx[3] = 0;
        bgra[0] = b, bgra[1] = g, bgra[2] = r, bgra[3] = OPAQUE;
    }
    p->changed = 1;
    if (p->screen_bits <= PALETTE_SCREEN_BITS && (p->palette_device & 1) != 0) {
        p->handed++;
        p->changed = 0;
    }
}

void race_palette_apply(RacePalette *p)
{
    for (int32_t i = 0; i < p->write_count && i < RACE_PALETTE_WRITES; i++) {
        const RacePaletteWrite *w = &p->writes[i];
        if (w->colours != NULL)
            race_palette_write(p, w->first, w->count, w->colours);
    }
    p->write_count = 0;
}

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

void race_palette_tint(RacePalette *p, int32_t first, int32_t end, uint8_t *colours)
{
    RacePaletteTint *t = &p->tint;
    if (p->tint_ready == 0)
        return;
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
