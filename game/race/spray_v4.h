/* port (spray v4, TNFS_SPRAY_V4 / port.cfg's spray_v4): the spray and dust the wheels throw up, redone. A particle is born at the tick the wheel was there (the simulation's, not the frame's: no catch-up), at the wheel's own place with the
 * ground under it at that height, and its look is a pure function of its age. By what the wheel runs on (SprayV4Surface):
 *   water  (wet road): drops, small and ballistic, thrown back and up in a fan behind each tyre, falling (gravity, a little drag) and gone where they touch the ground; and a fine, low mist that opens to the sides;
 *   gravel (the shoulder, a rally's compacted gravel): stones, dark, thrown up and falling, and a light soft dust behind them;
 *   earth  (a rally's desert dust): a puff that floats up slowly, spreads a lot, drifts back, two seconds;
 *   snow   (a rally on snow): a white puff that opens and settles slowly, and small lumps in a short parabola.
 * Pure: no drawing here (viewer/spray_v4_draw.h composites the looks). Metres, seconds, and race ticks: the simulation runs 30 steps a second and the race's tick counter goes up by two each (race_frame.c TICK_STEP), so a tick is
 * a sixtieth of a second and every particle is born on an even tick. */
#ifndef TNFS_SPRAY_V4_H
#define TNFS_SPRAY_V4_H

#include <stdint.h>

enum {
    SPRAY_V4_MAX = 2048,
    SPRAY_V4_CARS = 16,
    SPRAY_V4_TICKS_A_SECOND = 60
};

/* the kinds of particle: each is drawn with its own colour and opacity */
typedef enum {
    SPRAY_V4_MIST = 0,   /* water: the fine low mist */
    SPRAY_V4_GRAVEL,     /* the stones */
    SPRAY_V4_DUST,       /* the light dust behind the stones */
    SPRAY_V4_DROP,       /* water: the drops */
    SPRAY_V4_EARTH,      /* the desert's dust */
    SPRAY_V4_SNOW,       /* the snow's powder */
    SPRAY_V4_CLUMP,      /* the snow's lumps */
    SPRAY_V4_KINDS
} SprayV4Kind;

typedef enum { SPRAY_V4_NONE = 0, SPRAY_V4_WATER, SPRAY_V4_ON_GRAVEL, SPRAY_V4_ON_EARTH, SPRAY_V4_ON_SNOW } SprayV4Surface;

typedef struct {
    float x, y, z;       /* where it was born (metres) */
    float vx, vy, vz;    /* metres a second at birth */
    float gx, gz;        /* the ground's slope under it: the ground at (x, z) is y + gx (x - x0) + gz (z - z0) */
    float size;          /* radius at birth (metres) */
    uint32_t born;       /* the tick */
    int32_t node;        /* the road node of its car then */
    uint8_t kind, car, live, life;   /* life in ticks */
} SprayV4Particle;

typedef struct {
    SprayV4Particle p[SPRAY_V4_MAX];
    int next;
    uint32_t last_tick[SPRAY_V4_CARS];
    int has_tick[SPRAY_V4_CARS];
    float last_foot[SPRAY_V4_CARS][3];   /* where the car's wheels' middle was the tick before, for the ground's slope */
    int has_foot[SPRAY_V4_CARS];
} SprayV4;

/* What a car gives for a tick: the wheels (left, right) as they were at that tick, the unit vectors along and across the car (x, z), its speed along, and what it runs on. */
typedef struct {
    int car;
    uint32_t tick;
    int32_t node;
    float speed;         /* metres a second */
    int on_ground;       /* all wheels down */
    SprayV4Surface surface;
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

/* How a particle looks at `tick`; 0 when it is not alive (born in the future, past its life, a thing that fell back to the ground, or water in a closed zone). `open` 0..16: how open to the rain the road is. */
int spray_v4_look(const SprayV4Particle *p, uint32_t tick, int open, SprayV4Look *out);

#endif
