/* The game loop's key of a frame (hot_key.h). */
#include "game/sys/hot_key.h"

#include "game/sys/key_codes.h"

#include <stddef.h>
#include <stdio.h>

enum { SC_CTRL = 0x1d, SC_F5 = 0x3f, SC_F10 = 0x44, SC_INSERT = 0x52 };

static const uint16_t NAMED_KEYS[] = { 0x1b, 0x20, 0xd, 0x4800, 0x5000, 0x4b00, 0x4d00, 0x5200, 0x5300 };
static const uint8_t SCANCODES[] = { 0x2a, 0x36, 0x1d, 0x38, 0x15, 0x31, 0x24 };

static int named_down(const uint8_t key_flags[KEY_FLAGS], uint16_t key)
{
    const int32_t code = key_code(key);
    return code >= 0 && code < KEY_FLAGS && key_flags[code] != 0;  /* port guard: the table */
}

int32_t hot_key(HotKeyState *s, const uint8_t key_flags[KEY_FLAGS], JoyState *j, int32_t present, int32_t device,
                const HotKeyOps *ops, const JoyOps *joy_ops)
{
    if (key_flags[SC_F10] != 0 && key_flags[SC_CTRL] != 0) {
        if (s->playback != 0) s->dropped = 1;
        return 0;
    }
    if (key_flags[SC_INSERT] != 0 && key_flags[SC_CTRL] != 0 && s->debug != 0) {
        snprintf(s->shot_name, sizeof s->shot_name, "nfs%02d.tga", s->shot);
        ops->screenshot(ops->ctx, s->shot_name);
        s->shot++;
        return 0;
    }
    if (key_flags[SC_F5] != 0) {
        ops->mute(ops->ctx);
        return 0;
    }
    for (size_t i = 0; i < sizeof NAMED_KEYS / sizeof NAMED_KEYS[0]; i++) {
        if (named_down(key_flags, NAMED_KEYS[i])) return NAMED_KEYS[i];
    }
    for (size_t i = 0; i < sizeof SCANCODES; i++) {
        if (key_flags[SCANCODES[i]] != 0) return SCANCODES[i];
    }
    if (named_down(key_flags, 9)) return 0;
    if (joy_keys(j, present, device, joy_ops) == 0) return 0;
    if (j->button_key != 0) return j->button_key;
    return j->dir_key;
}
