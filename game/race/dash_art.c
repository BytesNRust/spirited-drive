/* The race's on-screen art from the mask bank (dash_art.h). */
#include "game/race/dash_art.h"

#include "game/sys/shape_bank.h"

#include <stddef.h>

enum {
    SHAPE_WIDTH = 4, SHAPE_HEIGHT = 6, SHAPE_X = 0xc, SHAPE_Y = 0xe
};

/* 004c5e34, 004c5e9c, 004c5e68 (entry 0 '!!!!' is never looked up). */
static const char BUTTON[REPLAY_BUTTONS][5] = {
    "!!!!", "dond", "0<d ", "01d ", "02d ", "03d ", "04d ", "05d ", "06d ", "07d ", "08d ", "0>d ", "camd"
};
static const char BUTTON_UP[REPLAY_BUTTONS][5] = {
    "!!!!", "donl", "0<lu", "01lu", "02lu", "03lu", "04lu", "05lu", "06lu", "07lu", "08lu", "0>lu", "caml"
};
static const char BUTTON_DOWN[REPLAY_BUTTONS][5] = {
    "!!!!", "doni", "0<ld", "01ld", "02ld", "03ld", "04ld", "05ld", "06ld", "07ld", "08ld", "0>ld", "cami"
};
/* 004c5ed0, 004c5ee8. */
static const char VIEW[REPLAY_VIEWS][5] = { "!!!!", "bumd", "taid", "held", "skyd", "chad" };
static const char VIEW_LIT[REPLAY_VIEWS][5] = { "!!!!", "buml", "tail", "hell", "skyl", "chal" };
/* 004c5f00. */
static const char GLYPH[REPLAY_GLYPHS][5] = {
    "00d ", "1d  ", "2d  ", "3d  ", "4d  ", "5d  ", "6d  ", "7d  ", "8d  ", "9d  ", "0a  ", "0b  ",
    "0c  ", "0d  ", "0e  ", "0f  ", "0g  ", "0h  ", "0i  ", "0j  ", "0k  ", "0l  ", "0m  ", "0n  ",
    "0o  ", "0p  ", "0q  ", "0r  ", "0s  ", "0t  ", "0u  ", "0v  ", "0w  ", "0x  ", "0y  ", "0z  "
};

static int32_t s16(const uint8_t *b)
{
    return (int16_t)(uint16_t)(b[0] | b[1] << 8);
}

static DashRect shape_box(const uint8_t *shape)
{
    const int32_t x = s16(shape + SHAPE_X), y = s16(shape + SHAPE_Y);
    return (DashRect){ x, y, s16(shape + SHAPE_WIDTH) + x, s16(shape + SHAPE_HEIGHT) + y };
}

/* 004732dd into *to; 0 when the bank lacks it. */
static int find(uint8_t **to, uint8_t *bank, const char name[5])
{
    *to = shape_bank_find(bank, (const uint8_t *)name);
    return *to != NULL;
}

int dash_replay_art(ReplayArt *art, uint8_t *bank, int16_t replay)
{
    if (!find(&art->replay, bank, "repl") || !find(&art->bar, bank, "bard") || !find(&art->names, bank, "namd"))
        return DASH_ART_MISSING;
    for (int i = 1; i < REPLAY_BUTTONS; i++) {
        if (i == 1 && replay == 0) {
            /* 'donm' three times; the up and down ones are looked up again below */
            uint8_t *unused;
            if (!find(&art->button[i], bank, "donm") || !find(&unused, bank, "donm") || !find(&unused, bank, "donm"))
                return DASH_ART_MISSING;
        } else if (!find(&art->button[i], bank, BUTTON[i])) {
            return DASH_ART_MISSING;
        }
        if (!find(&art->button_up[i], bank, BUTTON_UP[i]) || !find(&art->button_down[i], bank, BUTTON_DOWN[i]))
            return DASH_ART_MISSING;
        art->box[i] = shape_box(art->button_down[i]);
    }
    for (int i = 1; i < REPLAY_VIEWS; i++)
        if (!find(&art->view[i], bank, VIEW[i]) || !find(&art->view_lit[i], bank, VIEW_LIT[i]))
            return DASH_ART_MISSING;
    for (int i = 0; i < REPLAY_GLYPHS; i++)
        if (!find(&art->glyph[i], bank, GLYPH[i])) return DASH_ART_MISSING;
    art->ready = 1;
    return 0;
}
