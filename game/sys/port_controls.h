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
#include <stdint.h>

typedef enum {
    ACT_STEER,                          /* a whole axis (a controls.cfg of before; loaded as the halves of the next two, no row of its own) */
    ACT_STEER_LEFT, ACT_STEER_RIGHT,    /* buttons (the full lock) or the halves of a stick (in proportion): the virtual stick's x */
    ACT_GAS, ACT_BRAKE,                 /* pedals: the virtual axes 2 and 3 */
    ACT_GEAR_UP, ACT_GEAR_DOWN,         /* buttons bits 0 and 1 */
    ACT_CAMERA,                         /* bit 2 (PAD_HOLD, X today) */
    ACT_HANDBRAKE,                      /* bit 3 (Y today) */
    ACT_HORN, ACT_PAUSE,               /* virtual keys: the horn's key from config.dat, P */
    ACT_NITRO,                          /* virtual key: RACE_KEYS_PAD_NITRO, the NITRO cheat's start (start_keys.h) */
    ACT_LOOK_BACK,                      /* virtual key: RACE_KEYS_PAD_LOOK_BACK, held to turn the 360 camera about (port_follow_camera only) */
    ACT_COUNT
} PortAction;

/* The keyboard's driving keys, in the order of the race's key codes (input_setup.h: codes[0..7], and the hold key, the
 * camera's, last). A key is a set-1 scancode (platform_key_down's). */
typedef enum {
    PKEY_LEFT, PKEY_RIGHT, PKEY_GAS, PKEY_BRAKE, PKEY_GEAR_UP, PKEY_GEAR_DOWN, PKEY_HANDBRAKE, PKEY_HORN, PKEY_CAMERA,
    PKEY_RACE_COUNT,   /* the keys above are the race's key codes (input_setup.h); config.dat has a word for each */
    PKEY_NITRO = PKEY_RACE_COUNT,       /* the port's own: the NITRO cheat's start (start_keys.h), N until it is the player's */
    PKEY_LOOK_BACK,                     /* the port's own: the 360 camera's look back, B until it is the player's. Last, so hiding its row leaves no gap */
    PKEY_COUNT
} PortKey;

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
    int keys_custom;                     /* the keyboard's keys below are the player's: 0 leaves config.dat's (the exe's own
                                          * Controls page, port_controls_key_defaults) to the race */
    int key[PKEY_COUNT];                 /* per PortKey, with keys_custom: the scancode, 0 for none */
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
/* The keyboard's keys: "key_left", "key_right", "key_gas", "key_brake", "key_gear_up", "key_gear_down",
 * "key_handbrake", "key_horn", "key_camera", "key_nitro", "key_look_back" a line each with a scancode (decimal, or 0x..; empty or 0 for none); the
 * file has them all once any is the player's (keys_custom), and none otherwise. */
const char *port_controls_key_name(PortKey key);   /* the screen's word for it: "Steer left" */
/* config.dat's keys (the exe's Controls page, the 13 shorts 0052ad26..: words 3..12) as scancodes, what the race uses
 * while keys_custom is 0; `german` the language short */
void port_controls_key_defaults(const int16_t *config, int german, int out[PKEY_COUNT]);
/* the key now: the player's with keys_custom, else `defaults` */
int port_controls_key(const PortControls *c, const int defaults[PKEY_COUNT], PortKey key);
/* `scancode` (0: none) to `key`; the player's keys start from `defaults` the first time. A row that had that key is
 * given the key `key` had (a swap, so no control is left without one unless that one had none): its PortKey is
 * returned, -1 when there was none. */
int port_controls_set_key(PortControls *c, const int defaults[PKEY_COUNT], PortKey key, int scancode);
/* the config.dat short (the exe's Controls page, words 3..12) a driving key is, -1 for a word that is not one */
int port_controls_key_of_word(int word);
/* the keyboard's keys back to config.dat's */
void port_controls_reset_keys(PortControls *c);
/* keys the game keeps for itself, not offered to a driving key: Escape, the function keys, P (pause), the pad's
 * virtual keys */
bool port_controls_key_reserved(int scancode);
/* the race's key codes (codes[0..PKEY_RACE_COUNT-1], input_setup.h's order) with the player's keys over them; nothing
 * written while keys_custom is 0 */
void port_controls_apply_keys(const PortControls *c, int32_t codes[PKEY_RACE_COUNT]);
/* the look-back key's scancode: the player's with keys_custom, else B (config.dat has no word for it) */
int port_controls_look_back_key(const PortControls *c);
/* the NITRO start's key's scancode: the player's with keys_custom, else N (config.dat has no word for it) */
int port_controls_nitro_key(const PortControls *c);

PortControls port_controls_load(void);   /* user file "controls.cfg"; the defaults (custom 0) when it does not exist */
bool port_controls_save(const PortControls *c);

#endif
