/* The race's on-screen art from the mask bank (dash_art.h). */
#include "game/race/dash_art.h"

#include "game/sys/fixed_math.h"
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

/* 004c53dc, 004c5424, 004c5400 (entry 0 is never looked up). */
static const char CONTROL[DASH_CONTROLS][5] = { "!!!!", "dond", "rtsd", "rewd", "ford", "fted", "plad", "paud", "camd" };
static const char CONTROL_LIT[DASH_CONTROLS][5] = { "!!!!", "donl", "rtsl", "rewl", "forl", "ftel", "plal", "paul", "caml" };
static const char CONTROL_DOWN[DASH_CONTROLS][5] = { "!!!!", "doni", "rtsi", "rewi", "fori", "ftei", "plai", "paui", "cami" };
/* 004c5448, 004c5468. */
static const char DASH_VIEW[DASH_VIEWS][5] = { "card", "bumd", "taid", "held", "skyd", "chad", "oppd", "copd" };
static const char DASH_VIEW_LIT[DASH_VIEWS][5] = { "carl", "buml", "tail", "hell", "skyl", "chal", "oppl", "copl" };
/* 004c5488, 004c5494. */
static const char SPEED[DASH_SPEEDS][5] = { "fuld", "1/2d", "1/4d" };
static const char SPEED_LIT[DASH_SPEEDS][5] = { "full", "1/2l", "1/4l" };
/* 004c54a0, 004c5508, 004c54cc. */
static const char GREY[DASH_DIGITS][5] = {
    "gry0", "gry1", "gry2", "gry3", "gry4", "gry5", "gry6", "gry7", "gry8", "gry9", "gryb"
};
static const char RED[DASH_DIGITS][5] = {
    "red0", "red1", "red2", "red3", "red4", "red5", "red6", "red7", "red8", "red9", "redb"
};
static const char YELLOW[DASH_YELLOW][5] = {
    "yel0", "yel1", "yel2", "yel3", "yel4", "yel5", "yel6", "yel7", "yel8", "yel9", "yelr", "yeln", "plus", "mnus", "yelb"
};

enum {
    NEEDLE_START = 0x1a9,
    NEEDLE_STEP_LOW = 0x20, NEEDLE_STEP_HIGH = 0x13
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

/* 004b0974..004b09d4, in the order 004502b0 looks them up. */
static int find_singles(DashArt *art)
{
    uint8_t *const bank = art->bank;
    return find(&art->replay, bank, "repl") && find(&art->bar, bank, "bar1") && find(&art->tach, bank, "tach")
        && find(&art->wait, bank, "wait") && find(&art->lost, bank, "lost") && find(&art->paused, bank, "paus")
        && find(&art->sync, bank, "sync") && find(&art->srdd, bank, "srdd") && find(&art->srdl, bank, "srdl")
        && find(&art->syld, bank, "syld") && find(&art->syll, bank, "syll") && find(&art->green, bank, "sgrn")
        && find(&art->acct, bank, "acct");
}

/* The tach's middle and radii: three quarters of the half size, less a
 * sixteenth at low resolution. */
static void tach_frame(DashArt *art, int32_t high)
{
    const int32_t half_w = s16(art->tach + SHAPE_WIDTH) / 2, half_h = s16(art->tach + SHAPE_HEIGHT) / 2;
    art->centre_x = half_w + s16(art->tach + SHAPE_X);
    art->centre_y = half_h + s16(art->tach + SHAPE_Y);
    art->radius_x = half_w - half_w / 4;
    art->radius_y = half_h - half_h / 4;
    if (high == 0) {
        art->radius_x -= half_w / 16;
        art->radius_y -= half_h / 16;
    }
}

static void needle_points(DashArt *art, int32_t high)
{
    const int count = high != 0 ? DASH_NEEDLE_HIGH : DASH_NEEDLE_LOW;
    const uint32_t step = high != 0 ? NEEDLE_STEP_HIGH : NEEDLE_STEP_LOW;
    uint32_t angle = NEEDLE_START;
    for (int k = 0; k < count; k++, angle += step) {
        art->needle[k][0] = fx_mul_round(art->radius_x, fx_cos(angle)) + art->centre_x;
        art->needle[k][1] = fx_mul_round(art->radius_y, fx_sin(angle)) + art->centre_y;
        art->needle_mid[k][0] = (art->needle[k][0] + art->centre_x) / 2;
        art->needle_mid[k][1] = (art->needle[k][1] + art->centre_y) / 2;
    }
}

int dash_art_find(DashArt *art, ReplayArt *replay_art, int32_t high, int16_t replay)
{
    uint8_t *const bank = art->bank;
    if (!find_singles(art)) return DASH_ART_MISSING;
    tach_frame(art, high);
    for (int i = 1; i < DASH_CONTROLS; i++) {
        if (!find(&art->control[i], bank, CONTROL[i]) || !find(&art->control_lit[i], bank, CONTROL_LIT[i])
            || !find(&art->control_down[i], bank, CONTROL_DOWN[i]))
            return DASH_ART_MISSING;
        art->control_box[i] = shape_box(art->control_down[i]);
    }
    for (int i = 0; i < DASH_VIEWS; i++)
        if (!find(&art->view[i], bank, DASH_VIEW[i]) || !find(&art->view_lit[i], bank, DASH_VIEW_LIT[i]))
            return DASH_ART_MISSING;
    for (int i = 0; i < DASH_SPEEDS; i++)
        if (!find(&art->speed[i], bank, SPEED[i]) || !find(&art->speed_lit[i], bank, SPEED_LIT[i]))
            return DASH_ART_MISSING;
    for (int i = 0; i < DASH_DIGITS; i++) {
        if (!find(&art->grey[i], bank, GREY[i])) return DASH_ART_MISSING;
        art->grey_shown[i] = art->grey[i];
        if (!find(&art->red[i], bank, RED[i])) return DASH_ART_MISSING;
    }
    for (int i = 0; i < DASH_YELLOW; i++)
        if (!find(&art->yellow[i], bank, YELLOW[i])) return DASH_ART_MISSING;
    needle_points(art, high);
    return dash_replay_art(replay_art, bank, replay);
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
    art->camera = 1;
    return 0;
}
