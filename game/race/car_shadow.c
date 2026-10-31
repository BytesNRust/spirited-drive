/* A car's place in camera space and its shadow's corners (0045a3e8). */
#include "game/race/car_shadow.h"

#include "game/gfx/flat.h"
#include "game/gfx/x86_int.h"
#include "game/sys/fixed_math.h"

#include <string.h>

enum { CAMERA_SHIFT = 9, BOX_SHIFT = 10, NEAR_Z = 0x20, FAR_Z = 0x1900, SHADOW_REACH = 0x200 };   /* the reach: a box's half length and width together, at most, behind the lens */

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
    if (z > FAR_Z || (ops->clip_quad == NULL && z < NEAR_Z)) return 0;   /* port: the cut keeps a car nearer than 0x20 (the cone below drops the one behind the lens) */
    const int wide = ops->clip_quad != NULL && ops->frustum != 0;   /* port: the field's own frustum, tested on the cut box below */
    if (wide ? z < -SHADOW_REACH : (x > 0 ? x : x_sub(0, x)) > z) return 0;
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
    int32_t place_of[4][3];
    for (int k = 0; k < 4; k++) {
        int32_t turned[3];
        mat3_apply(corners[k], mv->camera_matrix, turned);
        for (int i = 0; i < 3; i++) place_of[k][i] = x_add(turned[i], centre[i]);
    }
    if (ops->clip_quad != NULL) {   /* port: the corners behind the plane slid to it, not lost */
        const int32_t plane = mv->view->near_z + 1;
        const int32_t *const in[4] = { place_of[0], place_of[1], place_of[2], place_of[3] };
        int32_t cut[4][3];
        int behind = 0;
        for (int k = 0; k < 4; k++) behind |= place_of[k][2] < plane;
        if (behind) {
            if (!ops->clip_quad(in, plane, cut)) return 0;
            memcpy(place_of, cut, sizeof place_of);
        }
    }
    if (wide) {   /* the box wholly outside the field's left or right side: nothing to draw */
        int right = 1, left = 1;
        for (int k = 0; k < 4; k++) {
            const int64_t side = (int64_t)place_of[k][0] * mv->view->focal_x, reach = (int64_t)place_of[k][2] * mv->view->centre_x;
            right &= side > reach, left &= -side > reach;
        }
        if (right || left) return 0;
    }
    for (int k = 0; k < 4; k++) view_project(mv->view, place_of[k], screen + 2 * k);
    if (followed) ops->followed(ops->ctx, car, f, s, point);
    return 1;
}

int car_prepare(const CarShadowOps *ops, Car *car, const ModelView *mv, int pass, int followed, const QuadTarget *target,
                const uint8_t *remap, int32_t screen[8])
{
    if (car->use_matrix != 0) car->heading = (int32_t)((uint32_t)fx_atan2(car->matrix[6], car->matrix[8]) << 8);
    if (!car_shadow(ops, car, mv, pass, followed, screen)) return 0;
    flat_polygon_shade(target, remap, 4, screen, 0);
    return 1;
}

void car_skid_unseen(const CarShadowOps *ops, Car *car)
{
    ops->road_contact(ops->ctx, car);
    int32_t f[3], s[3];
    const int32_t point[3] = { car->road_point_heading[0], car->road_point_heading[1], car->road_point_heading[2] };
    for (int i = 0; i < 3; i++) f[i] = car->road_forward[i] >> BOX_SHIFT, s[i] = car->road_side[i] >> BOX_SHIFT;
    ops->followed(ops->ctx, car, f, s, point);
}
