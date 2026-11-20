/* port (body shine): see shine_pass.h. The pieces come from the race's object sink and body shine (game/viewer/object_sink.c, body_shine.c). */
#include "raster/shine_pass.h"

#include "formats/fixed_math.h"
#include "raster/car_envmap.h"
#include "raster/shine_class.h"

#include <math.h>
#include <string.h>

void shine_tables_make(const uint8_t palette[768], const uint8_t usable[256], const uint8_t emissive[256], ShineTables *t)
{
    car_shine_tables(palette, usable, emissive, t->paint, t->glass);
    car_envmap_hard(palette, (const uint8_t(*)[256])t->paint, t->paint_hard);
    car_envmap_hard(palette, (const uint8_t(*)[256])t->glass, t->glass_hard);
}

EnvShine shine_tables_env(const ShineTables *t, const uint8_t *image)
{
    return (EnvShine){ t->paint, t->glass, image, t->paint_hard, t->glass_hard };
}

void shine_world_up(const int32_t camera_matrix[9], float out[3])
{
    const int32_t up_world[3] = { 0, 0x10000, 0 };
    int32_t c[3];
    mat3_apply(up_world, camera_matrix, c);
    const float len = sqrtf((float)c[0] * (float)c[0] + (float)c[1] * (float)c[1] + (float)c[2] * (float)c[2]);
    if (len <= 0) {
        out[0] = 0, out[1] = 1, out[2] = 0;
        return;
    }
    for (int k = 0; k < 3; k++) out[k] = (float)c[k] / len;
}

int shine_corner_uvs(const Object3DCorners *c, const float up[3], int32_t packed[4])
{
    if (c->normal == NULL || c->corners < 3 || c->corners > 4) return 0;
    for (int i = 0; i < c->corners; i++) {
        const float *n = c->normal + 3 * i;
        const float p[3] = { (float)c->camera[3 * i], (float)c->camera[3 * i + 1], (float)c->camera[3 * i + 2] };
        const float len = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
        float uv[2] = { 0.5f, 0.5f };
        if (len > 0 && !(n[0] == 0 && n[1] == 0 && n[2] == 0)) {
            const float v[3] = { p[0] / len, p[1] / len, p[2] / len };
            car_envmap_uv(n, v, up, uv);
        }
        packed[i] = car_envmap_pack(uv);
    }
    return 1;
}

int shine_shape_uv(ObjShape shape, const FshEntry *t, const Object3DCorners *c, int32_t uv[8])
{
    const int32_t w = (int16_t)t->width, h = (int16_t)t->height;
    switch (shape) {
    case OBJ_TRIANGLE:
    case OBJ_QUAD:
    case OBJ_QUAD_TRIS:
        for (int i = 0; i < 8; i++) uv[i] = (int32_t)((uint32_t)c->uv[i] << 16);
        return 1;
    case OBJ_FULL: {
        const int32_t r = (w << 16) - 1, b = (h << 16) - 1;
        const int32_t full[8] = { 0, 0, r, 0, r, b, 0, b };
        memcpy(uv, full, sizeof full);
        return 1;
    }
    case OBJ_INSET: {
        const int32_t r = (w - 1) << 16, b = (h - 1) << 16, o = 1 << 16;
        const int32_t inset[8] = { o, o, r - o, o, r - o, b - o, o, b - o };
        memcpy(uv, inset, sizeof inset);
        return 1;
    }
    }
    return 0;
}

void shine_pass_affine(const QuadTarget *q, ObjShape shape, const FshEntry *texture, const Object3DCorners *c, const EnvShine *env, const float up[3])
{
    int32_t packed[4], uv16[8], uv[8];
    const ShineClass *cls;
    if (env == NULL || texture->type != FSH_TYPE_BITMAP || (cls = shine_class_find(texture->payload)) == NULL) return;
    if (!shine_corner_uvs(c, up, packed) || !shine_shape_uv(shape, texture, c, uv16)) return;
    for (int i = 0; i < 2 * c->corners; i++) uv[i] = uv16[i] >> 16;
    shine_draw_env_affine(q, cls, env, c->screen, uv, packed, c->corners);
}
