#include "game/race/rain_spray.h"
#include "game/race/trail_rise.h"

#include "game/sys/fixed_math.h"

#include <math.h>
#include <string.h>

static uint32_t hash2(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca6bu;
    h ^= h >> 15, h *= 0x2c1b3c6du, h ^= h >> 12;
    return h;
}

void rain_spray_clear(RainSpray *s)
{
    memset(s, 0, sizeof *s);
}

static void drop(RainSpray *s, int car, int side, uint32_t tick, const int32_t wheel[3], const int32_t right[2], int32_t node, int32_t metres, int life)
{
    const uint32_t h = hash2((uint32_t)car * 2u + (side > 0 ? 1u : 0u), tick);
    RainSprayParticle *p = &s->p[s->next];
    s->next = (s->next + 1) % RAIN_SPRAY_MAX;
    const int32_t jx = (int32_t)(h & 0xff) - 128, jz = (int32_t)(h >> 8 & 0xff) - 128;   /* a hand's width either way: the plume is not a line */
    p->x = wheel[0] + jx * 0x80, p->z = wheel[2] + jz * 0x80, p->y = wheel[1];
    const int32_t out = side * 0x7000 + (int32_t)(h >> 16 & 0xff) * 0x40 - 0x2000;   /* about half a metre outward by its end, a little at random */
    p->drift_x = (int32_t)(((int64_t)right[0] * out) >> 16), p->drift_z = (int32_t)(((int64_t)right[1] * out) >> 16);
    p->node = node, p->born = tick;
    p->size = (uint8_t)(metres >= 40 ? 255 : metres <= 8 ? 120 : 120 + (metres - 8) * 135 / 32);
    p->car = (uint8_t)car;
    p->live = 1;
    p->life = (uint8_t)life;
}

static int step(RainSpray *s, int car, uint32_t tick, const int32_t left[3], const int32_t right_wheel[3], const int32_t right[2], int32_t node, int32_t speed, int wet, int wisp)
{
    if (car < 0 || car >= RAIN_SPRAY_CARS) return 0;
    if (s->has_tick[car] && tick < s->last_tick[car]) {   /* the clock went back (a restart, a replay): the old trail is of another time */
        for (int i = 0; i < RAIN_SPRAY_MAX; i++) s->p[i].live = 0;
    } else if (s->has_tick[car] && tick == s->last_tick[car]) return 0;   /* the same tick drawn again: nothing new */
    s->last_tick[car] = tick, s->has_tick[car] = 1;
    const int64_t v = speed < 0 ? -(int64_t)speed : speed;
    const int32_t metres = (int32_t)(v >> 16);
    if (!wet || metres < (wisp == 2 ? RAIN_SPRAY_WISP2_MIN_SPEED : 8)) return 0;
    if (wisp == 2) {   /* rain_v2: a wisp from each wheel, a little longer, at any pace */
        drop(s, car, -1, tick, left, right, node, metres, RAIN_SPRAY_WISP2_LIFE);
        drop(s, car, 1, tick, right_wheel, right, node, metres, RAIN_SPRAY_WISP2_LIFE);
        return 2;
    }
    if (wisp) {
        const int32_t mid[3] = { left[0] / 2 + right_wheel[0] / 2, left[1] / 2 + right_wheel[1] / 2, left[2] / 2 + right_wheel[2] / 2 };
        drop(s, car, 0, tick, mid, right, node, metres, RAIN_SPRAY_WISP_LIFE);
        return 1;
    }
    drop(s, car, -1, tick, left, right, node, metres, RAIN_SPRAY_LIFE);
    drop(s, car, 1, tick, right_wheel, right, node, metres, RAIN_SPRAY_LIFE);
    return 2;
}

int rain_spray_step(RainSpray *s, int car, uint32_t tick, const int32_t left[3], const int32_t right_wheel[3], const int32_t right[2], int32_t node, int32_t speed, int wet)
{
    return step(s, car, tick, left, right_wheel, right, node, speed, wet, 0);
}

int rain_spray_step_wisp(RainSpray *s, int car, uint32_t tick, const int32_t left[3], const int32_t right_wheel[3], const int32_t right[2], int32_t node, int32_t speed, int wet)
{
    return step(s, car, tick, left, right_wheel, right, node, speed, wet, 1);
}

