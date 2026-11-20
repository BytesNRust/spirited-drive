/* A car of a CFM drawn the way the race draws it, for the Studio's 3D view (dep59 car editor, step 2): the high bank's mesh and shapes opened,
 * its sections (inside, surface, outside, convex) kept, the wheels at rest, and a render into an 8-bit surface through the classic rasterizer
 * (object3d, quad, flat), turned and placed by an Orbit. The pixels are palette indices; car_scene_rgba turns them into colours with the
 * bank's own palette. No game code: this is core. */
#ifndef TNFS_CAR_SCENE_H
#define TNFS_CAR_SCENE_H

#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/mesh.h"
#include "raster/object3d.h"
#include "raster/orbit.h"

#include <stddef.h>
#include <stdint.h>

enum { CAR_SCENE_SECTIONS = 4, CAR_SCENE_CLEAR = 0xFF };

typedef struct {
    Chunk file;
    Mesh mesh;
    Fsh bank;
    int has_bank;
    Object3D obj;
    Object3DList sections[CAR_SCENE_SECTIONS];   /* inside, surface, outside, convex: the order the race draws them */
    uint8_t palette[768];                        /* the bank's '!PAL', as 8-bit RGB */
    uint8_t quad[256 * 4];                       /* the same as the colour matcher reads it: B, G, R, 0 */
    uint8_t enabled[256];
    int32_t radius;                              /* the farthest vertex of the body from the model's origin */
} CarScene;

/* Opens the CFM `cfm` (kept: it must outlive the scene). 0 when it is not a car (nothing is left open). */
int car_scene_open(CarScene *s, const uint8_t *cfm, size_t size);
void car_scene_close(CarScene *s);

/* The car as `o` sees it into `pixels`, w x h bytes, which the caller has cleared (CAR_SCENE_CLEAR is what car_scene_rgba leaves transparent). */
void car_scene_render(CarScene *s, const Orbit *o, uint8_t *pixels, int w, int h);

/* `n` indices as RGBA; the clear index is transparent */
void car_scene_rgba(const CarScene *s, const uint8_t *pixels, size_t n, uint8_t *rgba);

#endif
