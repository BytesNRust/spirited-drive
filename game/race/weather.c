#include "game/race/weather.h"

#include <stdlib.h>
#include <string.h>

enum {
    DROPS_640x480 = 240,       /* the count in a 640 x 480 view; more pixels, more drops */
    DROPS_MAX = 2000,
    FALL_MIN = 5200,           /* a streak's fall a tick, of 65536 box heights: 7 to 12 ticks across */
    FALL_RANGE = 3600,
    LENGTH_MIN = 28,           /* a streak's length in 480ths of the box height */
    LENGTH_RANGE = 24,
    SLANT_DIVISOR = 5          /* the tail's push to the right: length / 5 */
};

int weather_named(const char *name)
{
    char lower[8];
    size_t i = 0;
    for (; name[i] != '\0' && i < sizeof lower - 1; i++) lower[i] = (char)(name[i] >= 'A' && name[i] <= 'Z' ? name[i] + 32 : name[i]);
    lower[i] = '\0';
    if (name[i] != '\0') return -1;
    for (int w = 0; w < WEATHER_COUNT; w++)
        if (strcmp(lower, weather_name(w)) == 0) return w;
    return -1;
}

const char *weather_name(int weather)
{
    static const char *const NAMES[WEATHER_COUNT] = { "clear", "fog", "rain" };
    return weather >= 0 && weather < WEATHER_COUNT ? NAMES[weather] : NAMES[0];
}

static void mix(const uint8_t a[3], const uint8_t b[3], int percent, uint8_t out[3])
{
    for (int k = 0; k < 3; k++) out[k] = (uint8_t)((a[k] * (100 - percent) + b[k] * percent + 50) / 100);
}

void weather_fog_colour(int weather, const uint8_t base[3], uint8_t out[3])
{
    static const uint8_t FOG_GREY[3] = { 196, 198, 204 }, RAIN_GREY[3] = { 104, 110, 120 };
    mix(base, weather == WEATHER_RAIN ? RAIN_GREY : FOG_GREY, weather == WEATHER_RAIN ? 60 : 55, out);
}

void weather_sky_colour(int weather, const uint8_t fog[3], uint8_t out[3])
{
    static const uint8_t RAIN_SKY[3] = { 62, 68, 78 };
    if (weather == WEATHER_RAIN) memcpy(out, RAIN_SKY, 3);
    else memcpy(out, fog, 3);
}

int weather_fog_curve(void)
{
    const char *v = getenv("TNFS_FOG_CURVE");
    return v != NULL && strcmp(v, "exp") == 0;
}

int weather_sky_level(int32_t x, int32_t y, int32_t cx, int32_t cy, float focal, const float up[3], int low, int high)
{
    static const float BAYER[4][4] = { { 0.0f / 16, 8.0f / 16, 2.0f / 16, 10.0f / 16 }, { 12.0f / 16, 4.0f / 16, 14.0f / 16, 6.0f / 16 },
                                       { 3.0f / 16, 11.0f / 16, 1.0f / 16, 9.0f / 16 }, { 15.0f / 16, 7.0f / 16, 13.0f / 16, 5.0f / 16 } };
    if (high <= low) return low;
    const float rise = ((float)(x - cx) * up[0] + (float)(cy - y) * up[1] + focal * up[2]) / focal;   /* the ray's rise over its run */
    const float t = rise <= 0 ? 0.0f : rise >= WEATHER_SKY_FADE_TAN / 100.0f ? 1.0f : rise * 100.0f / WEATHER_SKY_FADE_TAN;
    const float level = (float)high - (float)(high - low) * t;
    const int l = (int)(level + BAYER[y & 3][x & 3]);
    return l < low ? low : l > high ? high : l;
}

void weather_rain_colour(uint8_t out[3])
{
    out[0] = 214, out[1] = 220, out[2] = 232;
}

/* a 32-bit hash (the finaliser of murmur3) of a drop and which of its numbers it is */
static uint32_t hash(uint32_t drop, uint32_t which)
{
    uint32_t h = drop * 0x9e3779b1u + which * 0x85ebca6bu + 0x27d4eb2fu;
    h ^= h >> 16, h *= 0x85ebca6bu, h ^= h >> 13, h *= 0xc2b2ae35u, h ^= h >> 16;
    return h;
}

int weather_rain_count(int32_t box_w, int32_t box_h)
{
    if (box_w <= 0 || box_h <= 0) return 0;
    const int64_t n = (int64_t)DROPS_640x480 * box_w * box_h / (640 * 480);
    return n > DROPS_MAX ? DROPS_MAX : n < 1 ? 1 : (int)n;
}

int weather_rain_width(int scale)
{
    return scale < 1 ? 1 : scale;
}

void weather_rain_streak(int drop, uint32_t tick, int32_t box_w, int32_t box_h, int32_t out[4])
{
    const uint32_t d = (uint32_t)drop;
    const uint32_t fall = FALL_MIN + hash(d, 1) % FALL_RANGE;
    const uint32_t phase = hash(d, 2) & 0xffff;
    const int32_t column = (int32_t)((int64_t)(hash(d, 3) & 0xffff) * box_w >> 16);
    const int32_t length = box_h * (LENGTH_MIN + (int32_t)(hash(d, 4) % LENGTH_RANGE)) / 480;
    const uint32_t along = (phase + fall * tick) & 0xffff;   /* the head's way down the box, of 65536 */
    const int32_t head = (int32_t)((int64_t)along * box_h >> 16);
    out[2] = column - head / SLANT_DIVISOR;
    out[3] = head;
    out[0] = out[2] + length / SLANT_DIVISOR;
    out[1] = head - length;
}
