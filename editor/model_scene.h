/* The car of the Model window, drawn by the core's rasterizer. A C face of raster/car_scene.h for the C++ panel: that header brings the game's View, which the
 * editor's own View (view.h) would clash with in one file. */
#ifndef TNFS_TRACK_EDITOR_MODEL_SCENE_H
#define TNFS_TRACK_EDITOR_MODEL_SCENE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "raster/orbit.h"

typedef struct ModelScene ModelScene;

/* The CFM at `path` read and opened; NULL with `why` when it cannot be. */
ModelScene *model_scene_open(const char *path, char *why, size_t why_cap);
void model_scene_close(ModelScene *m);
int32_t model_scene_radius(const ModelScene *m);
int model_scene_max_rows(void);
/* The car as `o` sees it, w x h RGBA (a clear background has alpha 0). 0 when the size is not one the rasterizer takes. */
int model_scene_render(ModelScene *m, const Orbit *o, int w, int h, uint8_t *rgba);

#ifdef __cplusplus
}
#endif

#endif
