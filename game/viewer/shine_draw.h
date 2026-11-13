/* port (body shine): the second pass over a car's body polygon. It never writes a texel of its own: where the polygon's texture says "paint" (a
 * class map made at load, the draft's REPAINT_DRAFT_PAINT = 0) the pixel already on the screen goes through the table of the pixel's envmap
 * level (EnvShine, car_envmap.h), so the paint keeps its shading and the glass, lamps, wheels, plate and trim are not touched. Recorded for the
 * render bands like the other raster primitives. */
#ifndef TNFS_SHINE_DRAW_H
#define TNFS_SHINE_DRAW_H

#include "game/gfx/quad.h"
#include "game/race/car_shine.h"
#include "game/race/view.h"

#include <stdint.h>

typedef struct {
    const uint8_t *klass;   /* width x height bytes, 0 paint; kept by pointer (it lives as long as the car) */
    int width, height;
} ShineClass;

/* the tables for the paint texels and for the keep-and-shine ones (class 128: glass, lamps), and the envmap image (car_envmap.h). Kept by
 * pointer until the frame is flushed. */
typedef struct {
    const uint8_t (*paint)[256];
    const uint8_t (*glass)[256];
    const uint8_t *image;
    const uint8_t (*paint_hard)[256], (*glass_hard)[256];   /* car_envmap_hard of each table: a step not to dither (may be NULL: none) */
} EnvShine;

/* Modern 3D's: the view the camera path draws with, kept by pointer. */
void shine_draw_set_view(const View *view);

/* the exe's path: the polygon's integer screen corners (x, y) and its uvs in whole texels (as the mappers take them), 3 or 4 corners, affine.
 * `packed[i]` is corner i's sphere-map uv (car_envmap_pack); each pixel takes the image's value at its own interpolated uv, quantised to the
 * levels, and goes through the paint table or, for class 128, the glass table. */
void shine_draw_env_affine(const QuadTarget *q, const ShineClass *cls, const EnvShine *env, const int32_t screen[8], const int32_t uv[8],
                           const int32_t packed[4], int corners);

/* Modern 3D's: the camera-space corners (cam[i] points at x, y, z), uv 16.16, clipped at the view's near plane and perspective-correct, as
 * the affine one with the view's shine_draw_set_view. */
void shine_draw_env_camera(const QuadTarget *q, const ShineClass *cls, const EnvShine *env, const int32_t uv[8], const int32_t *const cam[4],
                           const int32_t packed[4], int corners);

#endif
