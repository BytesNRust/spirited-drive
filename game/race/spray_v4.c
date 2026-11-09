#include "game/race/spray_v4.h"

#include <math.h>
#include <string.h>

static const float MIN_WATER_SPEED = 8.0f, MIN_GRAVEL_SPEED = 6.0f, MIN_EARTH_SPEED = 3.0f;

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

typedef struct { float gravity, drag, peak, grow0, grow1; int lands; } Physics;   /* radius = size (grow0 + grow1 f) */
static const Physics PHYSICS[SPRAY_V4_KINDS] = {
    [SPRAY_V4_MIST]   = { 0.0f, 0.0f, 0.70f, 0.7f, 1.5f, 0 },
    [SPRAY_V4_GRAVEL] = { 9.8f, 0.0f, 0.90f, 1.0f, 0.0f, 1 },
    [SPRAY_V4_DUST]   = { 0.0f, 0.0f, 0.80f, 0.7f, 1.5f, 0 },
    [SPRAY_V4_DROP]   = { 9.8f, 0.8f, 0.95f, 1.0f, 0.0f, 1 },
    [SPRAY_V4_EARTH]  = { 0.0f, 0.0f, 0.95f, 0.8f, 3.2f, 0 },
    [SPRAY_V4_SNOW]   = { 0.9f, 0.0f, 0.75f, 0.8f, 2.4f, 0 },
    [SPRAY_V4_CLUMP]  = { 9.8f, 0.3f, 0.95f, 1.0f, 0.0f, 1 }
};

static SprayV4Particle *take(SprayV4 *s, const SprayV4Emit *e, SprayV4Kind kind, int life, const float at[3], float slope)
{
    SprayV4Particle *p = &s->p[s->next];
    s->next = (s->next + 1) % SPRAY_V4_MAX;
    memset(p, 0, sizeof *p);
    p->live = 1, p->kind = (uint8_t)kind, p->car = (uint8_t)e->car, p->life = (uint8_t)life, p->born = e->tick, p->node = e->node;
    p->x = at[0], p->y = at[1], p->z = at[2];
    p->gx = slope * e->forward[0], p->gz = slope * e->forward[1];   /* the ground rises along the way by slope a metre */
    return p;
}

/* the particle's velocity from the car's frame: `back` metres a second behind the car (relative to it), `side` outward (signed), `up` */
static void throw_at(SprayV4Particle *p, const SprayV4Emit *e, float back_rel, float side, float up)
{
    const float world_forward = e->speed - back_rel;
    p->vx = e->forward[0] * world_forward + e->right[0] * side, p->vz = e->forward[1] * world_forward + e->right[1] * side, p->vy = up;
}

static void wheel_point(const SprayV4Emit *e, int side, uint32_t h, float spread, float out[3])
{
    out[0] = e->wheel[side][0] + (unit(h, 0) - 0.5f) * spread, out[1] = e->wheel[side][1], out[2] = e->wheel[side][2] + (unit(h, 8) - 0.5f) * spread;
}

static int water(SprayV4 *s, const SprayV4Emit *e, float slope)
{
    int n = 0;
    const float frac = (e->speed > 45.0f ? 45.0f : e->speed) / 45.0f;
    const int drops = 2 + (int)(e->speed / 12.0f) > 5 ? 5 : 2 + (int)(e->speed / 12.0f);
    for (int side = 0; side < 2; side++) {
        const float dir = side == 0 ? -1.0f : 1.0f;
        uint32_t h = hash2((uint32_t)e->car * 2u + (uint32_t)side, e->tick);
        float at[3];
        wheel_point(e, side, h, 0.3f, at);
        SprayV4Particle *m = take(s, e, SPRAY_V4_MIST, 48, at, slope);   /* the mist: fine, low, opening to the sides */
        throw_at(m, e, 0.0f, dir * (0.8f + 0.7f * unit(h, 16)), 0.35f);
        m->size = 0.7f + 0.5f * frac * (0.8f + 0.4f * unit(h, 24));
        n++;
        for (int k = 0; k < drops; k++) {   /* the drops: a fan, back and up, with the car's speed */
            h = hash2((uint32_t)e->car * 32u + (uint32_t)side * 16u + (uint32_t)k, e->tick * 5u + 1u);
            wheel_point(e, side, h, 0.2f, at);
            SprayV4Particle *d = take(s, e, SPRAY_V4_DROP, 48, at, slope);
            throw_at(d, e, e->speed * (0.25f + 0.45f * unit(h, 0)), dir * (1.0f + 0.05f * e->speed) * (0.4f + 0.6f * unit(h, 8)), 1.6f + 1.6f * unit(h, 16) + 0.01f * e->speed);
            d->size = 0.05f + 0.04f * unit(h, 24);
            n++;
        }
    }
    return n;
}

