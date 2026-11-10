/* The joystick's buttons, axes and menu keys (joystick.h). */
#include "game/sys/joystick.h"

enum {
    SHIFT_MASK = 31,
    SLOT_X = 1, SLOT_Y = 2, SLOT_Z = 4, SLOT_R = 8,
    KEY_BUTTON0 = 0x20, KEY_BUTTON1 = 0xd, KEY_ESCAPE = 0x1b,
    KEY_UP = 0x4800, KEY_DOWN = 0x5000, KEY_LEFT = 0x4b00, KEY_RIGHT = 0x4d00
};

typedef char joy_state_layout[sizeof(JoyState) == 0xd4 ? 1 : -1];

static uint32_t wide(int16_t s) { return (uint32_t)(int32_t)s; }

static uint32_t bit_at(uint32_t v, uint32_t bit, uint8_t at) { return (uint32_t)((v & bit) != 0) << (at & SHIFT_MASK); }

void joy_remap(const JoyState *j, uint32_t *buttons)
{
    const uint32_t mask = wide(j->button_on[0]) | wide(j->button_on[1]) << 1 | wide(j->button_on[2]) << 2 |
                          wide(j->button_on[3]) << 3;
    const uint32_t kept = *buttons & mask;
    const uint32_t v = ((kept & 2) ^ wide(j->invert[1])) | (wide(j->invert[0]) ^ kept) | ((kept & 4) ^ wide(j->invert[2])) |
                       ((kept & 8) ^ wide(j->invert[3]));
    *buttons = bit_at(v, 1, j->button_bit[0]) | bit_at(v, 2, j->button_bit[1]) | bit_at(v, 4, j->button_bit[2]) |
               bit_at(v, 8, j->button_bit[3]);
}

int32_t joy_poll(JoyState *j, int32_t device, const JoyOps *ops)
{
    if (ops->read(ops->ctx, device, &j->buttons, j->axis) == 0) return 0;
    if (j->raw == 0) joy_remap(j, &j->buttons);
    if (j->raw_axes != 0) return 1;
    int32_t to[JOY_AXES] = { 0, 0, 0, 0 };  /* the original: the caller's ebx, esi, ebp, edi */
    for (int k = 0; k < JOY_AXES; k++) {
        if (j->axis_on[k] == 0) continue;
        switch (j->axis_slot[k]) {
        case SLOT_X: to[0] = j->axis[k]; break;
        case SLOT_Y: to[1] = j->axis[k]; break;
        case SLOT_Z: to[2] = j->axis[k]; break;
        case SLOT_R: to[3] = j->axis[k]; break;
        default: break;
        }
    }
    for (int k = 0; k < JOY_AXES; k++) j->axis[k] = to[k];
    return 1;
}

int32_t joy_menu_key(int32_t button_key, int cancel)
{
    return cancel && button_key == KEY_BUTTON1 ? KEY_ESCAPE : button_key;
}

int32_t joy_keys(JoyState *j, int32_t present, int32_t device, const JoyOps *ops)
{
    if (present == 0 || joy_poll(j, device, ops) == 0) {
        j->dir_key = 0;
        j->button_key = 0;
        return 0;
    }
    j->button_key = (j->buttons & 1) != 0 ? KEY_BUTTON0 : (j->buttons & 2) != 0 ? KEY_BUTTON1 : 0;
    const int32_t x = j->axis[0], y = j->axis[1];
    j->dir_key = y < j->y_low ? KEY_UP : y > j->y_high ? KEY_DOWN : x < j->x_low ? KEY_LEFT : x > j->x_high ? KEY_RIGHT : 0;
    return j->button_key != 0 || j->dir_key != 0;
}
