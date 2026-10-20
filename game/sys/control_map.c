/* port: the pad's or wheel's raw buttons and axes through the bindings (control_map.h). */
#include "game/sys/control_map.h"

#include <stdlib.h>

enum {
    AXIS_TOP = 0xffff, AXIS_MIDDLE = 0x8000, PEDAL_MAX = 0x7fff, PRESSED_FROM = 0x4000,
    STICK_MIN = -32768, STICK_MAX = 32767, FAR_REST = 16384, PERCENT = 100,
    /* the gamepad's own buttons and axes (SDL_GamepadButton / SDL_GamepadAxis), for what is not bindable */
    PAD_BACK = 4, PAD_START = 6, PAD_LEFT_STICK = 7, PAD_RIGHT_STICK = 8, PAD_LEFT_SHOULDER = 9,
    PAD_RIGHT_SHOULDER = 10, PAD_DPAD_UP = 11, PAD_DPAD_DOWN = 12, PAD_AXIS_LEFTY = 1,
    BUTTON_BITS = 4, FIRST_EXTRA_BIT = 4, EXTRA_PAD_BUTTONS = 6
};

/* the pad's buttons after the four actions, in XInput's WinMM order (platform_joy_sdl3.c's old PAD_ORDER from 4) */
static const int EXTRA_PAD_ORDER[EXTRA_PAD_BUTTONS] = {
    PAD_LEFT_SHOULDER, PAD_RIGHT_SHOULDER, PAD_BACK, PAD_START, PAD_LEFT_STICK, PAD_RIGHT_STICK };
static const PortAction BUTTON_ACTION[BUTTON_BITS] = { ACT_GEAR_UP, ACT_GEAR_DOWN, ACT_CAMERA, ACT_HANDBRAKE };
static const PortAction KEY_ACTION[3] = { ACT_HORN, ACT_PAUSE, ACT_VIEW };

static int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static int button_down(const RawPad *r, int i)
{
    return i >= 0 && i < r->buttons && i < RAW_MAX_BUTTONS && r->button[i] != 0;
}

static int axis_of(const RawPad *r, int i)
{
    return i >= 0 && i < r->axes && i < RAW_MAX_AXES ? r->axis[i] : 0;
}

/* How far a source is pressed, 0..PEDAL_MAX: a button or a hat's direction all or nothing, an axis from its rest
 * toward the end its kind names (a whole axis toward the end farther from its rest). */
static int amount(const ControlSource *s, const RawPad *r)
{
    switch (s->kind) {
    case SRC_BUTTON:
        return button_down(r, s->index) ? PEDAL_MAX : 0;
    case SRC_HAT:
        return s->index >= 0 && s->index < r->hats && s->index < RAW_MAX_HATS && (r->hat[s->index] & s->rest) != 0
                   ? PEDAL_MAX : 0;
    case SRC_AXIS_POS:
    case SRC_AXIS_NEG:
    case SRC_AXIS_FULL: {
        if (s->index < 0 || s->index >= r->axes || s->index >= RAW_MAX_AXES) return 0;
        int v = axis_of(r, s->index), rest = s->rest;
        if (s->invert) v = clamp(-v, STICK_MIN, STICK_MAX), rest = clamp(-rest, STICK_MIN, STICK_MAX);
        if (s->kind == SRC_AXIS_POS) return clamp(v - rest, 0, PEDAL_MAX);
        if (s->kind == SRC_AXIS_NEG) return clamp(rest - v, 0, PEDAL_MAX);
        const int downward = rest >= FAR_REST;
        const int span = downward ? rest - STICK_MIN : STICK_MAX - rest;
        const int travelled = clamp(downward ? rest - v : v - rest, 0, span);
        return span > 0 ? (int)((long long)travelled * PEDAL_MAX / span) : 0;
    }
    default:
        return 0;
    }
}

static int pressed(const ControlSource *s, const RawPad *r)
{
    return amount(s, r) >= PRESSED_FROM;
}

static int action_pressed(const PortControls *c, PortAction a, const RawPad *r)
{
    return pressed(&c->bind[a][0], r) || pressed(&c->bind[a][1], r);
}

