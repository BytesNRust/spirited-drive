/* port: the pad's or wheel's raw buttons and axes turned into the virtual joystick the original reads (0043d598's
 * buttons and four axes) through the bindings (port_controls.h), plus the virtual keys they press. Pure: no SDL. */
#ifndef TNFS_CONTROL_MAP_H
#define TNFS_CONTROL_MAP_H

#include "game/sys/port_controls.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { RAW_MAX_BUTTONS = 32, RAW_MAX_AXES = 8, RAW_MAX_HATS = 2 };

/* A device as it is now. `gamepad`: the numbers are SDL_Gamepad's (the buttons SDL_GamepadButton, the axes
 * SDL_GamepadAxis: the sticks -32768..32767, the triggers 0..32767) and not a joystick's own. */
typedef struct {
    int gamepad;
    int buttons, axes, hats;                 /* how many of the arrays below the device has */
    uint8_t button[RAW_MAX_BUTTONS];
    int16_t axis[RAW_MAX_AXES];
    uint8_t hat[RAW_MAX_HATS];               /* SDL's directions: up 1, right 2, down 4, left 8 */
} RawPad;

/* buttons: bit 0 gear up, 1 gear down, 2 camera, 3 handbrake, the rest as the original's pad had them; axis[0] the
 * steering, [1] the menus' vertical, [2] and [3] the pedals, each 0..0xffff (the steering 0x8000 at centre, a pedal
 * 0xffff released); keys: bit (1 << action) of ACT_HORN, ACT_PAUSE, ACT_VIEW and ACT_NITRO while their sources are down. */
typedef struct { uint32_t buttons; int32_t axis[4]; uint32_t keys; } VirtualPad;

/* Sources the device does not have (a number past its buttons or axes) are ignored. A wheel (not a gamepad) without
 * a controls.cfg (c->custom 0) passes its buttons and first four axes through as before. */
VirtualPad control_map_apply(const PortControls *c, const RawPad *raw);

/* Capture a binding by pressing it. rest: the device's photo when the capture began; now: the one now. The first
 * source that has moved enough, else SRC_NONE: a button newly pressed (not with want_axis), a hat direction newly
 * held (not with want_axis), or an axis moved more than half its travel from its rest toward the end farther away,
 * the most moved if several. An axis is SRC_AXIS_FULL when want_axis (steering) or when it rests at an end
 * (a wheel's pedal), SRC_AXIS_POS / SRC_AXIS_NEG by the way it moved when it rests near its middle (a gamepad's
 * trigger); its `rest` is the photo's value. */
ControlSource control_map_detect(const RawPad *rest, const RawPad *now, bool want_axis);

/* `src` as source `slot` (0 or 1) of `act`, taken from whatever other action (or slot) had it; the slots compact to
 * the first. A source is the same by kind and index (and a hat's direction). */
PortControls control_map_assign(const PortControls *c, PortAction act, int slot, ControlSource src);

/* control_map_assign, but a source taken from another action is replaced there by `old`, the source the action gave up
 * (a swap, so neither is left bare): not when `old` is none, or is one that action cannot use (a button for the steering
 * axis) or already has. */
PortControls control_map_swap(const PortControls *c, PortAction act, int slot, ControlSource src, ControlSource old);

/* port: the words the controls screen shows. A source's name: SDL_Gamepad's own ("A", "RT", "Left X") when `gamepad`,
 * else "Button 5", "Axis 2-", "Hat 0 Left"; " inv" after an inverted axis; "-" for none. Cut to `size`. */
void control_map_source_name(const ControlSource *s, int gamepad, char *out, size_t size);
/* an action's sources, both, separated by ", " ("-" for none) */
void control_map_binding_text(const PortControls *c, PortAction act, int gamepad, char *out, size_t size);
const char *control_map_action_name(PortAction act);

/* port: a force the game asks for, in percent (-100..100, or 0..100 for a rumble), scaled to `strength` percent (0..100,
 * clamped): 100 leaves it, 0 is nothing, 50 halves it; cut toward zero, never past 100 either way. */
int control_map_force(int percent_game, int strength);

#endif
