#include "raster/orbit.h"

#include "raster/car_pose.h"

enum { HOME_YAW = 215 * 16, HOME_PITCH = 18 * 16, ZOOM_STEP_DEN = 10 };   /* a three-quarter view from the front, a little above */

/* the views' yaw: 0 has the car's rear toward the eye (the model looks down +z) */
static const int32_t VIEW_YAW[4] = { 180 * 16, 90 * 16, 0, 180 * 16 };

static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

void orbit_init(Orbit *o, int32_t radius)
{
    if (radius < 1) radius = 1;
    o->dist_home = radius * 3;
    o->dist_min = radius + radius / 4;
    o->dist_max = radius * 12;
    orbit_home(o);
}

void orbit_home(Orbit *o)
{
    o->yaw = HOME_YAW, o->pitch = HOME_PITCH, o->dist = o->dist_home;
}

void orbit_drag(Orbit *o, int dx, int dy)
{
    o->yaw = (int32_t)(((int64_t)o->yaw + (int64_t)dx * ORBIT_DRAG) % ORBIT_TURN);
    if (o->yaw < 0) o->yaw += ORBIT_TURN;
    o->pitch = clamp(o->pitch + dy * ORBIT_DRAG, ORBIT_PITCH_MIN, ORBIT_PITCH_MAX);
}

void orbit_zoom(Orbit *o, int notches)
{
    for (; notches > 0; notches--) o->dist -= o->dist / ZOOM_STEP_DEN;
    for (; notches < 0; notches++) o->dist += o->dist / ZOOM_STEP_DEN + 1;
    o->dist = clamp(o->dist, o->dist_min, o->dist_max);
}

void orbit_view(Orbit *o, OrbitView v)
{
    o->yaw = VIEW_YAW[v];
    o->pitch = v == ORBIT_TOP ? ORBIT_PITCH_MAX : 0;
}

void orbit_matrix(const Orbit *o, int32_t out[9])
{
    /* the angles as the race keeps them: 24 bits a turn */
    const int32_t heading = (int32_t)((int64_t)o->yaw * 0x1000000 / ORBIT_TURN), tip = (int32_t)((int64_t)o->pitch * 0x1000000 / ORBIT_TURN);
    car_body_matrix(heading, tip, 0, out);
}
