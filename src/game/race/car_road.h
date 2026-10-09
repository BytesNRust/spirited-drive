/* Where the road is under a car: its plane at the car (00428918) and the
 * distance from the centre line (00440a68). */
#ifndef TNFS_CAR_ROAD_H
#define TNFS_CAR_ROAD_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 00428918: the body axes and the car's point on the road plane, turned by
 * the yaw into road_point, or by the heading into road_point_heading. */
void car_road_contact(Car *car, const RaceState *race, int use_heading);

/* 00440a68 */
void car_lateral(Car *car, const RaceState *race);

#endif