static int gravel(SprayV4 *s, const SprayV4Emit *e, float slope)
{
    int n = 0;
    for (int side = 0; side < 2; side++) {
        const float dir = side == 0 ? -1.0f : 1.0f;
        for (int k = 0; k < 9; k++) {   /* stones: eight, small, and the dust */
            const uint32_t h = hash2((uint32_t)e->car * 32u + (uint32_t)side * 16u + (uint32_t)k, e->tick * 7u + 3u);
            float at[3];
            wheel_point(e, side, h, 0.0f, at);
            if (k == 8) {   /* the dust: light, low, drifts back and up, widens */
                SprayV4Particle *d = take(s, e, SPRAY_V4_DUST, 44, at, slope);
                throw_at(d, e, 0.5f, dir * 0.35f, 0.5f);
                d->size = 0.7f + 0.5f * unit(h, 24);
            } else {
                SprayV4Particle *g = take(s, e, SPRAY_V4_GRAVEL, 28 + (int)(h >> 4 & 0xff) % 12, at, slope);
                throw_at(g, e, 0.6f + 1.4f * unit(h, 8), dir * (1.0f + 2.5f * unit(h, 0)), 0.9f + 1.3f * unit(h, 16));
                g->size = 0.09f + 0.08f * unit(h, 24);
            }
            n++;
        }
    }
    return n;
}

static int earth(SprayV4 *s, const SprayV4Emit *e, float slope)
{
    int n = 0;
    const float frac = (e->speed > 40.0f ? 40.0f : e->speed) / 40.0f;
    for (int side = 0; side < 2; side++)
        for (int k = 0; k < 2; k++) {   /* two puffs a wheel a step: a thick trail */
            const uint32_t h = hash2((uint32_t)e->car * 4u + (uint32_t)side * 2u + (uint32_t)k, e->tick * 3u + 11u);
            float at[3];
            wheel_point(e, side, h, 0.5f, at);
            SprayV4Particle *d = take(s, e, SPRAY_V4_EARTH, 100 + (int)(h >> 4 & 0xff) % 24, at, slope);   /* 1.7 to 2 s */
            throw_at(d, e, 0.6f, (side == 0 ? -1.0f : 1.0f) * 0.4f * (0.3f + unit(h, 16)), 0.3f + 0.15f * unit(h, 24));
            d->size = 1.1f + 0.8f * frac * (0.7f + 0.6f * unit(h, 16));
            n++;
        }
    return n;
}

static int snow(SprayV4 *s, const SprayV4Emit *e, float slope)
{
    int n = 0;
    for (int side = 0; side < 2; side++) {
        const float dir = side == 0 ? -1.0f : 1.0f;
        uint32_t h = hash2((uint32_t)e->car * 2u + (uint32_t)side, e->tick * 3u + 17u);
        float at[3];
        wheel_point(e, side, h, 0.4f, at);
        SprayV4Particle *p = take(s, e, SPRAY_V4_SNOW, 90, at, slope);   /* the powder: opens, rises a little and settles */
        throw_at(p, e, 0.5f, dir * (0.4f + 0.5f * unit(h, 16)), 0.35f);
        p->size = 0.7f + 0.4f * unit(h, 24);
        n++;
        for (int k = 0; k < 2; k++) {   /* lumps in a short parabola */
            h = hash2((uint32_t)e->car * 32u + (uint32_t)side * 16u + (uint32_t)k, e->tick * 5u + 29u);
            wheel_point(e, side, h, 0.2f, at);
            SprayV4Particle *c = take(s, e, SPRAY_V4_CLUMP, 24, at, slope);
            throw_at(c, e, 0.4f + 0.5f * e->speed * unit(h, 0) / 2.0f, dir * (0.8f + 1.2f * unit(h, 8)), 1.0f + 1.5f * unit(h, 16));
            c->size = 0.06f + 0.06f * unit(h, 24);
            n++;
        }
    }
    return n;
}

