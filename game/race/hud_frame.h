/* The HUD's frame outside the car (00422598, from 0044cc74 when the camera is
 * not in the car): the stop notice, or the dash reloaded, the full redraw
 * when asked for, the view surface to the screen, the keys, the top row,
 * the needle, the map and the panels. */
#ifndef TNFS_HUD_FRAME_H
#define TNFS_HUD_FRAME_H

#include "game/race/hud_mask.h"

#include <stdint.h>

enum {
    HUD_FRAME_MAP_X_HIGH = 0x206, HUD_FRAME_MAP_Y_HIGH = 0x130,  /* 004737d0's place for the map, 640 x 480 */
    HUD_FRAME_MAP_X_LOW = 0x103, HUD_FRAME_MAP_Y_LOW = 0x7e      /* 320 x 200 */
};

/* 00422598's callees; the car (eax) is the context's. */
typedef struct {
    void *ctx;
    void (*police)(void *ctx);                      /* 004214e4 */
    void (*reselect)(void *ctx);                    /* 00405260 */
    void (*panel)(void *ctx);                       /* 0044d640 */
    void (*reload)(void *ctx);                      /* 004217a8 */
    void (*clear_pages)(void *ctx);                 /* 00464a30: both pages black */
    void (*select)(void *ctx, HudSurface s);        /* 00473680(004c2da0) / 004735dc */
    void (*tiles)(void *ctx);                       /* 004218d0 */
    void (*select_front)(void *ctx);                /* 00473604 */
    void (*show)(void *ctx);                        /* 004737ec([004c2da0 + 0x2c]) */
    void (*video_flip)(void *ctx);                  /* 004733ec */
    void (*mask)(void *ctx, int32_t changed);       /* 00421c18 */
    void (*keys)(void *ctx);                        /* 00420710 */
    void (*top_row)(void *ctx);                     /* 00421e40(car) */
    void (*restore)(void *ctx);                     /* 00473614 */
    void (*needle)(void *ctx);                      /* 0042221c(car) */
    void (*map)(void *ctx, int32_t x, int32_t y);   /* 004737d0([004c2d9c + 0x2c], x, y) */
} HudFrameOps;

typedef struct {
    int32_t unknown_509d70;     /* set: nothing at all */
    int32_t ticket, warning;    /* 004b92dc, 004b92e0: the stop notices */
    int32_t back_buffer;        /* 004b95b4 */
    int32_t use_parts;          /* 004f9b3c: the map placed */
    int32_t high;               /* 004fa0c0 */
} HudFrameInputs;

/* The words 00422598 writes that live in other functions' states. */
typedef struct {
    int32_t *redraw;            /* 004b9568: the full redraw asked for */
    int32_t *loading;           /* 004fcdac */
    int32_t *notice_tick;       /* 004b92e4 */
} HudFrameWords;

/* 00422598 (watcall car, changed): nothing with 00509d70 set. A stop notice
 * up: police, reselect, panel, the redraw asked for, done. Else 004fcdac 1
 * around reload, then 0 (00509d70's value); the notice tick -1000; the
 * redraw when asked for (back buffer: clear_pages, select the screen, tiles,
 * select_front, show, video_flip; else select the default, tiles), then not;
 * mask(changed), keys, top_row; then needle, the map at (0x206, 0x130) high
 * / (0x103, 0x7e) low while use_parts, panel, each of the four after
 * restore and select the default without a back buffer. */
void hud_frame(const HudFrameOps *ops, const HudFrameWords *w, const HudFrameInputs *in, int32_t changed);

#endif
