/* port (rain_fx, rain v5): the water a car throws up on a wet road as particles of the WORLD, not of the car: each wheel drops one where it passes (rain_spray_emit, once a
 * tick), and it stays where it was dropped while the car goes on, a low mist at the wheel's height that widens and thins for RAIN_SPRAY_LIFE ticks and is gone, as a skid mark is laid.
 * Each remembers the road node it was dropped at, so the drawing can put it in the painter's order of the road (the scenery nearer than the node covers it, and the cars: the
 * player's too). Pure: no drawing, no globals. */
#ifndef TNFS_RAIN_SPRAY_H
#define TNFS_RAIN_SPRAY_H

#include <stdint.h>

enum {
    RAIN_SPRAY_MAX = 768,    /* the ring: 8 cars x 2 wheels x the life, with room over */
    RAIN_SPRAY_LIFE = 20,    /* ticks (30 a second): two thirds of a second */
    RAIN_SPRAY_WISP_LIFE = 3,/* a wisp's (traffic_spray): a tenth of a second, so one to three sprites of a car on the road at once */
    RAIN_SPRAY_WISP2_LIFE = 6,     /* a rain_v2 wisp's: a fifth of a second, still far under the plume's two thirds */
    RAIN_SPRAY_WISP2_MIN_SPEED = 1,/* metres a second: a car that moves at all leaves one */
    RAIN_SPRAY_CARS = 16,
    RAIN_SPRAY_LEVELS = 4    /* the density steps a particle's look has (the drawing has a shape for each) */
};

typedef struct {
    int32_t x, y, z;         /* where it was dropped (16.16 metres) */
    int32_t drift_x, drift_z;/* how far it has spread sideways by the end of its life (16.16 m) */
    int32_t node;            /* the road node it was dropped at */
    uint32_t born;           /* the tick */
    uint8_t size;            /* 0..255: the speed's, a faster car throws a bigger cloud */
    uint8_t car;             /* whose */
    uint8_t live;
    uint8_t life;            /* its life in ticks (RAIN_SPRAY_LIFE, or RAIN_SPRAY_WISP_LIFE for a wisp) */
} RainSprayParticle;

typedef struct {
    RainSprayParticle p[RAIN_SPRAY_MAX];
    int next;
    uint32_t last_tick[RAIN_SPRAY_CARS];
    int has_tick[RAIN_SPRAY_CARS];
} RainSpray;

void rain_spray_clear(RainSpray *s);

/* The car's heading as a unit vector (x, z; 16.16) for its wheels: its yaw (16.16 turn, the high bits: `yaw >> 8`), or, for a car that is drawn with its matrix (use_matrix: the rivals' and the
 * traffic's, whose yaw stays 0), the matrix's forward (`matrix6`, `matrix8`: its x and z). The matrix's is normalised; a degenerate one falls back to the yaw. */
void rain_spray_heading(int use_matrix, int32_t yaw, int32_t matrix6, int32_t matrix8, int32_t forward[2]);

/* Where a car's rear wheels are and which way is its right: `at` (x, y, z, 16.16 m) the car's centre, `forward` its heading (rain_spray_heading). wheel[0] the left, wheel[1] the right
 * (1.4 m behind the centre, 0.8 m out, 0.3 m down); `right` the car's right (x, z; 16.16 unit). */
void rain_spray_wheels(const int32_t at[3], const int32_t forward[2], int32_t wheel[2][3], int32_t right[2]);

/* A tick of car `car` (0..RAIN_SPRAY_CARS - 1): once for each tick (a frame drawn again at the same tick drops nothing, a tick that goes back clears the trails), each wheel drops a
 * particle at `left` / `right_wheel` (16.16 m, the road under the wheel) when `wet` (on the road, on the ground) and above 8 m/s (`speed` 16.16 m/s). `right` is the car's right (x, z;
 * 16.16 unit) so the mist spreads away from it. Returns how many were dropped. */
int rain_spray_step(RainSpray *s, int car, uint32_t tick, const int32_t left[3], const int32_t right_wheel[3], const int32_t right[2], int32_t node, int32_t speed, int wet);

/* The same for a wisp (port traffic_spray): one particle a tick, from the middle of the two wheels, living RAIN_SPRAY_WISP_LIFE ticks. Returns how many were dropped (1, or 0 as above). */
int rain_spray_step_wisp(RainSpray *s, int car, uint32_t tick, const int32_t left[3], const int32_t right_wheel[3], const int32_t right[2], int32_t node, int32_t speed, int wet);

/* The same for rain_v2's wisp: one particle a tick from each wheel living RAIN_SPRAY_WISP2_LIFE ticks, from RAIN_SPRAY_WISP2_MIN_SPEED m/s up. Returns how many were dropped (2, or 0 as above). */
int rain_spray_step_wisp2(RainSpray *s, int car, uint32_t tick, const int32_t left[3], const int32_t right_wheel[3], const int32_t right[2], int32_t node, int32_t speed, int wet);

/* How a particle looks at `tick`: its place (16.16 m: dropped, drifted, risen a little), its radius (16.16 m: widening) and its density 0..RAIN_SPRAY_LEVELS - 1 (thinning; 0 the
 * faintest). 0 when it is not alive (never emitted, or past its life). `open` (0..16) thins and lowers it in a tunnel (road water only). */
int rain_spray_look(const RainSprayParticle *p, uint32_t tick, int open, int32_t place[3], int32_t *radius, int *density);

#endif
