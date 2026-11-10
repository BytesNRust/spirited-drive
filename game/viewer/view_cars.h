/* how a car is drawn in the view (view_cars.c); ported_view.c calls these */
#ifndef TNFS_VIEW_CARS_H
#define TNFS_VIEW_CARS_H

#include "game/race/car_struct.h"

#include <stdint.h>

void row_prepare(void *ctx, Car *car, int pass);   /* a row's car, before it is drawn */
void row_draw(void *ctx, Car *car, int pass);
void draw_cars(void *user, int32_t node);   /* the cars on a node, the parked car's way */
void render_point(void *ctx, Car *car, int32_t camera[3], int32_t screen[2]);

#endif
