#include "game/race/spray_v4.h"

#include <math.h>
#include <string.h>

enum { WATER_EVERY = 1, WATER_MIN_SPEED = 8, GRAVEL_MIN_SPEED = 6 };   /* water: each wheel every simulation step */
static const float TICKS_A_SECOND = 60.0f;
static const float GRAVITY = 9.8f / 3600.0f;   /* metres a tick a tick */
static const float WATER_PEAK = 0.42f, GRAVEL_PEAK = 0.85f;

static uint32_t hash2(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca6bu;
    h ^= h >> 15, h *= 0x2c1b3c6du, h ^= h >> 12;
    return h;
}

static float unit(uint32_t h, int shift) { return (float)(h >> shift & 0xff) / 255.0f; }   /* 0..1 */

void spray_v4_clear(SprayV4 *s)
{
    memset(s, 0, sizeof *s);
}

static SprayV4Particle *take(SprayV4 *s)
{
    SprayV4Particle *p = &s->p[s->next];
    s->next = (s->next + 1) % SPRAY_V4_MAX;
    memset(p, 0, sizeof *p);
    p->live = 1;
    return p;
}

static int water(SprayV4 *s, const SprayV4Emit *e)
{
    int n = 0;
    const float size = 1.1f + 0.9f * (e->speed > 45.0f ? 45.0f : e->speed) / 45.0f;   /* a faster car throws a bigger cloud */
    for (int side = 0; side < 2; side++) {
        if ((e->tick / 2u + (uint32_t)e->car + (uint32_t)side) % WATER_EVERY != 0) continue;   /* the tick goes up by two a step */
        const float dir = side == 0 ? -1.0f : 1.0f;
        const uint32_t h = hash2((uint32_t)e->car * 2u + (uint32_t)side, e->tick);
        SprayV4Particle *p = take(s);
        p->x = e->wheel[side][0] + (unit(h, 0) - 0.5f) * 0.4f, p->y = e->wheel[side][1], p->z = e->wheel[side][2] + (unit(h, 8) - 0.5f) * 0.4f;
        const float out = (0.9f + 0.5f * unit(h, 16)) / TICKS_A_SECOND;   /* opens to the sides: about a metre and a half by its end */
        p->vx = e->right[0] * dir * out - e->forward[0] * 0.005f, p->vz = e->right[1] * dir * out - e->forward[1] * 0.005f;
        p->vy = 0.004f;   /* it lifts a little */
        p->size = size * (0.85f + 0.3f * unit(h, 24));
        p->born = e->tick, p->node = e->node, p->kind = SPRAY_V4_WATER, p->car = (uint8_t)e->car, p->life = SPRAY_V4_WATER_LIFE;
        n++;
    }
    return n;
}

static int gravel(SprayV4 *s, const SprayV4Emit *e)
{
    int n = 0;
    for (int side = 0; side < 2; side++)
        for (int k = 0; k < 2; k++) {
            const float dir = side == 0 ? -1.0f : 1.0f;
            const uint32_t h = hash2((uint32_t)e->car * 4u + (uint32_t)side * 2u + (uint32_t)k, e->tick * 7u + 3u);
            SprayV4Particle *p = take(s);
            p->x = e->wheel[side][0], p->y = e->wheel[side][1], p->z = e->wheel[side][2];
            const float side_v = dir * (1.0f + 2.5f * unit(h, 0)) / TICKS_A_SECOND, back = (0.6f + 1.4f * unit(h, 8)) / TICKS_A_SECOND, up = (0.9f + 1.3f * unit(h, 16)) / TICKS_A_SECOND;   /* it is down again (2 up / g) before its life ends */
            p->vx = e->right[0] * side_v - e->forward[0] * back, p->vz = e->right[1] * side_v - e->forward[1] * back, p->vy = up;
            p->size = 0.12f + 0.12f * unit(h, 24);
            p->born = e->tick, p->node = e->node, p->kind = SPRAY_V4_GRAVEL, p->car = (uint8_t)e->car;
            p->life = (uint8_t)(SPRAY_V4_GRAVEL_MIN_LIFE + (int)(h >> 4 & 0xff) % SPRAY_V4_GRAVEL_SPREAD);
            n++;
        }
    return n;
}

int spray_v4_emit(SprayV4 *s, const SprayV4Emit *e)
{
    if (e->car < 0 || e->car >= SPRAY_V4_CARS) return 0;
    if (s->has_tick[e->car] && e->tick < s->last_tick[e->car]) spray_v4_clear(s);
    else if (s->has_tick[e->car] && e->tick == s->last_tick[e->car]) return 0;
    s->last_tick[e->car] = e->tick, s->has_tick[e->car] = 1;
    if (!e->on_ground) return 0;
    if (e->gravel) return e->speed >= GRAVEL_MIN_SPEED ? gravel(s, e) : 0;   /* the gravel's throw is its own: dry or wet */
    return e->wet && e->speed >= WATER_MIN_SPEED ? water(s, e) : 0;
}

int spray_v4_look(const SprayV4Particle *p, uint32_t tick, int open, SprayV4Look *out)
{
    if (!p->live || tick < p->born || tick - p->born >= p->life || (open <= 0 && p->kind == SPRAY_V4_WATER)) return 0;   /* a closed zone is dry: no water; gravel is thrown anywhere */
    const float a = (float)(tick - p->born), f = a / (float)p->life;
    const float o = open > 16 ? 1.0f : (float)open / 16.0f;
    out->x = p->x + p->vx * a, out->z = p->z + p->vz * a;
    if (p->kind == SPRAY_V4_WATER) {
        out->y = p->y + p->vy * a;
        out->radius = p->size * (0.6f + 1.6f * f) * (0.5f + 0.5f * o);
        out->alpha = WATER_PEAK * powf(1.0f - f, 1.5f) * (a + 1.0f >= 4.0f ? 1.0f : (a + 1.0f) / 4.0f) * (0.4f + 0.6f * o);
    } else {
        float y = p->y + p->vy * a - 0.5f * GRAVITY * a * a;
        out->y = y < p->y ? p->y : y;   /* it lands on the road it was thrown from */
        out->radius = p->size;
        out->alpha = GRAVEL_PEAK * powf(1.0f - f, 0.6f);
    }
    return 1;
}
