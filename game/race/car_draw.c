#include "game/race/car_draw.h"

#include "game/sys/fixed_math.h"
#include "raster/car_pose.h"

#include <stdlib.h>

static const int32_t IDENTITY[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };
static const char *const SECTION_NAMES[CAR_SECTIONS] = { "inside", "surface", "outside", "convex" };

int car_model_init(CarModel *car, Object3D *body, Object3D *far)
{
    car->body = body;
    car->far = far;
    for (int i = 0; i < CAR_SECTIONS; i++) {
        const int section = object3d_section(body, SECTION_NAMES[i]);
        car->sections[i] = (Object3DList){ 0, NULL };
        if (section >= 0 && section < 8 && !object3d_section_list(body, section, &car->sections[i])) return 0;
    }
    return 1;
}

void car_model_free(CarModel *car)
{
    for (int i = 0; i < CAR_SECTIONS; i++) {
        free(car->sections[i].order);
        car->sections[i] = (Object3DList){ 0, NULL };
    }
}

/* 004591d4: z about x, then y. The angles reach the builders as 16-bit
 * values >> 14 (pushed as ushort). */
static void body_matrix(const CarPose *pose, int32_t out[9])
{
    if (pose->matrix != NULL) {
        for (int i = 0; i < 9; i++) out[i] = pose->matrix[i];
        return;
    }
    car_body_matrix(pose->heading, pose->angle_x, pose->angle_z, out);
}

static void place(Object3D *obj, const int32_t matrix[9], const int32_t eye[3], const View *view)
{
    object3d_set_matrix(obj, matrix);
    object3d_transform(obj, IDENTITY, eye, 0);
    object3d_project(obj, view);
}

/* 00458d4c */
static void draw_body(CarModel *car, int far, const int32_t matrix[9], const int32_t cam[3], const View *view,
                      const Object3DSink *sink, const CarSmokeHook *smoke)
{
    const int32_t eye[3] = { (int32_t)(0u - (uint32_t)cam[0]), (int32_t)(0u - (uint32_t)cam[1]),
                             (int32_t)(0u - (uint32_t)cam[2]) };
    if (far) {
        place(car->far, matrix, eye, view);
        object3d_sort(car->far);
        object3d_draw(car->far, MODEL_NEAR_Z, sink);
        return;
    }
    place(car->body, matrix, eye, view);
    if (smoke != NULL) smoke->draw(smoke->ctx, 1);
    for (int i = 0; i < CAR_SECTIONS; i++) {
        if (car->sections[i].order == NULL) continue;
        object3d_sort_list(car->body, &car->sections[i]);
        object3d_draw_list(car->body, &car->sections[i], MODEL_NEAR_Z, sink);
    }
    if (smoke != NULL) smoke->draw(smoke->ctx, 0);
}

static uint32_t magnitude(int32_t v)
{
    return v > 0 ? (uint32_t)v : 0u - (uint32_t)v;
}

static void set_near(Object3D *obj, int on)
{
    if (on) {
        obj->flags |= OBJECT3D_FLAG_NEAR;
    } else {
        obj->flags &= ~(uint32_t)OBJECT3D_FLAG_NEAR;
    }
}

static int32_t distance_scale = 1;   /* port: car_draw_set_distance_scale */

void car_draw_set_distance_scale(int32_t scale) { distance_scale = scale < 1 ? 1 : scale > 3 ? 3 : scale; }
int32_t car_draw_distance_scale(void) { return distance_scale; }
int32_t car_draw_max_z(int32_t scale) { return CAR_MAX_Z * (scale < 1 ? 1 : scale > 3 ? 3 : scale); }
int32_t car_draw_far_z(int32_t scale) { return CAR_FAR_Z * (scale < 1 ? 1 : scale > 3 ? 3 : scale); }

static int draw_at(CarModel *car, const CarPose *pose, const int32_t cam[3], const ModelView *mv, int mirror,
                   const Object3DSink *sink, const CarSmokeHook *smoke)
{
    const int32_t z = cam[2];
    if (z < CAR_MIN_Z || z > car_draw_max_z(distance_scale)) return 0;
    if ((int64_t)magnitude(cam[0]) > 2 * (int64_t)z) return 0;
    int far;
    if (!mirror) {
        far = z > car_draw_far_z(distance_scale) && !pose->always_near;
    } else {
        if (z > CAR_MIRROR_MAX_Z) return 0;
        far = z > CAR_MIRROR_FAR_Z;
    }
    if (car->far == NULL) far = 0;
    int32_t body[9], combined[9];
    body_matrix(pose, body);
    mat3_mul(body, mv->camera_matrix, combined);
    set_near(car->body, z < MODEL_NEAR_FLAG_Z);
    if (car->far != NULL) set_near(car->far, z < MODEL_NEAR_FLAG_Z);
    draw_body(car, far, combined, cam, mv->view, sink, smoke);
    return 1;
}

int car_draw_at(CarModel *car, const CarPose *pose, const int32_t cam[3], const ModelView *mv, int mirror,
                const Object3DSink *sink)
{
    return draw_at(car, pose, cam, mv, mirror, sink, NULL);
}

int car_draw(CarModel *car, const CarPose *pose, const ModelView *mv, int mirror, const Object3DSink *sink)
{
    return car_draw_smoking(car, pose, mv, mirror, sink, NULL);
}

int car_draw_smoking(CarModel *car, const CarPose *pose, const ModelView *mv, int mirror, const Object3DSink *sink,
                     const CarSmokeHook *smoke)
{
    /* 0045a3e8: camera space at world >> 9 */
    int32_t rel[3], cam[3];
    for (int i = 0; i < 3; i++)
        rel[i] = (int32_t)((uint32_t)pose->position[i] - (uint32_t)mv->camera_position[i]) >> 9;
    mat3_apply(rel, mv->camera_matrix, cam);
    return draw_at(car, pose, cam, mv, mirror, sink, smoke);
}

/* 004bad44: the wobble by step (past 33 the words are other data). */
static const int8_t WOBBLE[34] = { 0, 0, -1, -2, -2, -1, 0, 1, 2, 3, 4, 5, 5, 4, 3, 2, 0,
                                   -2, -4, -6, -7, -8, -7, -6, -5, -3, 0, 4, 8, 12, 16, 16, 16, 16 };

int32_t car_suspension_wobble(int32_t amplitude, int32_t step)
{
    if (step < 0 || step >= (int32_t)sizeof WOBBLE) return 0;  /* port guard */
    return (int32_t)((uint32_t)WOBBLE[step] * (uint32_t)amplitude) / 16;
}