static int pedal_of(const PortControls *c, PortAction a, const RawPad *r)
{
    const int t0 = amount(&c->bind[a][0], r), t1 = amount(&c->bind[a][1], r);
    return AXIS_TOP - 2 * (t0 > t1 ? t0 : t1);
}

/* a source's steering, -32768..32767: an axis from its rest, a half axis one way */
static int steer_of(const ControlSource *s, const RawPad *r)
{
    if (s->kind == SRC_AXIS_FULL) {
        if (s->index < 0 || s->index >= r->axes || s->index >= RAW_MAX_AXES) return 0;
        int v = axis_of(r, s->index), rest = s->rest;
        if (s->invert) v = clamp(-v, STICK_MIN, STICK_MAX), rest = clamp(-rest, STICK_MIN, STICK_MAX);
        return clamp(v - rest, STICK_MIN, STICK_MAX);
    }
    if (s->kind == SRC_AXIS_POS) return amount(s, r);
    if (s->kind == SRC_AXIS_NEG) return -amount(s, r);
    return 0;
}

/* the dead zone taken off the travel (the end still reached), then the gain, kept in the stick's range */
static int shaped(int s, int deadzone, int sensitivity)
{
    const int end = s < 0 ? -STICK_MIN : STICK_MAX;
    const long long dead = (long long)(-STICK_MIN) * deadzone / PERCENT;
    long long mag = s < 0 ? -(long long)s : s;
    if (mag <= dead) return 0;
    mag = (mag - dead) * end / (end - dead);
    mag = mag * sensitivity / PERCENT;
    if (mag > end) mag = end;
    return s < 0 ? -(int)mag : (int)mag;
}

static int steering(const PortControls *c, const RawPad *r)
{
    if (action_pressed(c, ACT_STEER_LEFT, r)) return AXIS_MIDDLE + STICK_MIN;
    if (action_pressed(c, ACT_STEER_RIGHT, r)) return AXIS_MIDDLE + STICK_MAX;
    int s = steer_of(&c->bind[ACT_STEER][0], r);
    const int other = steer_of(&c->bind[ACT_STEER][1], r);
    if (abs(other) > abs(s)) s = other;
    return AXIS_MIDDLE + clamp(shaped(s, c->deadzone, c->sensitivity), STICK_MIN, STICK_MAX);
}

/* a wheel with no controls.cfg: its first buttons and four axes as they came */
static VirtualPad pass_through(const RawPad *r)
{
    VirtualPad v = { 0, { 0 }, 0 };
    for (int k = 0; k < r->buttons && k < RAW_MAX_BUTTONS; k++)
        if (r->button[k]) v.buttons |= 1u << k;
    for (int k = 0; k < 4; k++) v.axis[k] = AXIS_MIDDLE + axis_of(r, k);
    return v;
}

VirtualPad control_map_apply(const PortControls *c, const RawPad *raw)
{
    if (!raw->gamepad && !c->custom) return pass_through(raw);
    VirtualPad v = { 0, { 0 }, 0 };
    for (int bit = 0; bit < BUTTON_BITS; bit++)
        if (action_pressed(c, BUTTON_ACTION[bit], raw)) v.buttons |= 1u << bit;
    if (raw->gamepad) {
        for (int k = 0; k < EXTRA_PAD_BUTTONS; k++)
            if (button_down(raw, EXTRA_PAD_ORDER[k])) v.buttons |= 1u << (FIRST_EXTRA_BIT + k);
        v.axis[1] = button_down(raw, PAD_DPAD_UP) ? 0
                    : button_down(raw, PAD_DPAD_DOWN) ? AXIS_TOP
                    : AXIS_MIDDLE + axis_of(raw, PAD_AXIS_LEFTY);
    } else {
        for (int k = FIRST_EXTRA_BIT; k < raw->buttons && k < RAW_MAX_BUTTONS; k++)
            if (raw->button[k]) v.buttons |= 1u << k;
        v.axis[1] = AXIS_MIDDLE + axis_of(raw, 1);
    }
    v.axis[0] = steering(c, raw);
    v.axis[2] = pedal_of(c, ACT_GAS, raw);
    v.axis[3] = pedal_of(c, ACT_BRAKE, raw);
    for (int k = 0; k < 3; k++)
        if (action_pressed(c, KEY_ACTION[k], raw)) v.keys |= 1u << KEY_ACTION[k];
    return v;
}