int spray_v4_emit(SprayV4 *s, const SprayV4Emit *e)
{
    if (e->car < 0 || e->car >= SPRAY_V4_CARS) return 0;
    if (s->has_tick[e->car] && e->tick < s->last_tick[e->car]) spray_v4_clear(s);
    else if (s->has_tick[e->car] && e->tick == s->last_tick[e->car]) return 0;
    s->last_tick[e->car] = e->tick, s->has_tick[e->car] = 1;
    const float foot[3] = { (e->wheel[0][0] + e->wheel[1][0]) * 0.5f, (e->wheel[0][1] + e->wheel[1][1]) * 0.5f, (e->wheel[0][2] + e->wheel[1][2]) * 0.5f };
    float slope = 0.0f;   /* the ground's rise a metre along the way, from where the wheels were the tick before */
    if (s->has_foot[e->car]) {
        const float *was = s->last_foot[e->car];
        const float dx = foot[0] - was[0], dz = foot[2] - was[2], along = dx * e->forward[0] + dz * e->forward[1];
        if (fabsf(along) > 0.2f) slope = (foot[1] - was[1]) / along;
        if (slope > 0.5f) slope = 0.5f;
        if (slope < -0.5f) slope = -0.5f;
    }
    memcpy(s->last_foot[e->car], foot, sizeof foot), s->has_foot[e->car] = 1;
    if (!e->on_ground) return 0;
    switch (e->surface) {
    case SPRAY_V4_WATER:     return e->speed >= MIN_WATER_SPEED ? water(s, e, slope) : 0;
    case SPRAY_V4_ON_GRAVEL: return e->speed >= MIN_GRAVEL_SPEED ? gravel(s, e, slope) : 0;
    case SPRAY_V4_ON_EARTH:  return e->speed >= MIN_EARTH_SPEED ? earth(s, e, slope) : 0;
    case SPRAY_V4_ON_SNOW:   return e->speed >= MIN_EARTH_SPEED ? snow(s, e, slope) : 0;
    default:                 return 0;
    }
}

int spray_v4_look(const SprayV4Particle *p, uint32_t tick, int open, SprayV4Look *out)
{
    if (!p->live || tick < p->born || tick - p->born >= p->life || p->kind >= SPRAY_V4_KINDS) return 0;
    if (open <= 0 && (p->kind == SPRAY_V4_MIST || p->kind == SPRAY_V4_DROP)) return 0;   /* a closed zone is dry: no water; the rest is thrown anywhere */
    const Physics *ph = &PHYSICS[p->kind];
    const float t = (float)(tick - p->born) / (float)SPRAY_V4_TICKS_A_SECOND, f = (float)(tick - p->born) / (float)p->life;
    const float o = open > 16 ? 1.0f : (float)open / 16.0f;
    float run = t, fall = 0.5f * ph->gravity * t * t;   /* the way along the velocity, and the pull */
    if (ph->drag > 0.0f) {
        const float k = ph->drag, e = 1.0f - expf(-k * t);
        run = e / k, fall = ph->gravity * (t / k - e / (k * k));
    }
    out->x = p->x + p->vx * run, out->z = p->z + p->vz * run;
    out->y = p->y + p->vy * run - fall;
    const float ground = p->y + p->gx * (out->x - p->x) + p->gz * (out->z - p->z);
    if (ph->lands && out->y <= ground && t > 0.02f) return 0;   /* it touched the ground: gone */
    if (out->y < ground) out->y = ground;
    out->radius = p->size * (ph->grow0 + ph->grow1 * f);
    const float fade_in = t * 60.0f + 1.0f >= 4.0f ? 1.0f : (t * 60.0f + 1.0f) / 4.0f;
    switch (p->kind) {
    case SPRAY_V4_MIST:  out->alpha = ph->peak * powf(1.0f - f, 1.4f) * fade_in * (0.4f + 0.6f * o), out->radius *= 0.5f + 0.5f * o; break;
    case SPRAY_V4_DROP:  out->alpha = ph->peak * (f < 0.7f ? 1.0f : (1.0f - f) / 0.3f) * (0.5f + 0.5f * o); break;
    case SPRAY_V4_GRAVEL:
    case SPRAY_V4_CLUMP: out->alpha = ph->peak * powf(1.0f - f, 0.6f); break;
    default:             out->alpha = ph->peak * powf(1.0f - f, 1.2f) * fade_in; break;
    }
    return 1;
}
