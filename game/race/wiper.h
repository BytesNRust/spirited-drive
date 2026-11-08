/* port (wipers): the windscreen wipers of the dash view in rain and snow. The pure parts: when a blade sweeps (a triangle over a cycle, then parked), where it
 * is, which points of the glass a sweep reaches and when the blade last crossed one; what the lens drops and snow marks do with that is in weather.h
 * (weather_lens_drop_wiped), and where it is drawn in the viewer. Angles are radians, 0 pointing right, counter-clockwise (up on the screen); positions are pixels
 * of the 3D box, y down. */
#ifndef TNFS_WIPER_H
#define TNFS_WIPER_H

#include <stdint.h>

enum { WIPER_BLADES = 2 };
enum { WIPER_OFF = 0, WIPER_LIGHT = 1, WIPER_RAIN = 2, WIPER_STORM = 3, WIPER_SNOW = 4 };   /* what falls: the sweep's pace */

typedef struct WiperBlade {
    float x, y;        /* the pivot */
    float len;         /* the blade's reach from the pivot */
    float park, top;   /* the angle at rest and the angle at the top of the sweep, in the glass's own plane */
    float ux, uy, vx, vy;   /* the glass's axes on the screen (y up): where one unit along the glass's bottom edge (u) and one unit up the glass (v) land; all zero: the identity */
    float xmax;        /* nothing is drawn or swept at or past this x (the dash layer's right edge); 0: no limit */
} WiperBlade;

typedef struct {
    int period;        /* ticks of a whole cycle; 0: no wipers (dry) */
    int sweep;         /* ticks of the cycle the blade moves (up and back); the rest of it is parked */
} WiperTiming;

typedef struct WiperPass {
    WiperBlade blade[WIPER_BLADES];
    int count;
    WiperTiming timing;
    const uint8_t *glass;   /* NULL, or 640x480 flags (1 = the windscreen's glass) of the dash layer: a blade pixel is drawn only where it lands on glass; the layer's pixel of a drawn pixel (x, y) is ((x - glass_x) / glass_scale, (y - glass_y) / glass_scale) */
    float glass_x, glass_y, glass_scale;
} WiperPass;

/* The pace for what falls: light rain slowly with a pause, rain, storm fast, snow in between. WIPER_OFF has period 0. */
WiperTiming wiper_timing(int kind);

/* 0 parked .. 1 at the top: a triangle over the first `sweep` ticks of every cycle, 0 for the rest of it, and always when there is no period. */
float wiper_fraction(uint32_t tick, WiperTiming t);

/* The blade's angle at a fraction. */
float wiper_angle(const WiperBlade *b, float fraction);

/* The screen offset (`*dx`, `*dy`, y down) of the point at `r` from the pivot, at the plane angle `angle`: the blade turns in the glass's plane and the glass's axes put it on the screen, so its tip traces an ellipse. */
void wiper_offset(const WiperBlade *b, float r, float angle, float *dx, float *dy);

/* 1 and `*fraction` (0 at the park .. 1 at the top) when the point is on the glass the blade sweeps: within its reach and between its park and its top; 0 otherwise. */
int wiper_point_fraction(const WiperBlade *b, float x, float y, float *fraction);

/* The tick the blade last crossed the point at or before `tick`, or -1 when it has not yet or never does (outside the arc, no wipers). */
int64_t wiper_last_pass(const WiperBlade *b, WiperTiming t, uint32_t tick, float x, float y);

/* The latest of the blades' passes (wiper_last_pass), -1 for none. */
int64_t wiper_pass_latest(const WiperPass *w, uint32_t tick, float x, float y);

#endif
