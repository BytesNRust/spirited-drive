/* port (psx_flares): the halo round a car's lit lamps (the brake light, the reversing light, the headlights, the police car's red and blue), found in the rectangle of the car's projected vertices after
 * the car is drawn, and the pairs of lamps a far traffic or police car is given by its rectangle (traffic_halo, police_halo). */
#ifndef TNFS_CAR_LAMP_HALO_H
#define TNFS_CAR_LAMP_HALO_H

#include "game/gfx/object3d.h"
#include "game/gfx/quad.h"
#include "game/race/car_struct.h"
#include "game/race/race_state.h"

#include <stdint.h>

/* `body` is the model the pass drew for `car` (the near body or the far one: the other one's screen corners are another view's); `police_flash_on` is the police lights' switch. */
void car_lamp_halo_draw(const Object3D *body, const Car *car, const RaceState *race, int police_flash_on, const QuadTarget *t, const uint8_t palette[768]);

#endif
