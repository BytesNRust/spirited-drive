#include "game/race/car_body_draw.h"

enum { CAR_PARTS = 4, PART_LIMIT = 8 };

static void lights_write(void *ctx, int32_t first, int32_t count, const uint8_t *colours)
{
    const CarBodyOps *ops = ctx;
    ops->palette(ops->ctx, first, count, colours);
}

static void draw_far(const CarBodyOps *ops, const Car *car, const void *matrix)
{
    ops->matrix(ops->ctx, car->far_object, matrix);
    ops->transform(ops->ctx, car->far_object);
    ops->project(ops->ctx, car->far_object);
    ops->sort(ops->ctx, car->far_object);
    ops->draw(ops->ctx, car->far_object);
}

void car_body_draw(const CarBodyOps *ops, const CarLightInputs *lights, Car *car, const Car *followed,
                   const void *matrix, const int32_t position[3])
{
    const int32_t eye[3] = { (int32_t)(0u - (uint32_t)position[0]), (int32_t)(0u - (uint32_t)position[1]),
                             (int32_t)(0u - (uint32_t)position[2]) };
    ops->eye(ops->ctx, eye);
    if (car->far != 0) {
        draw_far(ops, car, matrix);
        return;
    }
    if (car == followed) {
        ops->wheel_frames(ops->ctx, car);
        ops->steer(ops->ctx, car);
    }
    const CarLightOps light_ops = { (void *)ops, lights_write };
    car_lights(&light_ops, lights, car);
    ops->matrix(ops->ctx, car->body_object, matrix);
    ops->transform(ops->ctx, car->body_object);
    ops->project(ops->ctx, car->body_object);
    if (car == followed) ops->smoke(ops->ctx, car, 1);
    const struct { int32_t part, count; uint32_t list; } parts[CAR_PARTS] = {
        { car->inside_part, car->inside_count, car->inside_list },
        { car->surface_part, car->surface_count, car->surface_list },
        { car->outside_part, car->outside_count, car->outside_list },
        { car->convex_part, car->convex_count, car->convex_list },
    };
    for (int i = 0; i < CAR_PARTS; i++) {
        if ((uint32_t)parts[i].part >= PART_LIMIT) continue;
        ops->sort_list(ops->ctx, car->body_object, parts[i].count, parts[i].list);
        ops->draw_list(ops->ctx, car->body_object, parts[i].count, parts[i].list);
    }
    if (car == followed) ops->smoke(ops->ctx, car, 0);
}
