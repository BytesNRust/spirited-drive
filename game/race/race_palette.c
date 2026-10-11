/* The race's screen palette (race_palette.h). */
#include "game/race/race_palette.h"

#include "game/sys/palette.h"

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
    HIDDEN_MARK = '!'
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
