/* The race's heads-up display (00422598 from the frame draw 0044cc74): the
 * top row of times, position and lap over the view. */
#ifndef TNFS_HUD_H
#define TNFS_HUD_H

#include "game/gfx/quad.h"

#include <stdint.h>

enum {
    HUD_START_TICKS = 300,  /* the clock counts from here (the countdown) */
    HUD_MAX_DIGITS = 10     /* 0041e55c's buffer */
};

/* 004f9a0c..004f9a14: the race clock 0041e48c splits. */
typedef struct {
    int32_t tenths;     /* 004f9a0c */
    int32_t seconds;    /* 004f9a10 */
    int32_t minutes;    /* 004f9a14 */
} HudClock;

/* 0041e48c: ticks (60 a second) past HUD_START_TICKS into minutes, seconds
 * and tenths; 0 before. */
void hud_clock_split(HudClock *clock, int32_t ticks);

/* 0041e55c: `value` in `digits` digits, the most significant first, each
 * shape digit_shapes[d] drawn opaque (00473764) at x + i * step, y. The
 * original overruns its buffer past 10 digits and reads before the table
 * for a negative value; the port draws nothing then. */
void hud_draw_number(const QuadTarget *t, uint8_t *const *digit_shapes, int32_t value, int32_t x, int32_t y,
                     int32_t step, int32_t digits);

#endif
