/* What holds the car to the road: the weight on each axle (00430aec). */
#ifndef TNFS_CAR_GRIP_H
#define TNFS_CAR_GRIP_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 00430aec: crests and dips (the slope change to the next node), roll and
 * downforce move weight onto or off the axles. */
void car_axle_loads(Car *car, const RaceState *race);

#endif
