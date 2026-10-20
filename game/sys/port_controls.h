/* port: what the gamepad's or wheel's buttons and axes do in the race (controls.cfg), kept apart from config.dat's
 * keys. The race itself is unchanged: control_map.h turns these bindings into the virtual joystick the original reads.
 *
 * The actions (settled from the binary, 00438f00 and the key readers, not asked): the original's input word has a
 * horn (0x4000: car +0x4ba, sounded on channel 14 by 0047217c) and a handbrake (0x8000: car +0x3f5) and no look-back;
 * the pad's buttons bits are 0 gear up, 1 gear down, 2 camera (the "hold" key of race_keys.c, the controls screen's
 * CAMERA), 3 handbrake. The horn, the pause (P) and the view (F1) are keys: a binding presses them as a virtual key.
 * Not offered: F2..F9, the cheats and the camera numpad. */
#ifndef TNFS_PORT_CONTROLS_H
#define TNFS_PORT_CONTROLS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ACT_STEER,                          /* an axis: the virtual stick's x */
    ACT_STEER_LEFT, ACT_STEER_RIGHT,    /* buttons that take x to an end (the d-pad today) */
    ACT_GAS, ACT_BRAKE,                 /* pedals: the virtual axes 2 and 3 */
    ACT_GEAR_UP, ACT_GEAR_DOWN,         /* buttons bits 0 and 1 */
    ACT_CAMERA,                         /* bit 2 (PAD_HOLD, X today) */
    ACT_HANDBRAKE,                      /* bit 3 (Y today) */
    ACT_HORN, ACT_PAUSE, ACT_VIEW,      /* virtual keys: the horn's key from config.dat, P, F1 */
    ACT_COUNT
} PortAction;

typedef enum { SRC_NONE, SRC_BUTTON, SRC_AXIS_POS, SRC_AXIS_NEG, SRC_AXIS_FULL, SRC_HAT } SourceKind;

/* index: the button, axis or hat; rest: an axis's value when untouched (-32768..32767), a hat's direction mask (SDL's
 * up 1, right 2, down 4, left 8); invert: the axis the other way */
typedef struct { SourceKind kind; int index; int rest; int invert; } ControlSource;

typedef struct {
    ControlSource bind[ACT_COUNT][2];   /* up to two sources an action (a stick and the d-pad) */
    int deadzone;                        /* 0..50 %; 0 */
    int sensitivity;                     /* 50..200 %; 100 */
    int ffb_strength;                    /* 0..100 %; 100 */
    int custom;                          /* set by port_controls_load when a controls.cfg was read */
} PortControls;

/* Today's fixed pad mapping (platform_joy_sdl3.c's pad_read), in SDL_Gamepad's button and axis numbers. The dead zone
 * is 0 (the original's pad has none) so that a pad without a controls.cfg behaves exactly as it did. */
PortControls port_controls_defaults(void);
/* "key=value" a line, keys steer, steer_left, steer_right, gas, brake, gear_up, gear_down, camera, handbrake, horn,
 * pause, view, deadzone, sensitivity, ffb_strength; a source is button5, axis0 (a whole axis), axis5+ / axis2- (a
 * half), hat0.8 (a hat's direction), with @rest after an axis and ~ before to invert, two separated by a comma.
 * Unknown keys and values out of range or malformed are ignored (the default stays); an empty value clears. */
PortControls port_controls_parse(const char *text, size_t size);
size_t port_controls_format(const PortControls *c, char *out, size_t size);   /* 0 when it does not fit */
PortControls port_controls_load(void);   /* user file "controls.cfg"; the defaults (custom 0) when it does not exist */
bool port_controls_save(const PortControls *c);

#endif
