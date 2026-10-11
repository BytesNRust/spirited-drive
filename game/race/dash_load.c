/* The dash's files (dash_load.h). */
#include "game/race/dash_load.h"

#include "game/sys/shape_bank.h"

#include <stdio.h>

enum {
    SHAPE_DATA = 0x10,
    PALETTE_TYPE_8_BIT = 0x24,
    PALETTE_BYTES = 0x300,
    MMB_WORDS = 8           /* 004512f8: the file's words from +8 */
};

const uint8_t dash_colours[DASH_COLOUR_COUNT * 3] = {
    0, 47, 0, 0, 35, 0, 41, 41, 41, 37, 37, 37, 26, 26, 26, 55, 0, 0, 20, 28, 39, 30, 37, 43, 63, 51, 0,
    36, 1, 49, 63, 32, 1, 0, 63, 63, 1, 1, 63
};

/* 004b0a00, 004b09f4, 004b09e8, 004b09dc. */
static const char *bank_name(int16_t german, int32_t high)
{
    if (german != 0) return high != 0 ? "maskhig.fsh" : "masklog.fsh";
    return high != 0 ? "maskhi.fsh" : "masklo.fsh";
}

static int fits(int n)
{
    return n >= 0 && n < DASH_PATH;
}

int dash_load_paths(DashFiles *files, const char *dir, int16_t german, int32_t high)
{
    if (!fits(snprintf(files->bank, DASH_PATH, "%s%s", dir, bank_name(german, high)))
        || !fits(snprintf(files->mmb, DASH_PATH, high != 0 ? "%sMASKHI.mmb" : "%sMASKLO.mmb", dir)))
        return -1;
    for (int r = 0; r < DASH_PART_ROWS; r++)
        for (int c = 0; c < DASH_PART_COLUMNS; c++)
            if (!fits(snprintf(files->parts[r][c], DASH_PATH, high != 0 ? "%smaskhi.b%d%d" : "%smasklo.b%d%d", dir, r, c)))
                return -1;
    return 0;
}

static int32_t word(const uint8_t *b)
{
    return (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
}

int dash_load_setup(DashArt *art, ReplayArt *replay_art, RacePalette *pal, uint8_t *bank, uint8_t *mmb,
                    int32_t high, int16_t replay)
{
    static const int32_t STATE[DASH_STATE_WORDS] = { 0, 0, 1, 0, 0, 1, 0 };
    art->bank = bank;
    uint8_t *const shape = shape_bank_find(bank, (const uint8_t *)"!pal");
    if (shape == NULL) return DASH_LOAD_NO_PALETTE;
    uint8_t *const colours = shape + SHAPE_DATA;
    art->palette = colours;
    if (shape[0] == PALETTE_TYPE_8_BIT)
        for (int i = 0; i < PALETTE_BYTES; i++) colours[i] = (uint8_t)(colours[i] >> 2);
    race_palette_queue_bank(pal, bank, colours, DASH_BANK_FIRST, DASH_BANK_COUNT);
    race_palette_queue(pal, DASH_COLOUR_FIRST, DASH_COLOUR_COUNT, dash_colours);
    const int found = dash_art_find(art, replay_art, high, replay);
    if (found != 0) return found;
    for (int i = 0; i < DASH_STATE_WORDS; i++) art->unknown_513888[i] = STATE[i];
    art->mmb = mmb;
    if (mmb == NULL) return DASH_LOAD_NO_MMB;
    for (int r = 0; r < DASH_PART_ROWS; r++)
        for (int c = 0; c < DASH_PART_COLUMNS; c++)
            art->mmb_words[r][c] = word(mmb + MMB_WORDS + 4 * (DASH_PART_COLUMNS * r + c));
    return 0;
}
