/* The replay's panel on the race screen (00471364 and below, from
 * 0044d640 while a replay plays): the camera buttons and their marks. */
#ifndef TNFS_REPLAY_PANEL_H
#define TNFS_REPLAY_PANEL_H

#include "game/race/dash_art.h"
#include "game/race/hud_mask.h"

#include <stdint.h>

enum { REPLAY_PANEL_CARS = 8, REPLAY_PANEL_FIRST_CAR_VIEW = 3, REPLAY_PANEL_GLYPHS = 36, REPLAY_PANEL_NAME = 8,
       REPLAY_PANEL_CAMERA_VIEW = 12 };


/* 004829b0 (cdecl x, y, width, height, colour): a solid rectangle */
typedef struct {
    void *ctx;
    void (*rect)(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t colour);
    void (*blit)(void *ctx, const uint8_t *shape, int32_t x, int32_t y);  /* 00473764 (cdecl): opaque at x, y */
    void (*blit_own)(void *ctx, const uint8_t *shape);                   /* 004737ec (cdecl): at its own place */
    void (*blit_at)(void *ctx, const uint8_t *shape, int32_t x, int32_t y);  /* 004737d0 (cdecl): opaque at x, y */
} ReplayPanelDrawOps;

/* 00471de8's words */
typedef struct {
    const uint8_t *buttons[REPLAY_PANEL_CARS];  /* 0052b6e8: the car views' button shapes (FSH entry headers) */
    int32_t high;                               /* 004fa0c0: 640 x 480 */
    int32_t view;                               /* 0052b8d4: the view taken */
    const uint8_t *colours;                     /* 0050b7d4: a colour by car, REPLAY_PANEL_CARS */
} ReplayPanelMarks;

/* 00471de8 (watcall ()): a mark in each car's colour on its view's button
 * (views 3..10): at the button's x (+0xc) + 4 (high) or 3, the first
 * button's y (+0xe) + 3; 5 x 6 (high) or 2 x 1; the view taken (0052b8d4)
 * one right, and in high resolution one down. */
void replay_panel_marks(const ReplayPanelMarks *in, const ReplayPanelDrawOps *ops);

/* 00471760's words */
typedef struct {
    const uint8_t *box;                            /* 0052b6a4: the name's box (an FSH entry header) */
    const uint8_t *glyphs[REPLAY_PANEL_GLYPHS];    /* 0052b774: '0'..'9', then 'a'..'z' */
    const char *name;                              /* 0052ad44 + 0x95 x 004fbb04: the shown car's driver */
    int32_t high;                                  /* 004fa0c0 */
    int32_t view_taken;                            /* 0052b8f8 */
} ReplayPanelName;

/* 00471760 (watcall ()): with 0052b8f8 0 the box at its own place
 * (004737ec). A name of 1..8 characters: each digit its glyph, each other
 * byte (unsigned) - 'a' (- 'A' when below 0) + 10 when that is 0..35,
 * else a gap of 10 (high) or 5; the glyphs' widths (+4) and the gaps
 * centred on the box (its x +0xc + its width / 2, each halved toward
 * minus infinity), each glyph drawn opaque (00473764) at its own y (+0xe). */
void replay_panel_name(const ReplayPanelName *in, const ReplayPanelDrawOps *ops);

/* 00471628's words: the panel's state (0052b8dc, the camera button, is ReplayArt.camera) */
typedef struct {
    int32_t view;      /* 0052b8d4: the view taken, 0 none */
    int32_t cursor;    /* 0052b8d8: the view the panel points at */
    int32_t driver;    /* 0052b8e0: the shown car whose name is drawn */
    int32_t lit;       /* 0052b8ec: the button drawn lit */
    int32_t redraw;    /* 0052b8f4 */
    int32_t whole;     /* 0052b8f8: the panel drawn whole */
    int32_t last;      /* 0052b8e8: the view taken the frame before (00471460) */
} ReplayPanel;

/* 00471628's callees drawn with their own words: 00471de8, 00471760 */
typedef struct {
    void *ctx;
    void (*marks)(void *ctx);
    void (*name)(void *ctx);
} ReplayPanelParts;

/* 00471628 (watcall ()) on ReplayArt (0052b69c, 00471180's): whole, the
 * frame ('repl', 'bard') and view 1's unlit button at their own place and a
 * redraw. A view taken: its pressed button, the cursor onto
 * it; none: when the cursor moved off the lit button or on a redraw, the
 * lit one drawn unlit and the cursor's lit (the lit one then the cursor).
 * The marks (00471de8); the camera button: the view 12 taken, the lit one
 * drawn (004737d0) one right and one down, else lit with the cursor on 12,
 * unlit otherwise; the name (00471760) when the shown car (004fbb04)
 * changed or whole. */
void replay_panel_buttons(ReplayPanel *p, const ReplayArt *art, int32_t viewed, const ReplayPanelDrawOps *ops,
                          const ReplayPanelParts *parts);

