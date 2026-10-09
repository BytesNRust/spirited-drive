/* A car readied for the race from its slot's entry and performance data
 * (004400f4). */
#ifndef TNFS_CAR_LOAD_H
#define TNFS_CAR_LOAD_H

#include "game/race/car.h"
#include "game/race/car_specs.h"
#include "game/race/race_state.h"

enum { CAR_MODELS = 12 };

/* 004bc664: the cars' file names (CARSPECS/<name>.PBS and the art), by
 * RaceEntry.car; 8 and 9 repeat MRX7. */
extern const char *const car_model_names[CAR_MODELS];

/* 004400f4 from the point the specs are read (the caller loads
 * CARSPECS/<name>.PBS): the checksum and arcade tuning unless
 * race->specs_loaded, the torque curve scaled (00409990), the car's
 * starting fields, then car_setup_physics. The cockpit art for the
 * followed car (0041f9a0) is the caller's, and the debug path behind
 * 004c5ccc (writing "By_R&T" and zeroing the power) is left out. */
void car_load(Car *car, CarSpecs *specs, RaceState *race);

/* 00409990: the torque curve scaled by the power and the mass, the power
 * first trimmed for automatic gearboxes. */
void car_specs_scale_torque(CarSpecs *specs, int automatic);

#endif
