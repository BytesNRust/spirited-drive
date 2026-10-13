/* The race's key commands (race_keys.h). */
#include "game/race/race_keys.h"

#include "game/gfx/x86_int.h"

#include <stddef.h>

enum {
    KEY_TAB = 0x0f, KEY_ENTER = 0x1c, KEY_CTRL = 0x1d, KEY_LSHIFT = 0x2a, KEY_M = 0x32, KEY_RSHIFT = 0x36,
    KEY_ALT = 0x38, KEY_SPACE = 0x39, KEY_F1 = 0x3b, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_PAD_8 = 0x48, KEY_PAD_9, KEY_PAD_4 = 0x4b, KEY_PAD_6 = 0x4d, KEY_PAD_1 = 0x4f, KEY_PAD_2, KEY_PAD_3,
    KEY_PAD_0, KEY_1 = 0x02, CARS = 8, FREE_CARS = 2,
    PAD_HOLD = 4, HOLD_PHASE = 1, HOLD_BLOCK = 5, RESET_WAIT = 100, RESET_WAIT_PLAYBACK = 400, SCREEN_CLEAR = 3
};

static const uint8_t DETAIL[3] = { 0x1c, 0xe, 9 };

static const struct {
    uint8_t key;
    uint32_t bit;
} CAMERA_KEYS[] = { { KEY_PAD_1, 0x800000 }, { KEY_PAD_0, 0x1000000 }, { KEY_PAD_8, 0x40000000 },
                    { KEY_PAD_2, 0x80000000u }, { KEY_PAD_4, 0x10000000 }, { KEY_PAD_6, 0x20000000 },
                    { KEY_PAD_9, 0x400000 }, { KEY_PAD_3, 0x200000 }, { KEY_SPACE, 0x8000000 },
                    { KEY_ENTER, 0x4000000 }, { KEY_TAB, 0x2000000 } };

static void watch(const RaceKeysOps *ops, RaceKeysState *st, const RaceKeysInputs *in)
{
    if (in->keys[in->hold_key] != 0 || (in->pad_present != 0 && (in->pad_buttons & PAD_HOLD) != 0)) {
        if (st->hold_armed != 0 && st->viewed < in->humans && (in->phase == HOLD_PHASE || in->w513730 != HOLD_BLOCK)) {
            ops->follow_own(ops->ctx);
            ops->own_changed(ops->ctx);
        }
        st->hold_armed = 0;
        return;
    }
    if (in->keys[KEY_ALT] == 0 || in->keys[KEY_CTRL] == 0) {
        st->hold_armed = 1;
        return;
    }
    for (int32_t n = 0; n < CARS; n++) {
        if (in->keys[KEY_1 + n] == 0) continue;
        if (n >= FREE_CARS && in->race_type <= 1 && in->keys[KEY_RSHIFT] == 0) continue;
        st->viewed = n;
        st->view_changed = 1;
        ops->follow(ops->ctx, n);
        ops->viewed_changed(ops->ctx);
        return;
    }
}

static void function_key(const RaceKeysOps *ops, RaceKeysState *st, const RaceKeysInputs *in)
{
    const int first = st->fkey_held == 0;
    if (in->keys[KEY_F1] != 0) {
        if (first) {
            ops->f1(ops->ctx);
            if (st->screen_kind == SCREEN_CLEAR) st->clear_view = 1;
            st->view_changed = 1;
        }
    } else if (in->keys[KEY_F2] != 0) {
        if (first) {
            st->screen_kind = (uint8_t)((uint8_t)(st->screen_kind + 1) % 4);
            if (in->high == 0 && st->screen_kind > 1) st->screen_kind = 0;
            ops->screen(ops->ctx, st->screen_kind);
            if (st->screen_kind == SCREEN_CLEAR) st->clear_view = 1;
        }
    } else if (in->keys[KEY_F3] != 0) {
        if (first) {
            st->detail = (uint8_t)((uint8_t)(st->detail + 1) % 3);
            ops->detail(ops->ctx, DETAIL[st->detail]);
        }
    } else if (in->keys[KEY_F4] != 0) {
        if (first) st->w4fb786 = (uint8_t)(1 - st->w4fb786);
    } else if (in->keys[KEY_F5] != 0) {
        if (first) {
            st->f5 = x_sub(1, st->f5);
            if (st->f5 != 0) ops->f5_on(ops->ctx);
            else ops->f5_off(ops->ctx);
        }
    } else if (in->keys[KEY_F6] != 0) {
        if (first) st->hidden = (uint8_t)(1 - st->hidden);
    } else {
        st->fkey_held = 0;
        return;
    }
    st->fkey_held = 1;
}

void race_keys(const RaceKeysOps *ops, RaceKeysState *st, const RaceKeysInputs *in)
{
    watch(ops, st, in);
    if (in->replay == 0 && in->keys[KEY_PAD_1] != 0 && (in->keys[KEY_LSHIFT] != 0 || in->keys[KEY_RSHIFT] != 0) &&
        ((in->playback == 0 && in->timer_8d8 > RESET_WAIT) || in->timer_8d8 > RESET_WAIT_PLAYBACK))
        ops->reset(ops->ctx, in->player);
    if (in->w4c5ccc == 0 && in->keys[KEY_ALT] != 0 && in->keys[KEY_M] != 0) st->w4c2d80 ^= 1;
    if (in->keys[KEY_CTRL] == 0) function_key(ops, st, in);
    uint32_t bits = 0;
    for (size_t k = 0; k < sizeof CAMERA_KEYS / sizeof CAMERA_KEYS[0]; k++)
        if (in->keys[CAMERA_KEYS[k].key] != 0) bits |= CAMERA_KEYS[k].bit;
    st->camera_keys = bits;
}
