#include "game/race/weather_snow.h"

#include "game/race/tick_rate.h"
#include "game/race/weather.h"

#include <math.h>

enum {
    TICKS_A_SECOND = RACE_TICKS_A_SECOND,
    SWAY_PERIOD_MIN = 2 * RACE_TICKS_A_SECOND, SWAY_PERIOD_RANGE = 3 * RACE_TICKS_A_SECOND,   /* ticks of a flake's sway there and back: 2 to 5 s */
    SWAY_PERCENT_MIN = 2, SWAY_PERCENT_RANGE = 4,   /* the sway's reach each way, percent of the box's width */
    WIND_PERCENT = 12,                               /* the storm's wind: percent of the box's width a second */
    HEAVY_FALL_PERCENT = 125                         /* the storm's flakes fall this much faster */
};

static uint32_t hash(uint32_t flake, uint32_t which)
{
    uint32_t h = flake * 2654435761u ^ (which + 1u) * 2246822519u;
    h ^= h >> 15, h *= 2246822519u, h ^= h >> 13, h *= 3266489917u, h ^= h >> 16;
    return h;
}

int weather_snow_level(int weather, int track_snow, int enabled)
{
    if (!track_snow || !enabled) return SNOW_NONE;
    return weather == WEATHER_RAIN ? SNOW_LIGHT : weather == WEATHER_STORM ? SNOW_HEAVY : SNOW_NONE;
}

int snow_race_wanted(int in_replay, int replay_snow, int local) { return in_replay ? replay_snow != 0 : local != 0; }

static int snow_grip;
void snow_grip_set(int on) { snow_grip = on != 0; }
int snow_grip_get(void) { return snow_grip; }

int race_grip_nonstandard(int rain_grip_active, int snow_grip) { return rain_grip_active != 0 || snow_grip != 0; }

int weather_snow_count(int32_t box_w, int32_t box_h, int level)
{
    if (box_w <= 0 || box_h <= 0 || (level != SNOW_LIGHT && level != SNOW_HEAVY)) return 0;
    const int64_t base = (int64_t)SNOW_PER_640x480 * box_w * box_h / (640 * 480);
    const int64_t n = level == SNOW_HEAVY ? base * SNOW_HEAVY_TIMES : base;
    return n > SNOW_MAX ? SNOW_MAX : n < 1 ? 1 : (int)n;
}

/* what both ways of placing a flake share: the column it starts in, how far down the box it is, and its size */
typedef struct { int32_t column, y, size; } FlakeBase;

static FlakeBase flake_base(uint32_t f, uint32_t tick, int level, int32_t box_w, int32_t box_h)
{
    uint32_t fall = SNOW_FALL_MIN + hash(f, 1) % (SNOW_FALL_MAX - SNOW_FALL_MIN + 1);   /* percent of the box's height a second */
    if (level == SNOW_HEAVY) fall = fall * HEAVY_FALL_PERCENT / 100;
    const uint32_t along = ((hash(f, 2) & 0xffff) + (uint32_t)((uint64_t)fall * 65536u * tick / (100u * TICKS_A_SECOND))) & 0xffff;   /* of 65536 down the box */
    const uint32_t s = hash(f, 7) % 100u;
    FlakeBase b;
    b.y = (int32_t)((int64_t)along * box_h >> 16);
    b.column = (int32_t)((int64_t)(hash(f, 3) & 0xffff) * box_w >> 16);
    b.size = s < 55 ? 1 : s < 90 ? 2 : SNOW_SIZE_MAX;
    return b;
}

void weather_snow_flake(int flake, uint32_t tick, int level, int32_t box_w, int32_t box_h, int32_t out[3])
{
    const uint32_t f = (uint32_t)flake;
    const int heavy = level == SNOW_HEAVY;
    if (box_w <= 0 || box_h <= 0) {
        out[0] = out[1] = out[2] = 0;
        return;
    }
    const FlakeBase base = flake_base(f, tick, level, box_w, box_h);
    const int32_t column = base.column;
    const int32_t period = SWAY_PERIOD_MIN + (int32_t)(hash(f, 4) % SWAY_PERIOD_RANGE), half = period / 2;
    const int32_t reach = box_w * (SWAY_PERCENT_MIN + (int32_t)(hash(f, 5) % SWAY_PERCENT_RANGE)) / 100;
    const int32_t t = (int32_t)((tick + hash(f, 6)) % (uint32_t)period), tri = t < half ? t : period - t;   /* 0 .. half, there and back */
    int64_t x = column + (int64_t)(tri * 2 - half) * reach / half;
    if (heavy) x += (int64_t)box_w * WIND_PERCENT * (int64_t)(tick % 1000000u) / (100 * TICKS_A_SECOND);
    x %= box_w;
    out[0] = (int32_t)(x < 0 ? x + box_w : x);
    out[1] = base.y;
    out[2] = base.size;
}

