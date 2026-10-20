/* Viewer drawing of 3D objects (ORIP mesh + its SHPI textures), as the
 * track's FAM objects child holds them (004576f0), for `--track`'s stand-in view only; the race uses the
 * ported 3D engine (0048d798 object3d.c, 00458b7c model_draw.c). */
#ifndef TNFS_MODEL_VIEW_H
#define TNFS_MODEL_VIEW_H

#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/mesh.h"
#include "game/race/track_geometry.h"
#include "viewer/camera.h"

#include <stdint.h>

typedef struct {
    Mesh mesh;
    Fsh textures;
    int has_textures;
} ViewModel;

/* pair: a "wwww" of { ORIP, SHPI }. */
int model_load(ViewModel *model, const Chunk *pair);

/* Model units are 1/16 m (world 16.16 >> 12, 00458c04); heading is the
 * 24-bit angle of track_objects.h. */
void model_draw(const ViewModel *model, const RasterTarget *target, const Camera *cam, TrackPoint origin,
                uint32_t heading);

#endif
