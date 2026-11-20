/* port (body shine, body_shine 4): the envmap pass around a car's body polygon as the race runs it, for any sink that draws the classic 3D (the race's
 * object sink, the Studio's 3D view): the tables for a palette, the world's up in the camera's space, each corner's sphere-map uv, the uv a mapper
 * shape reads, and the pass over a textured polygon the mapper drew (shine_draw.h). */
#ifndef TNFS_SHINE_PASS_H
#define TNFS_SHINE_PASS_H

#include "formats/fsh.h"
#include "raster/car_shine.h"
#include "raster/object3d.h"
#include "raster/object_mappers.h"
#include "raster/shine_draw.h"

#include <stdint.h>

/* the tables of an EnvShine for one palette */
typedef struct {
    uint8_t paint[CAR_SHINE_LEVELS][256], glass[CAR_SHINE_LEVELS][256];
    uint8_t paint_hard[CAR_SHINE_LEVELS][256], glass_hard[CAR_SHINE_LEVELS][256];
} ShineTables;

/* car_shine_tables and car_envmap_hard of each for `palette`, picking only its `usable` entries and keeping its `emissive` ones */
void shine_tables_make(const uint8_t palette[768], const uint8_t usable[256], const uint8_t emissive[256], ShineTables *t);
/* the pass's tables and image (CAR_ENVMAP_SIZE squared bytes); both kept by pointer */
EnvShine shine_tables_env(const ShineTables *t, const uint8_t *image);

/* the world's up (0, 1, 0) taken to the camera's space by `camera_matrix` and made unit (the camera's own y when it has no length): the horizon
 * the envmap's sphere map is looked up against */
void shine_world_up(const int32_t camera_matrix[9], float out[3]);

/* each corner's sphere-map uv of the envmap (car_envmap.h) from its normal and the line of sight, packed; 0 when the polygon carries no normals */
int shine_corner_uvs(const Object3DCorners *c, const float up[3], int32_t packed[4]);

/* the uv a mapper shape reads, 16.16 for each corner, whatever the shape takes them in (the triangle, quad and split-quad ones come from the mesh
 * in whole texels, the whole-texture one from the texture's size, the inset one from its size less a texel each side) */
int shine_shape_uv(ObjShape shape, const FshEntry *t, const Object3DCorners *c, int32_t uv[8]);

/* the exe's path: the envmap's pass over the polygon and uvs the mapper drew, for a texture shine_class.h knows (the others are left alone) */
void shine_pass_affine(const QuadTarget *q, ObjShape shape, const FshEntry *texture, const Object3DCorners *c, const EnvShine *env, const float up[3]);

#endif
