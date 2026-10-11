/* The race camera's choice of view (0045573c, 00405b00, 00405de8). */
#ifndef TNFS_RACE_CAMERA_H
#define TNFS_RACE_CAMERA_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 00405de8: the car points along its node's heading within 0x1e0000 and
 * its pitch and roll are both within 0x240000 (a 24-bit turn). */
int car_faces_road(const Car *car, const RaceState *race);

#endif
