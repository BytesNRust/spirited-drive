/* The cars by road row (00459d70, 0045a848). */
#include "game/race/row_cars.h"

#include "game/gfx/x86_int.h"

#include <string.h>

enum { PHYSICS_SKIPPED = 6, LANE_EDGE = 0x29999 };

void car_rows_ahead(Car *const *cars, int32_t count, int32_t segments, int32_t camera_node,
                    uint8_t ahead[ROW_CARS_AHEAD], uint8_t behind[MIRROR_CAR_ROWS])
{
    memset(ahead, 0, ROW_CARS_AHEAD);
    const int32_t ring = x_shl(segments, 2);
    for (int32_t i = 0; i < count; i++) {
        Car *car = cars[i];
        if (car->physics == PHYSICS_SKIPPED) continue;
        int32_t node = car->draw_node;
        if (x_sub(car->lateral, LANE_EDGE) < 0) node = x_sub(node, 1);
        if (node < 0) node = x_add(node, ring);
        car->draw_node = node;
        const int32_t d = node >= camera_node ? x_sub(node, camera_node) : x_sub(x_add(node, ring), camera_node);
        if (d < ROW_CARS_AHEAD) ahead[d] = 1;
    }
    mirror_car_rows(cars, count, segments, camera_node, behind);
}
