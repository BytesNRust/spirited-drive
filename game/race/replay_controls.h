/* The replay controls' input (0044f55c, from 0044f9b8 while a replay
 * plays): the pointer over the eight buttons, the menu keys, Esc, and the
 * button pressed this frame. */
#ifndef TNFS_REPLAY_CONTROLS_H
#define TNFS_REPLAY_CONTROLS_H

#include <stdint.h>

enum { REPLAY_INPUT_BUTTONS = 8, REPLAY_INPUT_SPEEDS = 4 };

typedef struct {
    void *ctx;
    void (*window_size)(void *ctx, int32_t a, int32_t b, int32_t c, int32_t d);  /* 0047723c (cdecl) */
    int16_t (*buttons)(void *ctx);                                       /* [004c6194]: the mouse's buttons */
    int32_t (*in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    int32_t (*key)(void *ctx);                                           /* 00465358 */
    void (*place)(void *ctx, int32_t x, int32_t y);                      /* 00441990: the pointer moved there */
} ReplayInputOps;

/* Read again after every call */
typedef struct {
    int32_t high;                      /* 004fa0c0 */
    int16_t mouse;                     /* 00524790: the mouse is there */
    int16_t mouse_buttons;             /* 00524792 */
    int32_t buttons_seen;              /* 004c53c8 */
    int32_t pointer_x, pointer_y;      /* 004c6110, 004c6114 */
    int32_t last_x, last_y;            /* 005137ec, 005137e8 */
    int32_t pressed;                   /* 00513888: the button pressed, 0 none */
    int32_t cursor;                    /* 0051388c: the button the panel points at */
    int32_t redraw;                    /* 00513898 */
    int32_t kind;                      /* 00513730: 5 leaves button 8 out */
    int32_t rects[REPLAY_INPUT_BUTTONS + 1][4];   /* 005137f8 + 16 i: x0, y0, x1, y1 (1..8) */
    const uint8_t *shapes[REPLAY_INPUT_BUTTONS + 1];  /* 00513ca4[i]: FSH entry headers (+4 width, +6 height) */
    int32_t last_key;                  /* 004c53c4 */
    uint8_t escape;                    /* 004c693d: Esc's key flag */
    int32_t timer;                     /* 004bb8d8 */
    int32_t request;                   /* 00513738 */
    int32_t speed;                     /* 0051389c: the speed button's step, 0..3 */
    int32_t button;                    /* 0051373c */
    int32_t last_pressed;              /* 004c53cc */
    int32_t phase;                     /* 004c5390 */
    int32_t unknown_5138a0;
} ReplayInput;

/* 0044f55c (watcall ()): 0047723c(0, 0xb4, 0x140, 0xc8), high (0, 0x1c2,
 * 0x280, 0x1e0); with the mouse the buttons read; 004c53c8 = them. When the
 * pointer moved, a button is held or the cursor is on 6: each button 1..8
 * (8 not with 00513730 5) under the pointer (00463f80): no button held, the
 * cursor onto it (pressed 0); held, pressed it (cursor 0); redraw on a
 * change. The pointer kept. A new key (00465358, not 004c53c4): left the
 * cursor back (1 wraps to 8, 8 to 7 with 00513730 5), right on (9 to 1, 8 to
 * 1 with 5), the pointer onto the button's centre (00441990); space or
 * return press the cursor's; Esc with 004bb8d8 above 15 presses 1. Nothing
 * under the pointer or keyed: a pressed button becomes the cursor. Pressed
 * 1: 00513738 = 7; 2..5: 0051373c 1, 3, 4, 2; 6 newly: in phase 2 the speed
 * steps (3 wraps to 1), 0051373c = 004c5374[speed] (6, 6, 7, 8), and
 * 005138a0 = 1; 7 newly: 5; 8 newly: 10; 1..5 and a new 7 the speed 0.
 * 004c53cc = pressed. Port guards: a speed past 004c5374's four leaves 0051373c, a
 * cursor past 8 or a missing shape moves no pointer. */
void replay_controls_input(ReplayInput *in, const ReplayInputOps *ops);

#endif
