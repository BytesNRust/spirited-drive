#include "viewer/weather_draw.h"

#include "game/race/weather.h"

#include <stddef.h>

void weather_sky_rows(uint8_t *pixels, int32_t pitch, int32_t left, int32_t right, int32_t top, int32_t bottom,
                      const uint8_t table[256])
{
    for (int32_t y = top; y < bottom; y++) {
        uint8_t *row = pixels + (size_t)y * (size_t)pitch;
        for (int32_t x = left; x < right; x++) row[x] = table[row[x]];
    }
}

void weather_sky_fade(uint8_t *pixels, int32_t pitch, int32_t left, int32_t right, int32_t top, int32_t bottom,
                      const uint8_t tables[][256], int low, int high, int32_t cx, int32_t cy, float focal, const float up[3])
{
    for (int32_t y = top; y < bottom; y++) {
        uint8_t *row = pixels + (size_t)y * (size_t)pitch;
        for (int32_t x = left; x < right; x++) row[x] = tables[weather_sky_level(x, y, cx, cy, focal, up, low, high)][row[x]];
    }
}

void weather_rain_draw(uint8_t *pixels, int32_t pitch, const int32_t box[4], int scale, uint32_t tick, const uint8_t table[256])
{
    const int32_t w = box[2] - box[0], h = box[3] - box[1];
    const int width = weather_rain_width(scale);
    const int drops = weather_rain_count(w, h);
    for (int i = 0; i < drops; i++) {
        int32_t s[4];
        weather_rain_streak(i, tick, w, h, s);   /* (x0, y0) the tail, (x1, y1) the head */
        const int32_t dy = s[3] - s[1];
        if (dy <= 0) continue;
        for (int32_t y = s[1]; y <= s[3]; y++) {
            if (y < 0 || y >= h) continue;
            const int32_t x = s[0] + (int32_t)((int64_t)(s[2] - s[0]) * (y - s[1]) / dy);
            for (int k = 0; k < width; k++) {
                const int32_t px = x + k;
                if (px < 0 || px >= w) continue;
                uint8_t *p = pixels + (size_t)(box[1] + y) * (size_t)pitch + (size_t)(box[0] + px);
                *p = table[*p];
            }
        }
    }
}
