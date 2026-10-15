/* The menu key of a frame (menu_key.h). */
#include "game/sys/menu_key.h"

#include "game/sys/key_codes.h"

#include <stddef.h>

static const uint16_t MENU_KEYS[] = { 0x20, 0xd, 0x1b, 9, 0x4800, 0x5000, 0x4b00, 0x4d00 };

int32_t menu_key(const uint8_t key_flags[KEY_FLAGS], JoyState *j, int32_t present, int32_t device, const JoyOps *ops)
{
    for (size_t i = 0; i < sizeof MENU_KEYS / sizeof MENU_KEYS[0]; i++) {
        const int32_t code = key_code(MENU_KEYS[i]);
        if (code >= 0 && code < KEY_FLAGS && key_flags[code] != 0) return MENU_KEYS[i];  /* port guard: the table */
    }
    if (joy_keys(j, present, device, ops) == 0) return 0;
    if (j->button_key != 0) return j->button_key;
    return j->dir_key;
}
