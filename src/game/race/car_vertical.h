/* A car's height over the road (00428bd8): gravity, the road plane under
 * it, jumps and landings, and the pitch and roll that follow the road. */
#ifndef TNFS_CAR_VERTICAL_H
#define TNFS_CAR_VERTICAL_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* on_road 0 pins the car to the road and stops its vertical motion. */
void car_vertical(Car *car, RaceState *race, int on_road);

#endif
