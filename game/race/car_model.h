/* The car's model as 00457d40 loads it (the CFM's meshes and banks into
 * the car record); its leaves one at a time. */
#ifndef TNFS_CAR_MODEL_H
#define TNFS_CAR_MODEL_H

#include "game/race/car.h"

/* 0045762c: the model's size from the high-detail object's points (car
 * +0x52e's +0x70, x y z words), `count` of them (the ORIP's +0x14 at car
 * +0x52a): each axis from the least to the most, both starting at 0, x
 * 0x200 (wraps) into model_width (x), model_height (y), model_length (z). */
void car_model_size(Car *car, const int32_t (*points)[3], int32_t count);

#endif
