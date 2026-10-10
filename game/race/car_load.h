/* A car readied for the race from its slot's entry and performance data
 * (004400f4). */
#ifndef TNFS_CAR_LOAD_H
#define TNFS_CAR_LOAD_H

#include "game/race/car.h"
#include "game/race/car_specs.h"
#include "game/race/race_state.h"

#include <stddef.h>

enum { CAR_MODELS = 12 };

/* 004bc664: the cars' file names (CARSPECS/<name>.PBS and the art), by
 * RaceEntry.car; 8 and 9 repeat MRX7. */
extern const char *const car_model_names[CAR_MODELS];

/* A CARSPECS/<name>.PBS file (QFS-compressed) as the specs record; 0 when
 * it does not unpack to at least the record's size. */
int car_specs_read(CarSpecs *specs, const uint8_t *file, size_t size);

/* 004400f4 from the point the specs are read (the caller loads
 * CARSPECS/<name>.PBS): the checksum and arcade tuning unless
 * race->specs_loaded, the torque curve scaled (00409990), the car's
 * starting fields, then car_setup_physics. The cockpit art for the
 * followed car (0041f9a0) is the caller's, and the debug path behind
 * 004c5ccc (writing "By_R&T" and zeroing the power) is left out. */
void car_load(Car *car, CarSpecs *specs, RaceState *race);

/* 0043fd00: a car at rest at the start: x one unit right of the centre,
 * level, still, in neutral. `handle` is the car's identity where the
 * original keeps its own address (+0x88, and the collision body at
 * handle + 0xc4 in +0xc0). */
void car_reset(Car *car, uint32_t handle);

/* 004406d0: a car joins the race in its slot: human slots load their
 * specs (car_load), the others become computer cars; then car_reset.
 * TODO(004407b8, 00457d40): the collision body and sizes from the model. */
void car_enter(Car *car, CarSpecs *specs, uint32_t handle, RaceState *race);

/* The draw copies the renderer reads (node, lateral, pitch, yaw, roll),
 * taken after each tick. */
void car_keep_draw_state(Car *car);

/* 00409990: the torque curve scaled by the power and the mass, the power
 * first trimmed for automatic gearboxes. */
void car_specs_scale_torque(CarSpecs *specs, int automatic);

#endif
