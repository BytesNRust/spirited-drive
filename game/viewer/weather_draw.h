/* port: the weather's drawing on a 3D view (re/notes/weather-design.md): the sky moved toward its colour and the rain's streaks. */
#ifndef TNFS_WEATHER_DRAW_H
#define TNFS_WEATHER_DRAW_H

#include <stdint.h>

/* Every pixel of the rows [top, bottom) of the box [left, right) through `table` (256 bytes). */
void weather_sky_rows(uint8_t *pixels, int32_t pitch, int32_t left, int32_t right, int32_t top, int32_t bottom,
                      const uint8_t table[256]);

/* The rain's streaks at `tick` over the box [left, top, right, bottom) of a buffer: each pixel a streak covers goes through
 * `table`; their width is `scale` pixels. The same call draws the same picture. */
void weather_rain_draw(uint8_t *pixels, int32_t pitch, const int32_t box[4], int scale, uint32_t tick, const uint8_t table[256]);

#endif