enum { SPEED_FULL_MS = 60 };   /* the lean reaches its top at this speed */

static float turn(uint32_t ticks, uint32_t period) { return (float)(ticks % period) * 6.2831853f / (float)period; }

int weather_snow_wind(uint32_t tick)
{
    return (int)lrintf(sinf(turn(tick, SNOW_DRIFT_PERIOD_TICKS)) * (float)SNOW_WIND_MAX);
}

void weather_snow_flake_fx(int flake, uint32_t tick, int level, int32_t box_w, int32_t box_h, int32_t speed, int32_t vx, int32_t vy, int32_t out[3])
{
    const uint32_t f = (uint32_t)flake;
    if (box_w <= 0 || box_h <= 0) {
        out[0] = out[1] = out[2] = 0;
        return;
    }
    const FlakeBase base = flake_base(f, tick, level, box_w, box_h);
    out[2] = base.size;
    /* no flake sways from side to side: the column it starts in (weather_snow_flake's own, without its there-and-back sway), and in the storm the slow swinging wind */
    float x = (float)base.column, y = (float)base.y;
    if (level == SNOW_HEAVY) {
        const double omega = 6.2831853 * TICKS_A_SECOND / SNOW_DRIFT_PERIOD_TICKS;   /* rad a second */
        x += (float)((double)box_w * (SNOW_WIND_MAX / 10.0) / 100.0 / omega * (1.0 - cosf(turn(tick, SNOW_DRIFT_PERIOD_TICKS))));
    }
    y += sinf(turn(tick + hash(f, 11) % 300u, 41u + hash(f, 12) % 37u)) * (float)box_h * 0.015f;   /* its fall speeds up and slows */
    float wrapped = fmodf(x, (float)box_w);
    if (wrapped < 0.0f) wrapped += (float)box_w;
    y = fmodf(y, (float)box_h);
    if (y < 0.0f) y += (float)box_h;
    const int64_t kmh = speed < 0 ? -(int64_t)speed : speed;
    float lean = (float)kmh / 65536.0f / (float)SPEED_FULL_MS;
    lean = (lean > 1.0f ? 1.0f : lean) * (float)SNOW_LEAN_PERCENT / 100.0f;
    wrapped += lean * (wrapped - (float)vx) * (y - (float)vy) / (float)box_h;   /* outward from the vanishing point, more the lower and the farther out it is */
    if (wrapped < 0.0f || wrapped >= (float)box_w) {
        out[0] = out[1] = out[2] = 0;
        return;
    }
    out[0] = (int32_t)wrapped, out[1] = (int32_t)y;
}

int weather_snow_lens_count(int32_t box_w, int32_t box_h)
{
    const int n = weather_lens_count(box_w, box_h) / SNOW_LENS_EVERY;
    return n < 1 ? 1 : n;
}

int weather_snow_sprite(int size, int side, uint8_t *out)
{
    if (size < 1) size = 1;
    if (size > SNOW_SIZE_MAX) size = SNOW_SIZE_MAX;
    if (side < 1) side = 1;
    if (side > SNOW_SIDE_MAX) side = SNOW_SIDE_MAX;
    const float radius = (float)side * (0.55f + 0.95f * (float)(size - 1));   /* far flakes tiny, near ones big */
    const float soft = fmaxf(1.0f, radius * (size == 1 ? 0.7f : 0.5f + 0.15f * (float)size));   /* and the near ones softer at the rim */
    const int top = size == 1 ? 2 : size == 2 ? 3 : SNOW_RAMP;   /* the far ones dimmer, too */
    const int half = (int)ceilf(radius) + 1, dim = 2 * half + 1;
    for (int y = 0; y < dim; y++)
        for (int x = 0; x < dim; x++) {
            const float dist = sqrtf((float)((x - half) * (x - half) + (y - half) * (y - half)));
            float c = (radius + 0.5f - dist) / soft;
            c = c < 0.0f ? 0.0f : c > 1.0f ? 1.0f : c;
            int v = (int)floorf(c * (float)top + 0.35f);
            out[y * dim + x] = (uint8_t)(v > top ? top : v);
        }
    return dim;
}
