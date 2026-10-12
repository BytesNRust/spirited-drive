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
    HUD_GAP_UNSET = -99,
    HUD_LIGHTS = 5,
    HUD_BEEPS = 6           /* 00513e78..00513e82: green, then lights 3, 1, 0, 4, 2 */
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
    int32_t distance_tens_shown;        /* 004f9a6c: HUD_GAP_UNSET until first drawn (and after the gap) */
    int32_t distance_ones_shown;        /* 004f9a70 */
    int32_t unknown_4f9a74;
    int32_t lap_shown;                  /* 004f9a78: HUD_GAP_UNSET until first drawn (and after the gap) */
    int32_t gear_shown;                 /* 004f9a7c */
    int32_t unknown_4f9a80[4];          /* 004f9a80..004f9a8c */
    int32_t redraw;                     /* 004f9a90 */
    int32_t lights_until;               /* 004f9a94: the tick the start lights were last drawn + 10 */
    int32_t unknown_4f9a98;
    int32_t strip_dirty;                /* 004f9a9c: the top strip is cleared before the lights */
    int32_t split_shown[HUD_SPLITS];    /* 004f9aa0 */
    int32_t split[HUD_SPLITS];          /* 004f9ab8: 0..2 times in ticks, 3..5 counts */
} HudState;

/* 0041e48c: ticks (60 a second) past HUD_START_TICKS into minutes, seconds
 * and tenths; 0 before. */
void hud_clock_split(HudClock *clock, int32_t ticks);

/* 0041e230: every *_shown word to HUD_GAP_UNSET (so all is drawn again),
 * 004f9a2c and the split_shown words 0, lights_until 0x7d000. */
void hud_reset(HudState *hud);

/* 0041e794: ticks (as hud_clock_split, into hud->clock) as mm:ss.t in the
 * grey digits (dash->grey_shown, 005138a4): minutes at x, seconds 2 steps
 * + space on, tenths 2 steps + space further. */
void hud_draw_time(const QuadTarget *t, HudState *hud, uint8_t *const *grey, int32_t ticks, int32_t x, int32_t y,
                   int32_t step, int32_t space);

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

/* 0041f044 (a race without laps): the distance left, (loop_nodes - node)
 * x 0x30 / 1600 (miles) or / 1000 (km, German) + 1 tenths, 0 at or past
 * the end, as tens at x and ones at x + 2 step + space ... (see hud.c);
 * the 'mile' shape first while the tens were never shown. */
void hud_draw_distance(const QuadTarget *t, HudState *hud, const DashArt *dash, const Car *car, const RaceState *race,
                       int16_t german, int32_t x, int32_t y, int32_t step, int32_t space);

/* 0041f1cc (a circuit): the car's lap (at most race->laps_setting) in one
 * digit after a blank or two at x, and race->laps at laps_x, laps_y; the
 * 'lap#' shape first while the lap was never shown. */
void hud_draw_lap(const QuadTarget *t, HudState *hud, const DashArt *dash, const Car *car, const RaceState *race,
                  int32_t x, int32_t y, int32_t step, int32_t laps_x, int32_t laps_y);

/* 0041ec88: the start lights, before the clock runs (00421e40 below tick
 * 0x168). The top strip first when hud->strip_dirty (flat fill of 320 x 16,
 * or 640 x 32 at high resolution, in `strip_colour`, 0050b7b4). Before tick
 * 300: three red then two yellow lights 0x41 (0x82) apart at y 0 (1), lit
 * up to tick / 60, else dark; from 300 five green. A light's beep word is
 * cleared when it is lit (the original's sound call 0044372c is empty).
 * hud->lights_until = tick + 10. */
void hud_draw_start_lights(const QuadTarget *t, HudState *hud, const DashArt *dash, int16_t beeps[HUD_BEEPS],
                           int32_t tick, int32_t high_res, uint32_t strip_colour);

/* 00420370: with hud->redraw set and past tick 0x167, the top row's fixed
 * shapes at their places and the record to beat: 'stat'; layout 1 'rtet',
 * the circuit record (race->records + 0xea + track x 0xa6b, + record_slot
 * x 0x186 on a circuit) as a grey time, 'splt' and the sign ('mnus' for a
 * gap below 0); layout 0 'segt', 'mile' / 'lap#' when no gap is showing
 * (gap_ticks < 1) else 'splt' and the sign, the stage record (records +
 * track x 0xa6b + track_variant x 0x27) or race->best_lap on a circuit;
 * layout 2 'acct' and the splits; below layout 2 'head' (race types 0, 1)
 * or 'sing'; then hud_reset and strip_dirty. Otherwise the top strip is
 * filled with strip_colour. */
void hud_draw_labels(const QuadTarget *t, HudState *hud, const DashArt *dash, const RaceState *race, int32_t gap,
                     int32_t gap_ticks, int32_t high_res, uint32_t strip_colour);

/* 0041e6bc: from race tick 0x186, the race time `ticks` (as
 * hud_clock_split) in yellow: minutes (2 digits) at x, seconds (2) 2 steps
 * + space on, tenths (1) 2 steps + space further, each only when it differs
 * from what is shown; then, when a best lap was set (race->best_lap_set,
 * cleared here), the labels again (hud->redraw = 1, hud_draw_labels). */
void hud_draw_race_time(const QuadTarget *t, HudState *hud, const DashArt *dash, RaceState *race, int32_t gap,
                        int32_t gap_ticks, int32_t high_res, uint32_t strip_colour, int32_t ticks, int32_t x,
                        int32_t y, int32_t step, int32_t space);

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
