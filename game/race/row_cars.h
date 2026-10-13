/* The cars by road row (00459d70 before the track, 0045a848 between the
 * rows): which rows ahead of the camera hold a car, and the cars of a row. */
#ifndef TNFS_ROW_CARS_H
#define TNFS_ROW_CARS_H

#include "game/race/car_struct.h"
#include "game/race/mirror_scene.h"

#include <stdint.h>

enum { ROW_CARS_AHEAD = 0x23 };

/* 00459d70 (watcall (camera node), from 0044ce3f): `ahead` (0051c7ac,
 * ROW_CARS_AHEAD) cleared; each of the `count` cars (00509e1c, 004fb7e8) not
 * physics 6 moves its draw_node (+0x341) back by one when its lateral
 * (+0x39d) is below 0x29999, on by the ring (0050420c x 4) when that is
 * negative; the row it is ahead of the camera, under 0x23, is set. Then
 * 00459cc4 (mirror_car_rows) into `behind`. */
void car_rows_ahead(Car *const *cars, int32_t count, int32_t segments, int32_t camera_node,
                    uint8_t ahead[ROW_CARS_AHEAD], uint8_t behind[MIRROR_CAR_ROWS]);

#endif
