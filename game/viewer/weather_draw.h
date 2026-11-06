/* port: the weather's drawing on a 3D view (re/notes/weather-design.md): the sky moved toward its colour and the rain's streaks. */
#ifndef TNFS_WEATHER_DRAW_H
#define TNFS_WEATHER_DRAW_H

#include <stdint.h>

/* Every pixel of the rows [top, bottom) of the box [left, right) through `table` (256 bytes). */
/* port (rain v4): `table` taken to a share (0..16 of 16, an ordered dither) of the box's pixels: a grading that keeps the picture's detail where a whole-picture table would flatten it to a few palette entries. */
void weather_mood_rows(uint8_t *pixels, int32_t pitch, const int32_t box[4], const uint8_t table[256], int share);
void weather_sky_rows(uint8_t *pixels, int32_t pitch, int32_t left, int32_t right, int32_t top, int32_t bottom,
                      const uint8_t table[256]);

/* The same through the table of each pixel's level (weather_sky_level), `tables` the fog's levels: the sky meets the fog at the horizon. */
void weather_sky_fade(uint8_t *pixels, int32_t pitch, int32_t left, int32_t right, int32_t top, int32_t bottom,
                      const uint8_t tables[][256], int low, int high, int32_t cx, int32_t cy, float focal, const float up[3]);

/* The rain's streaks at `tick` over the box [left, top, right, bottom) of a buffer: each pixel a streak covers goes through
 * `table`; their width is `scale` pixels. The same call draws the same picture. */
void weather_rain_draw(uint8_t *pixels, int32_t pitch, const int32_t box[4], int scale, uint32_t tick, const uint8_t table[256], int storm, int v2);

/* port (rain_fx): the streaks of weather_rain_spoke: speed (the followed car's, 16.16 m/s), the vanishing point (vx, vy) in box pixels, the lens's focal in pixels, `night` for the
 * cone's light; `open` (0..16) how much of the rain falls here (0 in a tunnel, the drops thinned out by it at its mouths); `tables` the four brightness levels' (weather_rain_level_fog); `storm` WEATHER_STORM_DROPS times the streaks. */
void weather_rain_draw_fx(uint8_t *pixels, int32_t pitch, const int32_t box[4], int scale, uint32_t tick, int32_t speed, int32_t vx, int32_t vy, float focal, int night,
                          int open, const uint8_t tables[][256], int storm, int v2);

/* port (rain_fx, rain v3): the drops on the lens (weather_lens_drop): each a small lens, its body the scene behind it turned about the drop's centre (a drop shows the world upside down)
 * through `body` (a darkening table) and its rim lit, the upper left brighter, through `tables` (the brightness levels, `edge` the bright one); dithered away as it fades. */
void weather_lens_draw(uint8_t *pixels, int32_t pitch, const int32_t box[4], uint32_t tick, int32_t speed, int32_t vx, int32_t vy, int edge, const uint8_t tables[][256],
                       const uint8_t body[256], int open);

#endif
