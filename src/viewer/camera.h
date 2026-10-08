/* Viewer camera: world points (metres, x right, y up, z forward) to the
 * rasteriser's camera space. */
#ifndef TNFS_VIEWER_CAMERA_H
#define TNFS_VIEWER_CAMERA_H

#include "viewer/raster.h"

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    Vec3 eye, forward, right, up;
} Camera;

static inline RasterVertex camera_project(const Camera *c, Vec3 p, float u, float v)
{
    const Vec3 d = { p.x - c->eye.x, p.y - c->eye.y, p.z - c->eye.z };
    return (RasterVertex){ d.x * c->right.x + d.y * c->right.y + d.z * c->right.z,
                           d.x * c->up.x + d.y * c->up.y + d.z * c->up.z,
                           d.x * c->forward.x + d.y * c->forward.y + d.z * c->forward.z, u, v };
}

#endif
