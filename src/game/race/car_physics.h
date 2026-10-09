/* The driving physics (00430cb0): one tick of a car on the road, from the
 * engine and the brakes through the tyres to the car's speed, turn and
 * place, then the walls and the drag strip's timing. */
#ifndef TNFS_CAR_PHYSICS_H
#define TNFS_CAR_PHYSICS_H

#include "game/race/car.h"
#include "game/race/car_specs.h"
#include "game/race/race_state.h"

/* 00430cb0 */
void car_driving_physics(Car *car, const CarSpecs *specs, RaceState *race);

/* 004286a0: the driving model (00509dac index 2): the physics, then the
 * car's place along the track and its height over the road. */
void car_driving_tick(Car *car, const CarSpecs *specs, RaceState *race);

#endif