/* 00471364's calls */
typedef struct {
    void *ctx;
    void (*hide)(void *ctx);                                                  /* 00441938: the pointer put back */
    int32_t (*buttons)(void *ctx);                                            /* [004c6194]: the mouse's, unused */
    void (*select)(void *ctx, HudSurface s);                                  /* 00473680(004c2da0) / 004735dc */
    void (*clip)(void *ctx, int32_t left, int32_t right, int32_t top, int32_t bottom);  /* 0047383c */
    void (*draw)(void *ctx);                                                  /* 00471628 */
    void (*show)(void *ctx, int32_t pointer);                                 /* 004418a4 */
} ReplayPanelFrameOps;

/* 00471364 (watcall ()), from 0044d640 while a replay plays: the pointer
 * put back; with no view taken the mouse's buttons read (and dropped).
 * On a redraw or whole (0052b8f4, 0052b8f8): the screen selected (004b95b4
 * set: 004c2da0, else the default), clipped to the panel's strip (high
 * 0, 640, 448, 480; else 0, 320, 180, 200), the panel drawn (00471628),
 * both cleared. The pointer then drawn, the second (1) with a view taken
 * (above 0), else the first. */
void replay_panel_frame(ReplayPanel *p, int32_t high, int32_t back_buffer, const ReplayPanelFrameOps *ops);

/* 00471460's calls */
typedef struct {
    void *ctx;
    int32_t (*mouse)(void *ctx);                        /* 004718c0: a button clicked, nonzero */
    int32_t (*pick)(void *ctx);                         /* 00471a38: the keys, nonzero on a move or a take */
    void (*follow)(void *ctx, int32_t viewed);          /* 00404530(004f801c, 00509e1c[viewed], 4) */
    void (*engine)(void *ctx);                          /* 00444394 */
    void (*cycle)(void *ctx);                           /* 0040594c(004f801c): the camera's next kind */
    void (*point)(void *ctx, int32_t x, int32_t y);     /* 00441990: the pointer onto x, y */
} ReplayPanelInputOps;

/* 00471460's words outside the panel */
typedef struct {
    int32_t racers;    /* 004fb7dc */
    int32_t humans;    /* 004fb7e0 */
    int32_t viewed;    /* 004fbb04: the shown car */
    int16_t done;      /* 0052b314: written 1 by view 1 ('don') */
} ReplayPanelWorld;

/* 00471460 (watcall ()), the panel's input (from 0044f9b8 in a replay):
 * the mouse (004718c0), else the keys (00471a38, then the pointer follows).
 * Neither: a view taken is let go (the cursor onto it, a redraw). Else by
 * the view taken: 1 done; 2 / 11 (only when newly taken, 0052b8e8) the
 * shown car back / on, wrapping round the racers, followed (00404530); 3..10
 * the car 0..7 when not shown already, followed, its engine (00444394);
 * 12 when newly taken with a human's car shown, the camera's next kind
 * (0040594c). The view kept as the last, the cursor onto a view taken;
 * after the keys the pointer onto the cursor's button's middle (its box,
 * 0052b804, + half its unlit shape's width and height). */
void replay_panel_input(ReplayPanel *p, const ReplayArt *art, ReplayPanelWorld *w, const ReplayPanelInputOps *ops);

/* 004718c0's calls */
typedef struct {
    void *ctx;
    void (*range)(void *ctx, int32_t left, int32_t top, int32_t right, int32_t bottom);  /* 0047723c */
    int16_t (*buttons)(void *ctx);                                                      /* [004c6194] */
    /* 00463f80 (the menus' library): the pointer (004c6110, 004c6114) inside, nonzero */
    int32_t (*inside)(void *ctx, int16_t left, int16_t right, int16_t top, int16_t bottom);
} ReplayPanelMouseOps;

/* 004718c0's mouse words */
typedef struct {
    int16_t present;   /* 00524790: a mouse */
    int16_t buttons;   /* 00524792 */
    int32_t held;      /* 004c5f90: the buttons, widened */
    int32_t last_x;    /* 0052b900: the pointer the frame before */
    int32_t last_y;    /* 0052b8fc */
} ReplayPanelMouse;

typedef struct {
    int32_t x, y;      /* 004c6110, 004c6114: the pointer */
    int32_t high;      /* 004fa0c0 */
    int32_t racers;    /* 004fb7dc */
    int32_t humans;    /* 004fb7e0 */
    int32_t viewed;    /* 004fbb04 */
    int16_t replay;    /* 0052b316 */
} ReplayPanelMouseInputs;

/* 004718c0 (watcall ()): the pointer kept to the panel's strip (0047723c:
 * 0, 450, 640, 480 high; 0, 180, 320, 200); with a mouse its buttons read.
 * When the pointer moved or a button is down, each view button 1..12 that
 * is there (the cars' only for the racers, 12 only with a human's car
 * shown, 1 only in a replay) and under the pointer (00463f80 on its box,
 * as shorts): with no button down the cursor onto it, else the view taken
 * (the other let go, a redraw when it changes); 1 then. The pointer kept
 * as the last. */
int32_t replay_panel_mouse(ReplayPanel *p, const DashRect box[REPLAY_BUTTONS], ReplayPanelMouse *m,
                           const ReplayPanelMouseInputs *in, const ReplayPanelMouseOps *ops);

#endif
