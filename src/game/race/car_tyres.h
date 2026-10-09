/* The tyres' forces on the road (0042fb88): the slip angle, the grip tables
 * by slip, locked wheels, drift, wheelspin, ABS and traction control. */
#ifndef TNFS_CAR_TYRES_H
#define TNFS_CAR_TYRES_H

#include "game/race/car.h"
#include "game/race/car_specs.h"
#include "game/race/race_state.h"

/* 0042fb88 for one axle (1 front, 2 rear). The forces come in along the
 * car (lateral_in sideways, along_in forwards), are turned into the
 * wheels' frame by steer, limited, and turned back. drive and brake are
 * the axle's push and braking. *out_lateral and *out_along are read before
 * they are written (00509e0c). */
void car_tyre_forces(Car *car, const CarSpecs *specs, RaceState *race, int32_t *out_lateral, int32_t *out_along,
                     int32_t lateral_in, int32_t along_in, int32_t steer, int32_t drive, int32_t brake, int axle);

#endif
