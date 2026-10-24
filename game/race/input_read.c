/* The followed player's controls from the keys, the pad or the mouse (input_read.h). */
#include "game/race/input_read.h"

#include "game/gfx/x86_int.h"

static const uint32_t STAND_IN_WORD = 0xffff87a0u;

enum {
    CENTRE = 0x20, FULL = 0x3c, AT_REST = 0x1e, PRESSED = 5,
    PAD = 1, PAD_BUTTONS = 3, PAD_HANDBRAKE = 8,
    KEY_CHEAT_A = 0x30, KEY_CHEAT_B = 0x13, KEY_CHEAT_C = 0x11,
    GEAR_UP = 1, GEAR_DOWN = 2, HANDBRAKE = 8, HORN = 4   /* the word's >> 12: 0x8000 and 0x4000 (00438f00) */
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

/* port_pad_and_keys: the stick's x when it is off its centre, else the keys (a pad with no stick off centre, or none,
 * has x 0) */
static int32_t both_steering(InputPad *pad, const InputReadInputs *in)
{
    if (pad->x == 0) {
        const ControlSetup keyboard = { 0 };
        return steering(NULL, pad, NULL, in, &keyboard, NULL);
    }
    const ControlSetup stick = { .analog_steering = PAD };
    return steering(NULL, pad, NULL, in, &stick, NULL);
}

/* the key's pedal and the pad's, each as its distance from the rest: the further wins, the brake at a tie */
static int32_t both_pedal(const InputPad *pad, const InputReadInputs *in)
{
    int32_t key_at = 0;
    if (key(in, INPUT_KEY_ACCELERATE)) key_at = PRESSED - AT_REST;
    if (key(in, INPUT_KEY_BRAKE)) key_at = FULL - AT_REST;
    const int32_t pad_at = pad->y;
    const int32_t pad_far = pad_at < 0 ? -pad_at : pad_at, key_far = key_at < 0 ? -key_at : key_at;
    const int32_t at = pad_far > key_far || (pad_far == key_far && pad_at > 0) ? pad_at : key_at;
    return x_add(at, AT_REST);
}

static uint32_t both_gears(const InputPad *pad, const InputReadInputs *in)
{
    if (key(in, INPUT_KEY_GEAR_UP)) return GEAR_UP;
    if (key(in, INPUT_KEY_GEAR_DOWN)) return GEAR_DOWN;
    if (pad->buttons & 1) return GEAR_UP;
    return (pad->buttons & 2) != 0 ? GEAR_DOWN : 0;
}

/* the word of port_pad_and_keys: as input_read's, from both devices */
static uint32_t read_both(InputPad *pad, const InputReadInputs *in)
{
    const int32_t steer = both_steering(pad, in), p = both_pedal(pad, in);
    uint32_t g = both_gears(pad, in);
    if (in->keys[KEY_CHEAT_A] != 0 && in->keys[KEY_CHEAT_B] != 0 && in->keys[KEY_CHEAT_C] != 0 &&
        in->entries[in->player].automatic != 0)
        g |= GEAR_UP | GEAR_DOWN;
    if (key(in, INPUT_KEY_HANDBRAKE) || (in->pad_present != 0 && (pad->buttons & PAD_HANDBRAKE) != 0)) g |= HANDBRAKE;
    if (key(in, INPUT_KEY_HORN)) g |= HORN;
    return g << 12 | (uint32_t)steer | (uint32_t)p << 6;
}

uint32_t input_read(const InputReadOps *ops, InputPad *pad, MouseSteerWords *mouse, const InputReadInputs *in)
{
    const int16_t stand_in = in->stand_in[in->player];
    if (stand_in == 1) return STAND_IN_WORD;
    ops->pad_read(ops->ctx, in->player, stand_in);
    const ControlSetup *setup = &in->controls[in->player];
    if (in->port_pad_and_keys != 0) return read_both(pad, in);
    int polled = 0;
    const int32_t steer = steering(ops, pad, mouse, in, setup, &polled);
    const int32_t p = pedal(ops, pad, mouse, in, setup, &polled);
    uint32_t g = gears(ops, pad, mouse, in, setup, &polled);
    if (in->keys[KEY_CHEAT_A] != 0 && in->keys[KEY_CHEAT_B] != 0 && in->keys[KEY_CHEAT_C] != 0 &&
        in->entries[in->player].automatic != 0)
        g |= GEAR_UP | GEAR_DOWN;
    if (key(in, INPUT_KEY_HANDBRAKE) || (in->pad_present != 0 && (pad->buttons & PAD_HANDBRAKE) != 0)) g |= HANDBRAKE;
    if (key(in, INPUT_KEY_HORN) ||
        (in->pad_present != 0 && setup->pedals != PAD_BUTTONS && setup->gears != PAD_BUTTONS && (pad->buttons & 1) != 0))
        g |= HORN;
    return g << 12 | (uint32_t)steer | (uint32_t)p << 6;
}
