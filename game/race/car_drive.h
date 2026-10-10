/* The engine's push on the driven wheels (0042f138) through the torque curve
 * (00409a04), gears and grip, with wheelspin and the revved start. */
#ifndef TNFS_CAR_DRIVE_H
#define TNFS_CAR_DRIVE_H

#include "game/race/car.h"
#include "game/race/car_specs.h"
#include "game/race/race_state.h"

/* 00409a04: torque at the revs, from the curve's 200-rpm points. */
int32_t car_torque(const CarSpecs *specs, int32_t rpm);

/* 0042f138: the drive force along the car; also sets the wheel speed. */
int32_t car_drive_force(Car *car, const CarSpecs *specs, RaceState *race);

#endif
