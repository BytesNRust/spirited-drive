#include "game/race/model_draw.h"

#include "game/sys/fixed_math.h"

static const int32_t IDENTITY[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };

/* 00491ec8: a turn about y from a 10-bit angle (sine and cosine by 004a4208). */
static void yaw_matrix(uint32_t angle, int32_t m[9])
{
    const int32_t s = fx_sin(angle), c = fx_cos(angle);
    m[0] = c, m[1] = 0, m[2] = -s;
    m[3] = 0, m[4] = 0x10000, m[5] = 0;
    m[6] = s, m[7] = 0, m[8] = c;
}

static uint32_t magnitude(int32_t v)
{
    return v > 0 ? (uint32_t)v : 0u - (uint32_t)v;
}

/* 00458c04: whole-object cull; z is in front, x within the view cone or
 * within MODEL_SIDE_MARGIN of the axis. */
static int visible(const int32_t cam[3], int half_side_test)
{
    const int32_t z = cam[2];
    if (z < MODEL_MIN_Z || z > MODEL_MAX_Z) return 0;
    const int32_t side = half_side_test ? cam[0] / 2 : cam[0];
    return !((int64_t)magnitude(side) > z && magnitude(cam[0]) > MODEL_SIDE_MARGIN);
}

/* 00458b7c: the engine's camera sits at minus the object's offset, so the
 * object's own position (0) lands on it. */
static void draw(Object3D *obj, const int32_t cam[3], const int32_t matrix[9], const View *view,
                 const Object3DSink *sink)
{
    const int32_t eye[3] = { (int32_t)(0u - (uint32_t)cam[0]), (int32_t)(0u - (uint32_t)cam[1]),
                             (int32_t)(0u - (uint32_t)cam[2]) };
    object3d_set_matrix(obj, matrix);
    object3d_transform(obj, IDENTITY, eye, 0);
    object3d_project(obj, view);
    object3d_sort(obj);
    object3d_draw(obj, MODEL_NEAR_Z, sink);
}

int model_draw_object(Object3D *obj, const int32_t position[3], int32_t heading, const ModelView *mv,
                      const Object3DSink *sink)
{
    int32_t yaw[9], rel[3], cam[3], combined[9];
    yaw_matrix((uint32_t)(heading >> 14), yaw);
    for (int i = 0; i < 3; i++)
        rel[i] = (int32_t)((uint32_t)position[i] - (uint32_t)mv->camera_position[i]) >> 12;
    mat3_apply(rel, mv->camera_matrix, cam);
    if (!visible(cam, mv->half_side_test)) return 0;
    if (cam[2] < MODEL_NEAR_FLAG_Z) {
        obj->flags |= OBJECT3D_FLAG_NEAR;
    } else {
        obj->flags &= ~(uint32_t)OBJECT3D_FLAG_NEAR;
    }
    mat3_mul(yaw, mv->camera_matrix, combined);
    draw(obj, cam, combined, mv->view, sink);
    return 1;
}
