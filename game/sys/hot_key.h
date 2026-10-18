/* The game loop's key of a frame (00464ef4), with its hot keys: Ctrl+F10
 * drops a replay, Ctrl+Insert saves a screenshot, F5 mutes the sound. */
#ifndef TNFS_HOT_KEY_H
#define TNFS_HOT_KEY_H

#include "game/sys/joystick.h"
#include "game/sys/menu_key.h"

#include <stdint.h>

enum { HOT_KEY_SHOT_NAME = 16 };  /* 005247f0: "nfs-32768.tga" at most */

typedef struct HotKeyState {
    int16_t playback;                   /* 0052b312 */
    int16_t dropped;                    /* 0052b314 */
    int32_t debug;                      /* 004c787c: screenshots only when set */
    int16_t shot;                       /* 004c5cc0: the next screenshot's number */
    char shot_name[HOT_KEY_SHOT_NAME];  /* 005247f0 */
} HotKeyState;

typedef struct HotKeyOps {
    void *ctx;
    void (*screenshot)(void *ctx, const char *name);  /* 0049290c (cdecl) */
    void (*mute)(void *ctx);                          /* 00464a68 */
} HotKeyOps;

/* 00464ef4 (from 004015dc; the flags 004c693c read as 00483f8c does):
 * F10 (0x44) and Ctrl (0x1d): dropped = 1 in playback, 0. Insert (0x52),
 * Ctrl and debug: sprintf(shot_name, "nfs%02d.tga", shot), screenshot, shot
 * + 1, 0. F5 (0x3f): mute, 0. Else the first of escape, space, return, up,
 * down, left, right, insert, delete whose key code (00487b84) is down;
 * else the first of the scancodes 0x2a, 0x36, 0x1d, 0x38, 0x15, 0x31, 0x24
 * down; else 0 with tab's code down; else the joystick's keys (0043d4c0):
 * its button key, else its direction key; else 0. */
int32_t hot_key(HotKeyState *s, const uint8_t key_flags[KEY_FLAGS], JoyState *j, int32_t present, int32_t device,
                const HotKeyOps *ops, const JoyOps *joy_ops);

/* 00464bd0 (the menus' key): hot_key without the screenshot, and F1..F4 (0x3b00..0x3e00, by their key codes) after
 * the named keys while function_keys (the short 004f9e8c, the graphics screen up) is set. */
int32_t menu_hot_key(HotKeyState *s, const uint8_t key_flags[KEY_FLAGS], int16_t function_keys, JoyState *j,
                     int32_t present, int32_t device, const HotKeyOps *ops, const JoyOps *joy_ops);

#endif
