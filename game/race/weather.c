#include "game/race/weather.h"
#include "game/race/headlight_pool.h"

#include <math.h>
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

void weather_fog_for_tint(const uint8_t fog[3], const float tint[3], uint8_t out[3])
{
    for (int k = 0; k < 3; k++) {
        const float t = tint[k] < 0.05f ? 0.05f : tint[k];
        const float v = (float)fog[k] * (1.0f + t) / (2.0f * t) + 0.5f;
        out[k] = (uint8_t)(v > 255.0f ? 255 : v);
    }
}

int weather_fog_curve(void)
{
    const char *v = getenv("TNFS_FOG_CURVE");
    return v == NULL || strcmp(v, "line") != 0;
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

/* ---- rain_fx: streaks by speed and light ---- */

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* the eye's place along the car's nose from its centre (the cockpit's and the bumper's; a chase camera sits behind, but the drops are taken near the lens either way) */
static const float EYE_FORWARD_M = 0.5f;

int weather_rain_cone_weight(float ahead, float lateral, float height)
{
    return headlight_cone_weight_local((double)ahead + (double)EYE_FORWARD_M, (double)lateral, (double)height);   /* the cone's own, so the rain is lit where the road is */
}

int weather_rain_level_fog(int level)
{
    static const int FOG[WEATHER_RAIN_LEVELS] = { 3, 5, 7, 10 };
    return FOG[level < 0 ? 0 : level >= WEATHER_RAIN_LEVELS ? WEATHER_RAIN_LEVELS - 1 : level];
}

void weather_rain_spoke(int drop, uint32_t tick, int32_t speed, int32_t box_w, int32_t box_h, int32_t vx, int32_t vy, float focal, int night, int32_t out[4], int *level)
{
    const uint32_t d = (uint32_t)drop;
    const uint32_t fall = FALL_MIN + hash(d, 1) % FALL_RANGE, phase = hash(d, 2) & 0xffff;
    const float column = (float)((int64_t)(hash(d, 3) & 0xffff) * box_w >> 16);
    const float base = (float)(box_h * (LENGTH_MIN + (int32_t)(hash(d, 4) % LENGTH_RANGE)) / 480);
    const float metres = (float)((speed < 0 ? -(int64_t)speed : speed) >> 16);
    const float s = clampf(metres / WEATHER_SPOKE_FULL, 0.0f, 1.0f);
    const float p = (float)((phase + fall * tick) & 0xffff) / 65536.0f;
    const float theta = (float)(hash(d, 5) & 0xffff) * (6.2831853f / 65536.0f);
    /* the direction: a falling streak's (today's slant) turned to the spoke's, from the drop's own angle about the vanishing point */
    float dx = (1.0f - s) * -0.196f + s * cosf(theta), dy = (1.0f - s) * 0.981f + s * sinf(theta) * 0.8f;
    const float n = sqrtf(dx * dx + dy * dy);
    dx /= n, dy /= n;
    const float ox = (1.0f - s) * column + s * (float)vx, oy = s * (float)vy;
    const float reach = (float)box_h * (1.0f + 0.6f * s);
    const float near_end = 0.12f * reach * s;   /* a spoke is born a little way from the vanishing point, not at it */
    const float head = near_end + (reach - near_end) * powf(p, 1.0f + s);
    const float len = base * (1.0f + 2.5f * s) * (0.3f + p * s + (1.0f - s) * 0.7f);
    const float tail = head - len > near_end ? head - len : near_end;
    out[0] = (int32_t)(ox + dx * tail), out[1] = (int32_t)(oy + dy * tail);
    out[2] = (int32_t)(ox + dx * head), out[3] = (int32_t)(oy + dy * head);
    int l = WEATHER_RAIN_DAY_LEVEL;
    if (night && focal > 0.0f) {
        const float z = 4.0f + 36.0f * (float)(hash(d, 6) & 0xffff) / 65536.0f * (float)(hash(d, 6) & 0xffff) / 65536.0f;   /* the depth the drop is taken at: most near */
        const float rx = ((float)out[2] - (float)vx) / focal, ry = ((float)vy - (float)out[3]) / focal;
        const int w = weather_rain_cone_weight(z, rx * z, 1.2f + ry * z);
        l = w < 24 ? 0 : w < 96 ? 1 : w < 176 ? 2 : 3;
    }
    *level = l;
}

int weather_rain_spray_puff(int k, int side, uint32_t tick, int32_t speed, int wet_road, int32_t out[3], int32_t *size)
{
    const int64_t v = speed < 0 ? -(int64_t)speed : speed;
    const float metres = (float)(v >> 16);
    *size = 0;
    if (!wet_road || metres < 8.0f || k < 0 || k >= WEATHER_SPRAY_PUFFS) return 0;
    const uint32_t h = hash((uint32_t)k * 2u + (side > 0 ? 1u : 0u), 7u + tick);
    const float jitter = (float)(h & 0xff) / 255.0f - 0.5f, jitter2 = (float)((h >> 8) & 0xff) / 255.0f - 0.5f;
    const float reach = clampf(metres / 30.0f, 0.4f, 1.6f);                  /* the plume's length grows with the speed */
    const float t = (float)k / (float)(WEATHER_SPRAY_PUFFS - 1);            /* 0 at the wheel .. 1 at the plume's end */
    const float behind = 0.3f + 2.4f * reach * t + jitter * 0.3f;   /* a chase camera sits only a few metres behind: a plume longer than that is behind the lens */
    const float sideways = (float)side * (0.3f + 1.6f * t) + jitter2 * 0.4f;
    const float up = 0.2f + 1.2f * t + jitter * 0.2f;
    out[0] = (int32_t)(behind * 65536.0f), out[1] = (int32_t)(sideways * 65536.0f), out[2] = (int32_t)(up * 65536.0f);
    const float grow = 0.5f + 1.1f * t, thin = 1.0f - t * t * 0.85f;           /* swells, then thins out */
    const float speedy = clampf(metres / 40.0f, 0.5f, 1.0f);
    *size = (int32_t)(256.0f * grow * thin * speedy);
    return *size > 0;
}

int weather_rain_spray_level(int night)
{
    return night ? 1 : WEATHER_RAIN_LEVELS - 1;
}

int weather_rain_flash(uint32_t tick)
{
    if (tick < 150) return 0;
    const uint32_t slot = tick / WEATHER_STORM_SLOT, at = tick - slot * WEATHER_STORM_SLOT;
    const uint32_t h = hash(slot, 11);
    if ((h & 3) == 0) return 0;                                     /* one slot in four is quiet */
    const uint32_t start = (h >> 4) % (WEATHER_STORM_SLOT - WEATHER_STORM_DOUBLE - 1);
    const int twice = (h >> 12) & 1;
    /* two ticks a flash: the race's tick goes up by two a drawn frame in the drive run, so a one-tick flash could fall between frames */
    return at - start < 2 || (twice && at - start - WEATHER_STORM_DOUBLE < 2);
}

void weather_flash_colour(uint8_t out[3])
{
    out[0] = 205, out[1] = 220, out[2] = 255;
}
