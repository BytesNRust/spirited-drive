/* The followed player's controls from the keys, the pad or the mouse (input_read.h). */
#include "game/race/input_read.h"

#include "game/gfx/x86_int.h"

static const uint32_t STAND_IN_WORD = 0xffff87a0u;

enum {
    CENTRE = 0x20, FULL = 0x3c, AT_REST = 0x1e, PRESSED = 5,
    PAD = 1, PAD_BUTTONS = 3, PAD_LOOK_BACK = 8,
    KEY_CHEAT_A = 0x30, KEY_CHEAT_B = 0x13, KEY_CHEAT_C = 0x11,
    GEAR_UP = 1, GEAR_DOWN = 2, LOOK_BACK = 8, HANDBRAKE = 4
};

static int key(const InputReadInputs *in, int which)
{
    return in->keys[in->key_codes[which]] != 0;
}

static void mouse_once(const InputReadOps *ops, MouseSteerWords *mouse, int *polled)
{
    if (*polled) return;
    mouse_steer(&ops->mouse, mouse);
    *polled = 1;
}

static int32_t steering(const InputReadOps *ops, InputPad *pad, MouseSteerWords *mouse, const InputReadInputs *in,
                        const ControlSetup *setup, int *polled)
{
    if (setup->analog_steering == 0) {
        const int left = key(in, INPUT_KEY_LEFT), right = key(in, INPUT_KEY_RIGHT);
        if (left && right) return CENTRE;
        if (left) return in->mirrored != 0 ? FULL : 0;
        if (right) return in->mirrored != 0 ? 0 : FULL;
        return CENTRE;
    }
    if (setup->analog_steering == PAD) {
        if (in->mirrored == 0) return x_add(pad->x, CENTRE);
        if (pad->x == 0x1f || pad->x == -0x20) pad->x = x_add(pad->x, 1);
        return x_sub(CENTRE, pad->x);
    }
    mouse_once(ops, mouse, polled);
    return mouse->steer;
}

static int32_t pedal(const InputReadOps *ops, const InputPad *pad, MouseSteerWords *mouse,
                     const InputReadInputs *in, const ControlSetup *setup, int *polled)
{
    int32_t p = AT_REST;
    if (setup->pedals == 0) {
        if (key(in, INPUT_KEY_ACCELERATE)) p = PRESSED;
        if (key(in, INPUT_KEY_BRAKE)) p = FULL;
    } else if (setup->pedals == PAD) {
        p = x_add(pad->y, AT_REST);
    } else if (setup->pedals == PAD_BUTTONS) {
        if (pad->buttons & 1) p = PRESSED;
        if (pad->buttons & 2) p = FULL;
    } else {
        mouse_once(ops, mouse, polled);
        if (mouse->buttons == 1) p = PRESSED;
        if (mouse->buttons == 2) p = FULL;
    }
    return p;
}

static uint32_t gears(const InputReadOps *ops, const InputPad *pad, MouseSteerWords *mouse,
                      const InputReadInputs *in, const ControlSetup *setup, int *polled)
{
    uint32_t g = 0;
    if (setup->gears == 0) {
        if (key(in, INPUT_KEY_GEAR_UP)) g = GEAR_UP;
        else if (key(in, INPUT_KEY_GEAR_DOWN)) g = GEAR_DOWN;
    } else if (setup->gears == PAD_BUTTONS) {
        if (pad->buttons & 1) g = GEAR_UP;
        else if (pad->buttons & 2) g = GEAR_DOWN;
    } else {
        mouse_once(ops, mouse, polled);
        if (mouse->buttons == 1) g = GEAR_UP;
        else if (mouse->buttons == 2) g = GEAR_DOWN;
    }
    return g;
}

uint32_t input_read(const InputReadOps *ops, InputPad *pad, MouseSteerWords *mouse, const InputReadInputs *in)
{
    const int16_t stand_in = in->stand_in[in->player];
    if (stand_in == 1) return STAND_IN_WORD;
    ops->pad_read(ops->ctx, in->player, stand_in);
    const ControlSetup *setup = &in->controls[in->player];
    int polled = 0;
    const int32_t steer = steering(ops, pad, mouse, in, setup, &polled);
    const int32_t p = pedal(ops, pad, mouse, in, setup, &polled);
    uint32_t g = gears(ops, pad, mouse, in, setup, &polled);
    if (in->keys[KEY_CHEAT_A] != 0 && in->keys[KEY_CHEAT_B] != 0 && in->keys[KEY_CHEAT_C] != 0 &&
        in->entries[in->player].automatic != 0)
        g |= GEAR_UP | GEAR_DOWN;
    if (key(in, INPUT_KEY_LOOK_BACK) || (in->pad_present != 0 && (pad->buttons & PAD_LOOK_BACK) != 0)) g |= LOOK_BACK;
    if (key(in, INPUT_KEY_HANDBRAKE) ||
        (in->pad_present != 0 && setup->pedals != PAD_BUTTONS && setup->gears != PAD_BUTTONS && (pad->buttons & 1) != 0))
        g |= HANDBRAKE;
    return g << 12 | (uint32_t)steer | (uint32_t)p << 6;
}