int rain_spray_step_wisp2(RainSpray *s, int car, uint32_t tick, const int32_t left[3], const int32_t right_wheel[3], const int32_t right[2], int32_t node, int32_t speed, int wet)
{
    return step(s, car, tick, left, right_wheel, right, node, speed, wet, 2);
}

int rain_spray_look(const RainSprayParticle *p, uint32_t tick, int open, int rise, int32_t place[3], int32_t *radius, int *density)
{
    if (!p->live || tick < p->born || tick - p->born >= p->life) return 0;
    if (open <= 0) return 0;   /* port (tunnel_dry): a closed zone is dry: no water to lift, what was dropped before the mouth is gone */
    const int age = (int)(tick - p->born);
    const float f = (float)age / (float)p->life, grow = 0.45f + 1.3f * f;   /* metres */
    const float big = 0.55f + 0.45f * (float)p->size / 255.0f;
    const float o = open < 0 ? 0.0f : open > 16 ? 1.0f : (float)open / 16.0f;
    place[0] = p->x + (int32_t)((float)p->drift_x * f), place[2] = p->z + (int32_t)((float)p->drift_z * f);
    place[1] = p->y + (int32_t)(65536.0f * 0.30f * o * f);   /* it hangs low: it lifts only a little, and not at all in a tunnel */
    if (rise) {   /* port (trail_rise): it drifts up to a car's height, less in a tunnel's mouth; a wisp (a short life) climbs by its share of the plume's life, or the traffic's trail is two chimneys */
        const float share = p->life >= RAIN_SPRAY_LIFE ? 1.0f : (float)p->life / (float)RAIN_SPRAY_LIFE;
        place[1] = p->y + (int32_t)((float)trail_rise_height(age, p->life) * o * share);
    }
    *radius = (int32_t)(65536.0f * grow * big * (0.5f + 0.5f * o));
    float d = (1.0f - f) * (1.0f - f) * (0.35f + 0.65f * (float)p->size / 255.0f) * (0.4f + 0.6f * o);
    if (rise) d *= (float)trail_rise_fade(age, p->life) / 100.0f;   /* and thins as it rises */
    int level = (int)(d * (float)RAIN_SPRAY_LEVELS);
    if (level >= RAIN_SPRAY_LEVELS) level = RAIN_SPRAY_LEVELS - 1;
    if (level < 0) level = 0;
    *density = level;
    return 1;
}

int32_t rain_spray_speed(int32_t speed, int32_t vx, int32_t vz)
{
    if (speed != 0) return speed;
    const int64_t ax = vx < 0 ? -(int64_t)vx : vx, az = vz < 0 ? -(int64_t)vz : vz;
    const int64_t big = ax > az ? ax : az, small = ax > az ? az : ax;
    const int64_t v = big + small / 4;
    return v > INT32_MAX ? INT32_MAX : (int32_t)v;
}

void rain_spray_heading(int use_matrix, int32_t yaw, int32_t matrix6, int32_t matrix8, int32_t forward[2])
{
    const double len = sqrt((double)matrix6 * matrix6 + (double)matrix8 * matrix8);
    if (use_matrix && len >= 1.0) {
        forward[0] = (int32_t)((double)matrix6 / len * 65536.0), forward[1] = (int32_t)((double)matrix8 / len * 65536.0);
        return;
    }
    const int32_t angle = (yaw >> 8) & 0xffff;
    forward[0] = fx_sin16(angle), forward[1] = fx_cos16(angle);
}

void rain_spray_wheels(const int32_t at[3], const int32_t forward[2], int32_t wheel[2][3], int32_t right[2])
{
    const int32_t behind = 0x16000, across = 0xd000;   /* a rear wheel: 1.4 m behind the centre, 0.8 m out */
    right[0] = forward[1], right[1] = -forward[0];
    for (int w = 0; w < 2; w++) {
        const int32_t a = w == 0 ? -across : across;
        wheel[w][0] = at[0] - (int32_t)(((int64_t)forward[0] * behind) >> 16) + (int32_t)(((int64_t)forward[1] * a) >> 16);
        wheel[w][1] = at[1] - 0x5000;
        wheel[w][2] = at[2] - (int32_t)(((int64_t)forward[1] * behind) >> 16) - (int32_t)(((int64_t)forward[0] * a) >> 16);
    }
}
