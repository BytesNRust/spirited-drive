/* The race camera's choice of view (race_camera.h). */
#include "game/race/race_camera.h"

enum {
    HALF_TURN = 0x800000,         /* 24-bit angles */
    FULL_TURN = 0x1000000,
    HEADING_SCALE = 0x400,        /* node heading (0x4000 a turn) to the car's angles */
    HEADING_OFF = 0x1e0000,
    TILT_OFF = 0x240000,
    FORCE_SHIFT = 13,
    FORCE_MAX = 100
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

void race_send_force(const Car *car, RaceState *race)
{
    if (race->force_off != 0) return;
    int32_t x = car->lateral_force >> FORCE_SHIFT, y = car->along_force >> FORCE_SHIFT;
    if (x > FORCE_MAX) x = FORCE_MAX;
    else if (x < -FORCE_MAX) x = -FORCE_MAX;
    if (y > FORCE_MAX || y < -FORCE_MAX) y = FORCE_MAX;  /* the original's: a strong backward force reads +100 */
    if (x == race->force_x && y == race->force_y) return;
    race->force_x = x;
    race->force_y = y;
    if (race->sounds.force != NULL) race->sounds.force(race->sounds.user, x, y);
}
