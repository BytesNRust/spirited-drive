/* The engine and the resistance to motion: revs (0042ee68) and drag
 * (0042f630). */
#ifndef TNFS_CAR_ENGINE_H
#define TNFS_CAR_ENGINE_H

#include "game/race/car.h"
#include "game/race/car_specs.h"
#include "game/race/race_state.h"

enum { CAR_GEARS = 9 };   /* -2 reverse, -1 neutral, 0..6 */

/* 0042ee68 */
void car_engine_revs(Car *car, const CarSpecs *specs, const RaceState *race);

/* 0042f630: the force opposing speed (along one axis of the car), rolling
 * and air drag on the car's surface, never more than stops it in a tick. */
int32_t car_drag(Car *car, const CarSpecs *specs, const RaceState *race, int32_t speed, int with_wind);

#endif
