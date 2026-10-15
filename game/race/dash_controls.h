/* The replay's controls on the dash (00450d60, from 0044d640 outside a live
 * race): the buttons, the camera and speed buttons and the clock's bar. */
#ifndef TNFS_DASH_CONTROLS_H
#define TNFS_DASH_CONTROLS_H

#include "game/race/dash_art.h"
#include "game/race/replay_panel.h"

#include <stdint.h>

enum {
    DASH_PRESSED = 0,   /* 00513888: the control taken, 0 none */
    DASH_CURSOR = 1,    /* 0051388c: the control under the cursor */
    DASH_VIEW = 2,      /* 00513890: the camera shown, 0..7 */
    DASH_SPEED = 3,     /* 00513894: the speed shown, 0..2 */
    DASH_REDRAW = 4,    /* 00513898 */
    DASH_CONTROL_DONE = 1, DASH_CONTROL_SPEED = 6, DASH_CONTROL_VIEW = 8
};

/* 00463f80 (the menus' library): the pointer inside left, right, top,
 * bottom (as shorts), nonzero */
typedef int32_t (*DashInside)(void *ctx, int16_t left, int16_t right, int16_t top, int16_t bottom);

/* 00450ac4 (watcall ()): 'repl' at its place; the control taken drawn
 * pressed, 'don' (1) at low resolution in English (0052b31c 0) placed 10
 * right and 1 down; else the one under the cursor lit, 'don' so (placed 10
 * right) or another only while the pointer is in its box. The camera and
 * speed buttons (controls 8 and 6): taken, lit placed one right and down;
 * under the cursor, lit while the pointer is in its box; else unlit. 'don'
 * unlit unless under the cursor (high at its place, low English placed 10
 * right and 1 down, low German not at all). */
void dash_controls_buttons(const DashArt *art, int32_t high, int16_t german, const ReplayPanelDrawOps *ops,
                           DashInside inside);

/* 00450d60's calls */
typedef struct {
    void *ctx;
    void (*hide)(void *ctx);                                                  /* 00441938 */
    int32_t (*buttons)(void *ctx);                                            /* [004c6194]: the mouse's, unused */
    void (*clip)(void *ctx, int32_t left, int32_t right, int32_t top, int32_t bottom);  /* 0047383c */
    void (*controls)(void *ctx);                                              /* 00450ac4 */
    void (*time)(void *ctx, int32_t ticks, int32_t x, int32_t y, int32_t step, int32_t space);  /* 0041e794 */
    void (*rect)(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t colour);       /* 004829b0 */
    void (*show)(void *ctx, int32_t pointer);                                 /* 004418a4 */
} DashControlsFrameOps;

typedef struct {
    int32_t high;           /* 004fa0c0 */
    int32_t tick;           /* 00513e70: the replay's clock */
    int32_t end_clock;      /* 005120c8: the recording's last tick */
    uint32_t played;        /* 0050b79c: the bar's played part (dark green) */
    uint32_t rest;          /* 0050b7b4: the rest (black) */
} DashControlsFrameInputs;

/* 00450d60 (watcall ()): the pointer put back; with no control taken the
 * mouse's buttons read (and dropped). A redraw (00513898, cleared): the
 * panel's strip clipped (0, 640, 448, 480 high; 0, 320, 180, 200), the
 * buttons (00450ac4), the clock and the bar; else only when the clock
 * moved off the one drawn (00513dac): the clock and the bar without the
 * buttons. The clock (0041e794) at 0x41, 0x1c5 (steps 13, 7) high, 0x2d,
 * 0xba (6, 2) low; the bar from 0x41, 0x1d8, 241 wide, 6 high (0x2d, 0xc3,
 * 122, 3): tick x 241 (122) / (end + 1) played, the rest black. The clock
 * kept as drawn; the pointer drawn, the second with a control taken. */
void dash_controls_frame(int32_t words[DASH_STATE_WORDS], int32_t *drawn_tick, const DashControlsFrameInputs *in,
                         const DashControlsFrameOps *ops);

#endif
