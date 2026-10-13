/* The cars by road row (00459d70, 0045a848). */
#include "game/race/row_cars.h"

#include "game/gfx/x86_int.h"

#include <string.h>

enum { PHYSICS_SKIPPED = 6, LANE_EDGE = 0x29999 };

void cars_on_row(const RowCarsOps *ops, const RowCarsInputs *in, int32_t row)
{
    const int32_t d = x_sub(row >= in->camera_node ? row : x_add(x_shl(in->segments, 2), row), in->camera_node);
    if (d >= ROW_CARS_AHEAD || d < 0 || in->ahead[d] == 0) return;
    for (int32_t i = x_sub(in->count, 1); i >= 0; i--) {
        Car *car = in->list[i];
        if (car->physics == PHYSICS_SKIPPED || row != car->draw_node) continue;
        if (car == in->followed && in->chase_kind == 0) {
            if (in->alternate == 0) ops->unseen(ops->ctx, car);
            continue;
        }
        ops->prepare(ops->ctx, car, 0);
        ops->draw(ops->ctx, car, 0);
    }
}

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
