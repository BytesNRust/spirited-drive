/* port: see dust_trail.h. */
#include "game/race/dust_trail.h"

#include "game/race/rally_gravel.h"
#include "game/race/tick_rate.h"
#include "game/sys/fixed_math.h"

enum {
    BEHIND = 0x16000,      /* the rear wheels sit 1.4 m behind the car's centre... */
    HALF_TRACK = 0xd000,   /* ...and 0.8 m either side of it */
    DROP = 0x5000,         /* 0.3 m under the centre: the tyre's own height */
    SPREAD = 0x5000,       /* a puff is dropped within this of the wheel */
    RISE = 0x1c0,          /* a tick's rise (16.16 metres): 0.5 m over a desert puff's life */
    DRIFT = 0x480,         /* a tick's drift outward */
    LIFE_DUST = 12 * RACE_TICKS_A_SECOND / 10,        /* 1.2 s */
    LIFE_REGOLITH = 18 * RACE_TICKS_A_SECOND / 10,    /* 1.8 s */
    LIFE_LUNAR = 3 * RACE_TICKS_A_SECOND,             /* the regolith under the moon's pull hangs 3 seconds */
    MAX_STEPS = 4 * RACE_TICKS_A_SECOND / 10          /* a frame may cover this much (0.4 s: a slow machine) and still lay its trail whole; a longer pause leaves a gap, not a burst */
};

void dust_trail_reset(DustTrail *trail)
{
    for (int i = 0; i < DUST_TRAIL_PUFFS; i++) trail->puff[i].born = -0x40000000;
    trail->next = 0;
    trail->last_tick = -1;
}

static uint32_t next_random(uint32_t seed) { return seed * 0x10004005u + 0x11u; }

static int32_t jitter(uint32_t r) { return (int32_t)((r >> 8) % (2 * SPREAD)) - SPREAD; }

static int32_t life_of(int kind, int lunar)
{
    if (kind == RALLY_GRAVEL_REGOLITH) return lunar ? LIFE_LUNAR : LIFE_REGOLITH;
    return LIFE_DUST;
}

int32_t dust_life(int kind, int lunar) { return life_of(kind, lunar); }

static int32_t mul16(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 16); }

static void drop_at(DustTrail *trail, int32_t tick, const int32_t car[3], int32_t yaw, int32_t back)
{
    const int32_t angle = (yaw >> 8) & 0xffff;
    const int32_t fx = fx_sin16(angle), fz = fx_cos16(angle);   /* forward (x, z); the right is (fz, -fx) */
    for (int side = -1; side <= 1; side += 2) {
        DustPuff *p = &trail->puff[trail->next];
        uint32_t r = next_random((uint32_t)(tick * 2 + side + 3) * 2654435761u);
        p->pos[0] = car[0] - mul16(fx, BEHIND + back) + mul16(fz, side * HALF_TRACK) + jitter(r);
        r = next_random(r);
        p->pos[1] = car[1] - DROP + jitter(r) / 4;
        r = next_random(r);
        p->pos[2] = car[2] - mul16(fz, BEHIND + back) - mul16(fx, side * HALF_TRACK) + jitter(r);
        p->born = tick;
        p->drift[0] = mul16(fz, side * DRIFT), p->drift[1] = -mul16(fx, side * DRIFT);
        p->seed = (int32_t)(next_random(r) >> 8);
        trail->next = trail->next + 1 == DUST_TRAIL_PUFFS ? 0 : trail->next + 1;
    }
}

void dust_trail_step(DustTrail *trail, int32_t tick, int kind, int drop, const int32_t car[3], int32_t yaw, int32_t speed)
{
    if (trail->last_tick < 0 || tick < trail->last_tick) {
        dust_trail_reset(trail);
        trail->last_tick = tick;
        return;
    }
    int32_t from = trail->last_tick + 1;
    if (tick - from + 1 > MAX_STEPS) from = tick - MAX_STEPS + 1;
    if (drop && (kind == RALLY_GRAVEL_DUST || kind == RALLY_GRAVEL_REGOLITH || kind == RALLY_GRAVEL_SNOW))
        for (int32_t t = from; t <= tick; t++) drop_at(trail, t, car, yaw, (int32_t)((int64_t)speed * (tick - t) / RACE_TICKS_A_SECOND));
    trail->last_tick = tick;
}

int dust_puff_at(const DustPuff *puff, int32_t tick, int kind, int lunar, int32_t place[3], int32_t *size)
{
    const int32_t age = tick - puff->born, life = life_of(kind, lunar);
    if (age < 0 || age >= life) return 0;
    const int32_t f = age * 256 / life;   /* 0..255 of its life */
    /* the dust rises a little; the regolith is thrown up and falls back (slowly under the moon's pull) */
    int32_t up = RISE * age;
    if (kind == RALLY_GRAVEL_REGOLITH) up = 3 * RISE * age - (lunar ? RISE / 16 : RISE / 4) * age * age / 4;
    if (up < 0) up = 0;
    place[0] = puff->pos[0];
    place[1] = puff->pos[1] + up;
    place[2] = puff->pos[2];
    place[0] += puff->drift[0] * age;   /* and spreads outward, across where the car was heading */
    place[2] += puff->drift[1] * age;
    /* grows to the middle of its life, thins to nothing at the end: 0x100 a standard puff */
    const int32_t grow = 0x80 + f;                       /* 0x80 .. 0x17f */
    const int32_t thin = 0x100 - (f * f >> 8) * 7 / 8;    /* 0x100 .. 0x20 */
    *size = grow * thin >> 8;
    *size += (puff->seed & 0x1f) - 0x10;
    if (*size < 1) *size = 1;
    return 1;
}
