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

enum {
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
