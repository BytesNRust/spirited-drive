/* The followed player's controls read from the keys, the pad or the mouse
 * (0043915c, from the poll 004394dc) as an input word (race_state.h's
 * inputs: steering bits 0..5, pedal 6..11, gear up / down 0x1000 / 0x2000,
 * look back 0x4000, handbrake 0x8000). */
#ifndef TNFS_INPUT_READ_H
#define TNFS_INPUT_READ_H

#include "game/race/mouse_steer.h"
#include "game/race/race_state.h"

#include <stdint.h>

enum {
    INPUT_KEY_LEFT, INPUT_KEY_RIGHT, INPUT_KEY_ACCELERATE, INPUT_KEY_BRAKE,
    INPUT_KEY_GEAR_UP, INPUT_KEY_GEAR_DOWN, INPUT_KEY_LOOK_BACK, INPUT_KEY_HANDBRAKE,
    INPUT_KEYS
};

typedef struct {
    int32_t x, y;           /* 004fce54 -0x20..0x1f, 004fce58 */
    uint8_t buttons;        /* 004fce68: bit 0 accelerate / gear up, 1 brake / gear down, 3 look back */
} InputPad;

typedef struct {
    void *ctx;
    /* 0043d598 (eax player, dx stand-in): the pad read into InputPad */
    void (*pad_read)(void *ctx, int32_t player, int16_t stand_in);
    MouseSteerOps mouse;    /* 004380e8's */
} InputReadOps;

typedef struct {
    const uint8_t *keys;              /* 004c693c: a flag per key code */
    const int32_t *key_codes;         /* 004fb6dc: INPUT_KEYS codes */
    const int16_t *stand_in;          /* 004f82a0[player] */
    const ControlSetup *controls;     /* 004fb724[player] */
    const RaceEntry *entries;         /* 004fb7fa[player] */
    int32_t player;                   /* 004fbb08 */
    int32_t mirrored;                 /* 004fbad8 */
    int32_t pad_present;              /* 004fcdcc */
} InputReadInputs;

/* 0043915c (no arguments), returns the word. The stand-in short of the
 * followed player 1: 0xffff87a0. Else 0043d598, then:
 * - steering by keys: left alone 0 (0x3c mirrored), right alone 0x3c (0
 *   mirrored), else 0x20; the pad: x + 0x20, mirrored 0x20 - x after x
 *   0x1f or -0x20 is moved one up (written back); else the mouse's (004380e8).
 * - pedal 0x1e: by keys accelerate 5, brake 0x3c over it; the pad's axis
 *   y + 0x1e; its buttons (3) bit 0 5, bit 1 0x3c over it; else the mouse
 *   buttons 1 -> 5, 2 -> 0x3c.
 * - gears 0: by keys up 1, else down 2; the pad's buttons (3) bit 0 1,
 *   else bit 1 2; else the mouse buttons 1 or 2. Keys 0x30, 0x13 and 0x11
 *   held with the slot's entry automatic: 3.
 * - look back (gears | 8): its key, or a pad present with button bit 3.
 * - handbrake (| 4): its key, or a pad present with neither the pedals nor
 *   the gears on its buttons (3) and button bit 0.
 * The mouse (004380e8) runs once at most: the pedals and gears use its
 * buttons from the steering's call. The word: gears << 12 | steer | pedal
 * << 6, ORed as 32-bit values. */
uint32_t input_read(const InputReadOps *ops, InputPad *pad, MouseSteerWords *mouse, const InputReadInputs *in);

#endif
