#include "viewer/weather_draw.h"

#include "game/race/weather.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

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

void weather_rain_draw_fx(uint8_t *pixels, int32_t pitch, const int32_t box[4], int scale, uint32_t tick, int32_t speed, int32_t vx, int32_t vy, float focal, int night,
                          const uint8_t tables[][256])
{
    const int32_t w = box[2] - box[0], h = box[3] - box[1];
    const int width = weather_rain_width(scale);
    const int drops = weather_rain_count_fx(w, h);
    static const int BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    const float turned = weather_rain_blend(speed), fade_r = 0.12f * (float)h;   /* a spoke is faded in over the first 12 % of the box's height from the vanishing point */
    for (int i = 0; i < drops; i++) {
        int32_t s[4];
        int level;
        weather_rain_spoke(i, tick, speed, w, h, vx, vy, focal, night, s, &level);
        if (level < 0) continue;   /* a drop of the wedge straight down that has gone */
        const int32_t steps = abs(s[2] - s[0]) > abs(s[3] - s[1]) ? abs(s[2] - s[0]) : abs(s[3] - s[1]);
        if (steps <= 0) continue;
        for (int32_t k = 0; k <= steps; k++) {
            const int32_t x = s[0] + (int32_t)((int64_t)(s[2] - s[0]) * k / steps), y = s[1] + (int32_t)((int64_t)(s[3] - s[1]) * k / steps);
            if (y < 0 || y >= h) continue;
            if (turned > 0.0f) {   /* no empty disc at the vanishing point: the streaks thin out toward it, by a dither, instead of beginning at a radius */
                const float d = hypotf((float)(x - vx), (float)(y - vy)) / fade_r;
                const float keep = 1.0f - turned * (d >= 1.0f ? 0.0f : 1.0f - d);
                if (keep * 16.0f <= (float)BAYER[y & 3][x & 3]) continue;
            }
            for (int j = 0; j < width; j++) {
                const int32_t px = x + j;
                if (px < 0 || px >= w) continue;
                uint8_t *p = pixels + (size_t)(box[1] + y) * (size_t)pitch + (size_t)(box[0] + px);
                *p = tables[level][*p];
            }
        }
    }
}

void weather_lens_draw(uint8_t *pixels, int32_t pitch, const int32_t box[4], uint32_t tick, int32_t speed, int32_t vx, int32_t vy, int edge, const uint8_t tables[][256],
                       const uint8_t body[256])
{
    static const int BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    const int32_t w = box[2] - box[0], h = box[3] - box[1];
    const int drops = weather_lens_count(w, h);
    enum { SIDE_MAX = 2 * 16 + 3 };
    uint8_t copy[SIDE_MAX][SIDE_MAX];
    for (int i = 0; i < drops; i++) {
        int32_t o[4];
        if (!weather_lens_drop(i, tick, speed, w, h, vx, vy, o)) continue;
        const int32_t cx = o[0], cy = o[1], r = o[2] > 16 ? 16 : o[2], keep = o[3];
        for (int32_t y = -r - 1; y <= r + 1; y++)   /* the scene as it was: the body samples it turned, and the rim is put over it */
            for (int32_t x = -r - 1; x <= r + 1; x++) {
                const int32_t px = cx + x, py = cy + y;
                copy[y + r + 1][x + r + 1] = px >= 0 && px < w && py >= 0 && py < h ? pixels[(size_t)(box[1] + py) * (size_t)pitch + (size_t)(box[0] + px)] : 0;
            }
        for (int32_t y = -r; y <= r; y++)   /* a filled, soft blob: the scene behind it turned and darker, its edge a dither that thins out, a small highlight on the upper left */
            for (int32_t x = -r; x <= r; x++) {
                const int32_t px = cx + x, py = cy + y;
                if (px < 0 || px >= w || py < 0 || py >= h) continue;
                const float dist = sqrtf((float)(x * x + y * y)), soft = fmaxf(1.5f, 0.45f * (float)r);   /* the edge's width */
                const float cover = fminf(1.0f, ((float)r + 0.5f - dist) / soft);
                if (cover <= 0.0f || cover * (float)keep <= (float)BAYER[py & 3][px & 3]) continue;
                uint8_t *p = pixels + (size_t)(box[1] + py) * (size_t)pitch + (size_t)(box[0] + px);
                const int32_t sx = -(x * 4) / 5, sy = -(y * 4) / 5;
                *p = body[copy[sy + r + 1][sx + r + 1]];
                if (r >= 3 && x == -r / 3 && y == -r / 3) *p = tables[edge][*p];   /* the light on it */
            }
    }
}
