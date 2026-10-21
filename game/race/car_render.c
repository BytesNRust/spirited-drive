#include "game/race/car_render.h"

#include "game/race/car_draw.h"
#include "game/sys/fixed_math.h"

#include <string.h>

enum {
    MIN_Z = 0x20, MAX_Z = 0x5a00, MIRROR_MAX_Z = 0x2100, MIRROR_FAR_Z = 0xa00, FAR_Z = 0x1900, NEAR_Z = 0x219,
    ALWAYS_NEAR_SLOT = 2, ANGLE_SHIFT = 14, TRAIL_BIT = 2,
    TAG_LIFT_HIGH = 0x3800, TAG_GAP_HIGH = 0xf, TAG_LIFT = 0x2000, TAG_GAP = 7
};

/* 00491e60, 00491f30, 00491ec8: turns about x, z and y. */
static void turn_x(int32_t a, int32_t m[9])
{
    const int32_t s = fx_sin((uint32_t)a), c = fx_cos((uint32_t)a);
    const int32_t r[9] = { 0x10000, 0, 0, 0, c, s, 0, -s, c };
    for (int i = 0; i < 9; i++) m[i] = r[i];
}

static void turn_z(int32_t a, int32_t m[9])
{
    const int32_t s = fx_sin((uint32_t)a), c = fx_cos((uint32_t)a);
    const int32_t r[9] = { c, s, 0, -s, c, 0, 0, 0, 0x10000 };
    for (int i = 0; i < 9; i++) m[i] = r[i];
}

static void turn_y(int32_t a, int32_t m[9])
{
    const int32_t s = fx_sin((uint32_t)a), c = fx_cos((uint32_t)a);
    const int32_t r[9] = { c, 0, -s, 0, 0x10000, 0, s, 0, c };
    for (int i = 0; i < 9; i++) m[i] = r[i];
}

static void body_matrix(const Car *car, int32_t out[9])
{
    if (car->use_matrix != 0) {
        for (int i = 0; i < 9; i++) out[i] = car->matrix[i];
        return;
    }
    int32_t x[9], z[9], y[9], zx[9];
    const int32_t pitch = car_suspension_wobble(car->suspension_x, car->suspension_x_index);
    turn_x((int32_t)((uint32_t)pitch + (uint32_t)car->pitch + (uint32_t)car->tilt_x) >> ANGLE_SHIFT, x);
    const int32_t roll = car_suspension_wobble(car->suspension_z, car->suspension_z_index);
    const int32_t tilt = (int32_t)((uint32_t)car->tilt_z - (uint32_t)(car->tilt_z / 4));
    turn_z((int32_t)((uint32_t)roll + (uint32_t)car->roll + (uint32_t)tilt) >> ANGLE_SHIFT, z);
    turn_y(car->heading >> ANGLE_SHIFT, y);
    mat3_mul(z, x, zx);
    mat3_mul(zx, y, out);
}

static int32_t far_model(const Car *car, int32_t mirror, int32_t z)
{
    if (mirror != 0) return z > MIRROR_FAR_Z;
    return z > FAR_Z && car->index != ALWAYS_NEAR_SLOT;
}

static void draw_trail(const CarRenderOps *ops, Car *car)
{
    int32_t offset[2] = { 0, 0 };
    if (car->far == 0) {
        int32_t camera[3], point[2];
        ops->point(ops->ctx, car, camera, offset);
        ops->vertex(ops->ctx, car->body_object, car->smoke_point, point);
        offset[0] = (int32_t)((uint32_t)point[0] - (uint32_t)offset[0]);
        offset[1] = (int32_t)((uint32_t)point[1] - (uint32_t)offset[1]);
    }
    ops->trail(ops->ctx, offset);
}

static void draw_tag(const CarRenderOps *ops, const CarRenderInputs *in, Car *car)
{
    int32_t camera[3], screen[2];
    ops->point(ops->ctx, car, camera, screen);
    const int32_t lift = in->high != 0 ? TAG_LIFT_HIGH / camera[2] + TAG_GAP_HIGH : TAG_LIFT / camera[2] + TAG_GAP;
    ops->tag(ops->ctx, screen[0], (int32_t)((uint32_t)screen[1] - (uint32_t)lift), car->index, camera[2]);
}

void car_render(const CarRenderOps *ops, const CarRenderInputs *in, Car *car, int32_t mirror, int32_t world[9])
{
    const int32_t z = car->camera_position[2], x = car->camera_position[0];
    if (in->port_reach > 0 && car->index != in->followed) {   /* the followed car: the bumper view sits inside it */
        if (z < -in->port_reach || z > MAX_Z) return;
    } else {
        if (z < MIN_Z || z > MAX_Z) return;
        if ((int32_t)((uint32_t)z * 2u) < (x > 0 ? x : (int32_t)(0u - (uint32_t)x))) return;
    }
    if (mirror != 0 && z > MIRROR_MAX_Z) return;
    car->far = far_model(car, mirror, z);
    int32_t body[9];
    body_matrix(car, body);
    mat3_mul(body, in->camera_matrix, world);
    ops->near(ops->ctx, car->body_object, z < NEAR_Z);
    ops->near(ops->ctx, car->far_object, z < NEAR_Z);
    int32_t position[3];  /* +0x391 copied: the packed record's field is not aligned for a pointer */
    memcpy(position, car->camera_position, sizeof position);
    ops->body(ops->ctx, car, world, position);
    if (in->followed == car->index && in->replay == 0) {
        if (car->flags_175 & TRAIL_BIT) draw_trail(ops, car);
        return;
    }
    if (in->race_type < 1) return;
    if (in->racers - 1 < car->index) return;
    if (z < MIN_Z) return;   /* port_reach: the centre behind the eye, no mark */
    draw_tag(ops, in, car);
}
