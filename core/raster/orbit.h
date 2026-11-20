/* The Studio's 3D view of a car: an orbit camera about the model. The car is turned (yaw about y, pitch about x: car_pose.h) and put `dist` in front of
 * the eye, as the race draws a car at a distance. Whole integers (1/16 degree, model units), so the same drags give the same picture everywhere. */
#ifndef TNFS_ORBIT_H
#define TNFS_ORBIT_H

#include <stdint.h>

enum {
    ORBIT_TURN = 360 * 16,         /* a turn, in 1/16 degree */
    ORBIT_PITCH_MIN = -10 * 16,    /* the car is seen from a little below the road to straight above it */
    ORBIT_PITCH_MAX = 90 * 16,
    ORBIT_DRAG = 6                 /* 1/16 degrees a pixel of a drag: 0.375 degree */
};

typedef enum { ORBIT_FRONT, ORBIT_SIDE, ORBIT_REAR, ORBIT_TOP } OrbitView;

typedef struct {
    int32_t yaw, pitch;            /* 1/16 degree: yaw 0..ORBIT_TURN-1, pitch ORBIT_PITCH_MIN..ORBIT_PITCH_MAX */
    int32_t dist;                  /* the model's origin this far in front of the eye (model units) */
    int32_t dist_home, dist_min, dist_max;
} Orbit;

/* a camera for a model of `radius` (its farthest vertex from the origin): home at the three-quarter view, the zoom from just outside the model to far */
void orbit_init(Orbit *o, int32_t radius);
/* a drag of dx, dy pixels: dx turns the car about its vertical axis (yaw wraps), dy tips it (pitch is held to its range) */
void orbit_drag(Orbit *o, int dx, int dy);
/* wheel notches (positive in): each takes a tenth off the distance, held between dist_min and dist_max */
void orbit_zoom(Orbit *o, int notches);
/* F, S, R, T: the car seen from the front, the side, the rear, and from above; the distance is kept */
void orbit_view(Orbit *o, OrbitView v);
/* Home: the three-quarter view at the home distance */
void orbit_home(Orbit *o);
/* the car's matrix for the view, as car_body_matrix builds it */
void orbit_matrix(const Orbit *o, int32_t out[9]);

#endif
