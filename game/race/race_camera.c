/* The race camera's choice of view (race_camera.h). */
#include "game/race/race_camera.h"

enum {
    HALF_TURN = 0x800000,         /* 24-bit angles */
    FULL_TURN = 0x1000000,
    HEADING_SCALE = 0x400,        /* node heading (0x4000 a turn) to the car's angles */
    HEADING_OFF = 0x1e0000,
    TILT_OFF = 0x240000
};

static int32_t magnitude(int32_t v)
{
    return v < 1 ? -v : v;
}

int car_faces_road(const Car *car, const RaceState *race)
{
    const TriNode n = race_node(race, car->node);
    int32_t off = car->yaw + n.orientation * -HEADING_SCALE;
    if (off > HALF_TURN) off -= FULL_TURN;
    else if (off < -HALF_TURN) off += FULL_TURN;
    if (off >= HEADING_OFF || off <= -HEADING_OFF) return 0;
    return magnitude(car->roll) <= TILT_OFF && magnitude(car->pitch) <= TILT_OFF;
}
