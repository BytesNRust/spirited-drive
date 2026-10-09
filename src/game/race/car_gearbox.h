/* Gear changes (0042ed94) and the automatic gearbox (004098a0). */
#ifndef TNFS_CAR_GEARBOX_H
#define TNFS_CAR_GEARBOX_H

#include "game/race/car.h"
#include "game/race/car_specs.h"
#include "game/race/race_state.h"

/* 004098a0: one gear up or down when the revs leave the gear's band. */
void car_auto_shift(Car *car, const CarSpecs *specs);

/* 0042ed94: runs a gear change's throttle cut, and starts one when the
 * automatic gearbox moves. */
void car_gear_change(Car *car, const CarSpecs *specs, const RaceState *race);

#endif
