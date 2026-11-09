/* port (spray v4, dev prototype: TNFS_SPRAY_V4=1): the spray a car throws up, redone. A particle is born at the tick the wheel was there, at the wheel's own place and height (no catch-up: the
 * emitter runs every tick of the simulation, not every frame drawn), and its look is a pure function of its age. Three kinds: water (large, few, low, opening to the sides, thinning over about a second and
 * a third) gravel (stones, dark, thrown up and falling under gravity, gone in half a second) and the dust that goes with it (light, soft, drifting back and rising, a second). Pure: no drawing here (viewer/spray_v4_draw.h composites the look). Metres and race ticks: the simulation runs 30 steps a second and the race's tick counter goes up by two each (race_frame.c TICK_STEP), so a tick is a sixtieth of a second and every particle is born on an even tick. */
#ifndef TNFS_SPRAY_V4_H
#define TNFS_SPRAY_V4_H

#include <stdint.h>

enum {
    SPRAY_V4_MAX = 1024,
    SPRAY_V4_CARS = 16,
    SPRAY_V4_WATER = 0,
    SPRAY_V4_GRAVEL = 1,
    SPRAY_V4_DUST = 2,           /* the light dust a gravel wheel raises behind the stones */
    SPRAY_V4_DUST_LIFE = 44,     /* ticks */
    SPRAY_V4_WATER_LIFE = 66,    /* ticks: 1.1 s, 20 to 40 m of trail at 65 to 130 km/h */
    SPRAY_V4_GRAVEL_MIN_LIFE = 28,
    SPRAY_V4_GRAVEL_SPREAD = 12  /* ticks added at random to the gravel's life */
};

typedef struct {
    float x, y, z;       /* where it was born (metres) */
    float vx, vy, vz;    /* metres a tick (a sixtieth of a second) */
    float size;          /* radius at birth (metres) */
    uint32_t born;       /* the tick */
    int32_t node;        /* the road node of its car then */
    uint8_t kind, car, live, life;
} SprayV4Particle;

typedef struct {
    SprayV4Particle p[SPRAY_V4_MAX];
    int next;
    uint32_t last_tick[SPRAY_V4_CARS];
    int has_tick[SPRAY_V4_CARS];
} SprayV4;

/* What a car gives for a tick: the wheels (left, right) as they were at that tick, the unit vectors along and across the car (x, z), and what is under it. */
typedef struct {
    int car;
    uint32_t tick;
    int32_t node;
    float speed;         /* metres a second */
    int on_ground;       /* all wheels down */
    int wet;             /* the road is wet here (rain, and not a closed zone) */
    int gravel;          /* the wheels are on gravel: the shoulder, or a rally's compacted gravel */
    float wheel[2][3];   /* metres */
    float forward[2], right[2];
} SprayV4Emit;

void spray_v4_clear(SprayV4 *s);

/* One tick of one car. Once for each tick (the same tick again does nothing; a tick that goes back clears the particles: the clock restarted). Returns how many particles it made. */
int spray_v4_emit(SprayV4 *s, const SprayV4Emit *e);

typedef struct {
    float x, y, z;       /* metres */
    float radius;        /* metres, the puff's half width */
    float alpha;         /* 0..1, the puff's peak before the frame's cap */
} SprayV4Look;

/* How a particle looks at `tick`; 0 when it is not alive. `open` 0..16: how open to the rain the road is (a tunnel's mouth thins it). */
int spray_v4_look(const SprayV4Particle *p, uint32_t tick, int open, SprayV4Look *out);

#endif
