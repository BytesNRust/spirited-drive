/* A car's place in camera space and its shadow's corners (0045a3e8). */
#include "game/race/car_shadow.h"

#include "game/gfx/x86_int.h"
#include "game/sys/fixed_math.h"

enum { CAMERA_SHIFT = 9, BOX_SHIFT = 10, NEAR_Z = 0x20, FAR_Z = 0x1900 };

static void camera_place(const int32_t world[3], const ModelView *mv, int32_t out[3])
{
    const int32_t rel[3] = { x_sub(world[0], mv->camera_position[0]) >> CAMERA_SHIFT,
                             x_sub(world[1], mv->camera_position[1]) >> CAMERA_SHIFT,
                             x_sub(world[2], mv->camera_position[2]) >> CAMERA_SHIFT };
    mat3_apply(rel, mv->camera_matrix, out);
}

int car_shadow(const CarShadowOps *ops, Car *car, const ModelView *mv, int pass, int followed, int32_t screen[8])
{
    /* the car's fields copied through locals: Car is packed at the original's offsets */
    const int32_t position[3] = { car->x, car->y, car->z };
    int32_t place[3];
    camera_place(position, mv, place);
    for (int i = 0; i < 3; i++) car->camera_position[i] = place[i];
    if (pass != 0) return 0;
    const int32_t z = place[2], x = place[0];
    if (z < NEAR_Z || z > FAR_Z) return 0;
    if ((x > 0 ? x : x_sub(0, x)) > z) return 0;
    ops->road_contact(ops->ctx, car);
    int32_t centre[3], f[3], s[3];
    const int32_t point[3] = { car->road_point_heading[0], car->road_point_heading[1], car->road_point_heading[2] };
    camera_place(point, mv, centre);
    for (int i = 0; i < 3; i++) f[i] = car->road_forward[i] >> BOX_SHIFT, s[i] = car->road_side[i] >> BOX_SHIFT;
    int32_t corners[4][3];
    for (int i = 0; i < 3; i++) {
        corners[0][i] = x_add(x_sub(0, f[i]), s[i]);
        corners[1][i] = x_sub(x_sub(0, f[i]), s[i]);
        corners[2][i] = x_sub(f[i], s[i]);
        corners[3][i] = x_add(f[i], s[i]);
    }
    for (int k = 0; k < 4; k++) {
        int32_t turned[3];
        mat3_apply(corners[k], mv->camera_matrix, turned);
        const int32_t p[3] = { x_add(turned[0], centre[0]), x_add(turned[1], centre[1]), x_add(turned[2], centre[2]) };
        view_project(mv->view, p, screen + 2 * k);
    }
    if (followed) ops->followed(ops->ctx, car, f, s, point);
    return 1;
}
