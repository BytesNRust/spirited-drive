/* The frames between the race's fixed steps (drive_interp.h): a pose blended between two steps and put back after
 * the draw, and the jumps that are shown as they are. */
#include "viewer/drive_interp.h"
#include "test.h"

#include <string.h>

static RaceSim sim;

static void place(int32_t x, int32_t heading, int32_t eye_x, int32_t kind, int32_t tick)
{
    Car *car = sim.race.cars[0];
    car->x = x, car->y = 2 * x, car->z = -x;
    car->heading = heading, car->draw_pitch = heading, car->draw_roll = 0, car->tilt_x = 0, car->tilt_z = 0;
    car->steer = x / 4;
    sim.race.chase.position[0] = eye_x, sim.race.chase.position[1] = 0, sim.race.chase.position[2] = 0;
    sim.race.chase.angles[1] = heading;
    sim.race.chase.kind = kind;
    sim.race.tick = tick;
}

int main(void)
{
    memset(&sim, 0, sizeof sim);
    for (int i = 0; i < RACE_CARS; i++) sim.race.cars[i] = &sim.slots[i].car;
    sim.count = 1;

    DriveInterpPose a, b, saved;
    place(0x100000, 0xfff000, 0x100000, 5, 10);
    drive_interp_capture(&a, &sim);
    place(0x180000, 0x001000, 0x180000, 5, 12);
    drive_interp_capture(&b, &sim);
    const RaceSim before = sim;

    /* the ends are the steps themselves */
    drive_interp_apply(&sim, &a, &b, 0, &saved);
    CHECK_EQ_INT(sim.race.cars[0]->x, 0x100000);
    CHECK_EQ_INT(sim.race.cars[0]->heading, 0xfff000);
    drive_interp_restore(&sim, &saved);
    CHECK(memcmp(&sim, &before, sizeof sim) == 0);
    drive_interp_apply(&sim, &a, &b, 0x10000, &saved);
    CHECK_EQ_INT(sim.race.cars[0]->x, 0x180000);
    CHECK_EQ_INT(sim.race.chase.position[0], 0x180000);
    drive_interp_restore(&sim, &saved);

    /* halfway, and a 24-bit angle the short way round, through 0 */
    drive_interp_apply(&sim, &a, &b, 0x8000, &saved);
    const Car *mid = sim.race.cars[0];
    CHECK_EQ_INT(mid->x, 0x140000);
    CHECK_EQ_INT(mid->y, 0x280000);
    CHECK_EQ_INT(mid->z, -0x140000);
    CHECK_EQ_INT(mid->steer, 0x50000);
    CHECK_EQ_INT(mid->heading & 0xffffff, 0);
    CHECK_EQ_INT(sim.race.chase.position[0], 0x140000);
    CHECK_EQ_INT(sim.race.chase.angles[1] & 0xffffff, 0);
    drive_interp_restore(&sim, &saved);
    CHECK(memcmp(&sim, &before, sizeof sim) == 0);

    /* the eye's node: between the steps the older one's, so the track drawn from it starts at or behind the blended
     * eye (the newer node's segment may begin past it: the road under the eye left out for a frame); the newer at the
     * newer step; put back after */
    sim.race.chase.node = 40;
    drive_interp_capture(&a, &sim);
    sim.race.chase.node = 41;
    drive_interp_capture(&b, &sim);
    drive_interp_apply(&sim, &a, &b, 0x8000, &saved);
    CHECK_EQ_INT(sim.race.chase.node, 40);
    drive_interp_restore(&sim, &saved);
    CHECK_EQ_INT(sim.race.chase.node, 41);
    drive_interp_apply(&sim, &a, &b, 0x10000, &saved);
    CHECK_EQ_INT(sim.race.chase.node, 41);
    drive_interp_restore(&sim, &saved);
    sim.race.chase.node = 0;
    place(0x100000, 0xfff000, 0x100000, 5, 10);
    drive_interp_capture(&a, &sim);
    place(0x180000, 0x001000, 0x180000, 5, 12);
    drive_interp_capture(&b, &sim);

    /* a normal step is not a jump */
    CHECK_EQ_INT(drive_interp_snap(&a, &b), 0);
    /* the jumps: a car moved too far, the eye moved too far, another camera kind, ticks not consecutive, no pose */
    DriveInterpPose j = b;
    j.pos[0][0] = a.pos[0][0] + 0x200001;
    CHECK_EQ_INT(drive_interp_snap(&a, &j), 1);
    j = b, j.eye[2] = a.eye[2] + 0x400001;
    CHECK_EQ_INT(drive_interp_snap(&a, &j), 1);
    j = b, j.chase_kind = 6;
    CHECK_EQ_INT(drive_interp_snap(&a, &j), 1);
    j = b, j.tick = a.tick + 6;
    CHECK_EQ_INT(drive_interp_snap(&a, &j), 1);
    j = b, j.tick = a.tick;
    CHECK_EQ_INT(drive_interp_snap(&a, &j), 1);
    DriveInterpPose none = a;
    none.valid = 0;
    CHECK_EQ_INT(drive_interp_snap(&none, &b), 1);

    /* alpha from the time since the step, held to 0..0x10000 */
    CHECK_EQ_INT(drive_interp_alpha(0, 1000), 0);
    CHECK_EQ_INT(drive_interp_alpha(500, 1000), 0x8000);
    CHECK_EQ_INT(drive_interp_alpha(2000, 1000), 0x10000);
    return TEST_REPORT();
}
