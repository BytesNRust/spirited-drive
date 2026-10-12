/* The race's heads-up display (00422598 from the frame draw 0044cc74): the
 * top row of times, position and lap over the view. */
#ifndef TNFS_HUD_H
#define TNFS_HUD_H

#include "game/gfx/quad.h"
#include "game/race/car.h"
#include "game/race/dash_art.h"
#include "game/race/race_state.h"

#include <stdint.h>

enum {
    HUD_START_TICKS = 300,  /* the clock counts from here (the countdown) */
    HUD_MAX_DIGITS = 10,    /* 0041e55c's buffer */
    HUD_SPEED_DIGITS = 3,
    HUD_BLANK = 10,         /* the blank digit shape, and "shown blank" */
    HUD_SPLITS = 6,
    HUD_GAP_UNSET = -99
};

/* 004f9a0c..004f9a14: the race clock 0041e48c splits. */
typedef struct {
    int32_t tenths;     /* 004f9a0c */
    int32_t seconds;    /* 004f9a10 */
    int32_t minutes;    /* 004f9a14 */
} HudClock;

/* 004f9a0c..004f9acf: the HUD's state, word for word. A *_shown word is
 * what is on screen now: a digit is drawn again only when it changes. */
typedef struct {
    HudClock clock;                     /* 004f9a0c */
    int32_t unknown_4f9a18;
    int32_t mode;                       /* 004f9a1c: 0, 1, 2 pick the top row's layout */
    int32_t unknown_4f9a20[4];          /* 004f9a20..004f9a2c */
    int32_t gap_shown;                  /* 004f9a30: HUD_GAP_UNSET until the gap is first drawn */
    int32_t speed_shown[HUD_SPEED_DIGITS]; /* 004f9a34: red digits, HUD_BLANK for a blank */
    int32_t unknown_4f9a40;
    int32_t tenths_shown;               /* 004f9a44 */
    int32_t seconds_shown;              /* 004f9a48 */
    int32_t minutes_shown;              /* 004f9a4c */
    int32_t unknown_4f9a50[6];          /* 004f9a50..004f9a64 */
    int32_t place_shown;                /* 004f9a68 */
    int32_t unknown_4f9a6c;             /* set to HUD_GAP_UNSET with the gap */
    int32_t unknown_4f9a70;             /* likewise */
    int32_t unknown_4f9a74;
    int32_t unknown_4f9a78;             /* likewise */
    int32_t gear_shown;                 /* 004f9a7c */
    int32_t unknown_4f9a80[4];          /* 004f9a80..004f9a8c */
    int32_t redraw;                     /* 004f9a90 */
    int32_t unknown_4f9a94[3];          /* 004f9a94..004f9a9c */
    int32_t split_shown[HUD_SPLITS];    /* 004f9aa0 */
    int32_t split[HUD_SPLITS];          /* 004f9ab8: 0..2 times in ticks, 3..5 counts */
} HudState;

/* 0041e48c: ticks (60 a second) past HUD_START_TICKS into minutes, seconds
 * and tenths; 0 before. */
void hud_clock_split(HudClock *clock, int32_t ticks);

/* 0041e55c: `value` in `digits` digits, the most significant first, each
 * shape digit_shapes[d] drawn opaque (00473764) at x + i * step, y. The
 * original overruns its buffer past 10 digits and reads before the table
 * for a negative value; the port draws nothing then. */
void hud_draw_number(const QuadTarget *t, uint8_t *const *digit_shapes, int32_t value, int32_t x, int32_t y,
                     int32_t step, int32_t digits);

/* 0041e5ec: as hud_draw_number, but leading zeros (not the last digit)
 * are drawn with digit_shapes[14] (the yellow blank, table + 0x38). */
void hud_draw_number_blank(const QuadTarget *t, uint8_t *const *digit_shapes, int32_t value, int32_t x, int32_t y,
                           int32_t step, int32_t digits);

/* Where a HUD number goes: its first digit and the step to the next. */
typedef struct {
    int32_t x, y, step;
} HudPlace;

/* 0041e8d0: ticks (0 past 1200) as seconds (two digits) and tenths (one)
 * with hud_draw_number_blank and the yellow digits, at `high` (004fa0c0
 * set; the tenths 8 pixels past the seconds) or `low` (2 pixels past). */
void hud_draw_seconds(const QuadTarget *t, uint8_t *const *yellow, int32_t ticks, int32_t high_res, HudPlace high,
                      HudPlace low);

/* 0041e9a0 (top row layout 2): each split that changed since shown,
 * redrawn: 0..2 as seconds and tenths (hud_draw_seconds), 3..5 in three
 * yellow digits, leading zeros blank (in the order 3, 5, 4). */
void hud_draw_splits(const QuadTarget *t, HudState *hud, uint8_t *const *yellow, int32_t high_res);

/* 0041f2a8: the gap to the rival (RaceSplits.gap, ticks) as m:ss.t in
 * yellow digits at x (minutes), x + step + space (seconds), then the
 * tenths a further 2 steps + space on. While hud->gap_shown is
 * HUD_GAP_UNSET the 'splt' shape and the sign are drawn first at their
 * own places (yellow 'mnus' for a gap >= 0, 'plus' below); the sign again
 * when it changes; the digits only when the gap changes. The clock words
 * are left holding the gap's minutes, seconds and tenths. */
void hud_draw_gap(const QuadTarget *t, HudState *hud, const DashArt *dash, int32_t gap, int32_t x, int32_t y,
                  int32_t step, int32_t space);

/* 0041ee8c: the followed car's speed in three red digits (dash->red) at
 * x + i * step, y, leading zeros blank (dash->red[HUD_BLANK]) but the last:
 * |forward speed| (+0x68 while race->viewed < race->humans, else the
 * computer speed +0x15c) x 2.24 (mph) or x 3.6 (km/h, German), rounded,
 * whole units. The original reads before the table for the most negative
 * speed; the port draws nothing then. */
void hud_draw_speed(const QuadTarget *t, HudState *hud, const DashArt *dash, const Car *car, const RaceState *race,
                    int16_t german, int32_t x, int32_t y, int32_t step);

/* 0041efb4: the gear in a yellow shape at x, y, drawn only when it differs
 * from hud->gear_shown: the car's gear (+0x3ed) while race->viewed <
 * race->humans, else how many of the PDN's speeds (+0x308, up to 6, in
 * turn) the computer speed (+0x15c) is past. 0.. 'yel1'.. (dash->yellow[1
 * + gear]), -1 'yeln', below 'yelr'. */
void hud_draw_gear(const QuadTarget *t, HudState *hud, const DashArt *dash, const Car *car, const RaceState *race,
                   int32_t x, int32_t y);

#endif
