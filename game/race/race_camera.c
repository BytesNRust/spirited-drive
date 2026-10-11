/* The race camera's choice of view (race_camera.h). */
#include "game/race/race_camera.h"

#include "game/race/race_chase.h"

enum {
    HALF_TURN = 0x800000,         /* 24-bit angles */
    FULL_TURN = 0x1000000,
    HEADING_SCALE = 0x400,        /* node heading (0x4000 a turn) to the car's angles */
    HEADING_OFF = 0x1e0000,
    TILT_OFF = 0x240000,
    FORCE_SHIFT = 13,
    FORCE_MAX = 100,
    CAMERA_HOLD = 0x3c,
    KIND_OFF_ROAD = 13,
    KIND_IN_CAR = 1,              /* kinds 0..1 sit in the car, 2..3 just behind it */
    KIND_NEAR = 3,
    KIND_DRIVER = 10,             /* 0 and 1 taken back as 10 and 11 */
    KIND_DRIVER_2 = 11
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

void race_camera_pick(RaceState *race, const Car *car)
{
    if (race->camera_hold != 0) race->camera_hold = (int32_t)((uint32_t)race->camera_hold - 1u);
    RaceChase *chase = &race->chase;
    if (chase->kind == chase->next_kind && chase->kind > KIND_NEAR && chase->kind != KIND_DRIVER) return;
    if (car == NULL) return;  /* the original faults: cars[viewed] is never empty */
    int32_t kind;
    if (!car_faces_road(car, race)) {
        race->camera_hold = CAMERA_HOLD;
        if (chase->kind > KIND_NEAR && chase->kind != KIND_DRIVER && chase->kind != KIND_DRIVER_2) return;
        kind = KIND_OFF_ROAD;
    } else {
        if (chase->kind == chase->next_kind || race->camera_hold != 0) return;
        kind = chase->next_kind == 0 ? KIND_DRIVER : chase->next_kind == KIND_IN_CAR ? KIND_DRIVER_2 : chase->next_kind;
    }
    race_chase_take(race, chase, kind);
}
