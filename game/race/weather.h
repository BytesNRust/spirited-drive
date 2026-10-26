/* port: the weather (re/notes/weather-design.md). Pure functions of the setting and the tick: no RNG of the game's, no clock,
 * so a replay or a screenshot at the same tick draws the same weather. */
#ifndef TNFS_WEATHER_H
#define TNFS_WEATHER_H

#include <stdint.h>

enum { WEATHER_CLEAR = 0, WEATHER_FOG = 1, WEATHER_RAIN = 2, WEATHER_COUNT = 3 };

/* The weather a name stands for ("clear", "fog", "rain", any case), or -1. */
int weather_named(const char *name);
const char *weather_name(int weather);   /* "clear" for a value out of range */

/* The colours the weather moves the view to, in the palette's scale. `base` is the track's horizon-foot fog colour. */
void weather_fog_colour(int weather, const uint8_t base[3], uint8_t out[3]);   /* the land's far end */
void weather_sky_colour(int weather, const uint8_t fog[3], uint8_t out[3]);    /* the sky's, at WEATHER_SKY_LEVEL */
void weather_rain_colour(uint8_t out[3]);                                      /* a streak's, at WEATHER_STREAK_LEVEL */

/* The fog table a sky pixel (x, y) goes through: the sky's own `low` high above the horizon, `high` at it and below, a
 * dither of the two between, so that what the fog has made whole (land and cars past the fog's end) meets the sky without
 * an edge. `up` is the world's up in camera space (x right, y up, z ahead), the view's centre cx, cy and focal in pixels;
 * the horizon is where a ray is level, whatever the pitch or roll. */
int weather_sky_level(int32_t x, int32_t y, int32_t cx, int32_t cy, float focal, const float up[3], int low, int high);

/* The fog's curve: 1 an exponential that takes the same level at 2500 (about 8 of 15) as the line and goes on toward
 * WEATHER_FOG_EXP_CAP of the way without reaching the whole, so the far cars keep a trace of shading (the owner's decision, the
 * default); 0 the line from WEATHER_FOG_START to WEATHER_FOG_END (the fog is whole from there on: a plateau, the cars past it flat),
 * for comparison: TNFS_FOG_CURVE=line. */
int weather_fog_curve(void);

/* With the PS1 light's time of day the 3D's pixels go through the tinted palette whole, which turns the fog into a saturated soup. The fog
 * is made at half the tint instead (as the dash is, the owner's decision): its colour is set so that after the tint it is the day's one with
 * half the tint. */
/* `fog` for a palette that will be multiplied by `tint` (per channel): fog x (1 + tint) / 2 / tint, at most 255 a channel (a tint far
 * under a half cannot be made up for). `out` may be `fog`. */
void weather_fog_for_tint(const uint8_t fog[3], const float tint[3], uint8_t out[3]);
enum { WEATHER_FOG_EXP_SCALE = 2380, WEATHER_FOG_EXP_CAP_PERCENT = 93 };

enum {
    WEATHER_SKY_FADE_TAN = 20,     /* a hundredth of the ray's rise (over its run) where the sky is whole its own: 0.20 */
    WEATHER_SKY_LEVEL = 11,        /* of MODERN_FOG_LEVELS - 1 = 15: how far the sky goes to its colour */
    WEATHER_RAIN_SKY_LEVEL = 8,
    WEATHER_STREAK_LEVEL = 7,
    WEATHER_FOG_END = 4200,        /* the camera depth where the fog is whole (the track's: about 780 a segment drawn) */
    WEATHER_FOG_START = 250,
    WEATHER_RAIN_FOG_END = 12000,  /* rain's haze, only where Modern 3D is on */
    WEATHER_RAIN_FOG_START = 1500
};

/* The streaks of a view box_w x box_h (pixels) at `tick`: how many, and streak i as the line tail (x0, y0) .. head (x1, y1)
 * relative to the box (the ends may lie outside it; the caller clips). The box is in the pixels of the buffer drawn, so at a render
 * scale the same drop is the same fraction of the box and its length grows with the box; only its width is `scale` pixels. */
int weather_rain_count(int32_t box_w, int32_t box_h);
void weather_rain_streak(int drop, uint32_t tick, int32_t box_w, int32_t box_h, int32_t out[4]);
int weather_rain_width(int scale);   /* pixels across */

#endif
