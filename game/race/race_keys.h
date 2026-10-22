/* The race's key commands (004381f8, from 0045597c each frame): the car to
 * watch, the reset, the function keys and the camera keys' bits. */
#ifndef TNFS_RACE_KEYS_H
#define TNFS_RACE_KEYS_H

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*follow)(void *ctx, int32_t slot);  /* 00404530(004f801c, 00509e1c[slot], 4) */
    void (*viewed_changed)(void *ctx);        /* 00444394 */
    void (*follow_own)(void *ctx);            /* 0040594c(004f801c) */
    void (*own_changed)(void *ctx);           /* 0044f9ac */
    void (*reset)(void *ctx, int32_t player); /* 0041019c */
    void (*f1)(void *ctx);                    /* 00405324 */
    void (*screen)(void *ctx, int32_t kind);  /* 00405370 */
    void (*detail)(void *ctx, int32_t value); /* 0044c1bc */
    void (*f5_on)(void *ctx);                 /* 00443768 */
    void (*f5_off)(void *ctx);                /* 0044379c */
} RaceKeysOps;

/* port: the pad's View button as a key (platform_virtual_key's scancode for ACT_VIEW while port_bumper_keys is on) */
enum { RACE_KEYS_PAD_VIEW = 0x59 };

typedef struct {
    const uint8_t *keys;      /* 004c693c: a flag per key code */
    int32_t hold_key;         /* 004fb6fc: the ninth key code */
    int32_t pad_present;      /* 004fcdcc */
    uint8_t pad_buttons;      /* 004fce68: bit 2 the hold button */
    int32_t humans;           /* 004fb7e0 */
    int32_t phase;            /* 004c5390 */
    int32_t w513730;          /* 00513730 */
    int32_t race_type;        /* 004fb7d0 */
    int16_t replay;           /* 0052b316 */
    int16_t playback;         /* 0052b312 */
    int32_t timer_8d8;        /* 004bb8d8 */
    int32_t player;           /* 004fbb08 */
    int16_t w4c5ccc;          /* 004c5ccc */
    int32_t high;             /* 004fa0c0 */
    int32_t port_bumper_keys; /* port: 1 leaves F1, F2 and F4 to the bumper mirror and radar (F4 to none) and has the
                               * pad's View (RACE_KEYS_PAD_VIEW) change the view; 0 is the exe's */
} RaceKeysInputs;

typedef struct {
    int32_t viewed;           /* 004fbb04 */
    int32_t view_changed;     /* 0052b8f8 */
    int32_t hold_armed;       /* 004bb080 */
    int32_t fkey_held;        /* 004bb084: a function key was held */
    uint8_t w4c2d80;          /* 004c2d80 */
    uint8_t detail;           /* 004fb785 */
    uint8_t w4fb786;          /* 004fb786 */
    uint8_t screen_kind;      /* 004fb789 */
    uint8_t clear_view;       /* 004fb78a */
    uint8_t hidden;           /* 004fb78b */
    int32_t f5;               /* 004bafe0 */
    uint32_t camera_keys;     /* 004fb7b4 */
} RaceKeysState;

/* 004381f8 (no arguments).
 * - The hold key or the pad's bit 2 (pad present): when armed, the viewed
 *   car a human's (viewed < humans) and phase 1 or 00513730 != 5:
 *   follow_own, own_changed; then disarmed. Else Alt (0x38) and Ctrl
 *   (0x1d) with 1..8 (keys 2..9; 3..8 need race type > 1 or right Shift
 *   0x36): viewed = n, view_changed = 1, follow(n), viewed_changed; without
 *   Alt and Ctrl: armed.
 * - Outside a replay, keypad 1 (0x4f) with a Shift (0x2a, 0x36), 004bb8d8
 *   past 100 with playback 0 or past 400: reset(player).
 * - 004c5ccc 0, Alt and M (0x32): 004c2d80 ^= 1.
 * - Without Ctrl, the first function key held acts when none was held
 *   before: F1 f1, clear_view on screen kind 3, view_changed = 1; F2 the
 *   screen kind + 1 (& 3 as bytes), 0 past 1 at low resolution (the Ctrl
 *   case is unreachable), screen(kind), clear_view on 3; F3 detail + 1 mod
 *   3, detail(0x1c, 0xe, 9); F4 004fb786 = 1 - it; F5 f5 = 1 - it, then
 *   f5_on or f5_off; F6 hidden = 1 - it. None held: no key held.
 * - camera_keys from the keypad and others: 0x4f 0x800000, 0x52 0x1000000,
 *   0x48 0x40000000, 0x50 0x80000000, 0x4b 0x10000000, 0x4d 0x20000000,
 *   0x49 0x400000, 0x51 0x200000, 0x39 0x8000000, 0x1c 0x4000000, 0x0f
 *   0x2000000. */
void race_keys(const RaceKeysOps *ops, RaceKeysState *st, const RaceKeysInputs *in);

/* A race's start: 004381f8's own words stay as the race before left them (globals no start writes): the hold key's
 * arming (004bb080, 1 as the exe starts: `was` NULL), the function key held (004bb084), Alt + M (004c2d80), F6's
 * marks off (004fb78b) and F5's sound off (004bafe0); the rest of st starts at 0. */
void race_keys_start(RaceKeysState *st, const RaceKeysState *was);

#endif
