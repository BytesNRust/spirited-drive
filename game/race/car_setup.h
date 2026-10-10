/* A car's physical constants from its performance data (0042ff60): the
 * axles' shares of the weight, the loads at rest and the turning inertia. */
#ifndef TNFS_CAR_SETUP_H
#define TNFS_CAR_SETUP_H

#include "game/race/car.h"
#include "game/race/car_specs.h"
#include "game/race/race_state.h"

/* 0042ff60: also sets the race's surfaces (0042f734) and, as the original,
 * scales the specs' drag by the mass in place, so it runs once per specs. */
void car_setup_physics(Car *car, CarSpecs *specs, RaceState *race);

#endif
