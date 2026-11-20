/* The car of the Model window, drawn by the core's rasterizer. A C face of raster/car_scene.h for the C++ panel: that header brings the game's View, which the
 * editor's own View (view.h) would clash with in one file. */
#ifndef TNFS_TRACK_EDITOR_MODEL_SCENE_H
#define TNFS_TRACK_EDITOR_MODEL_SCENE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "formats/car_night.h"
#include "raster/orbit.h"
#include "car_preview.h"

typedef struct ModelScene ModelScene;

/* The CFM at `path` read and opened; NULL with `why` when it cannot be. */
ModelScene *model_scene_open(const char *path, char *why, size_t why_cap);
/* The car file as the race has it after the game's own black and paint (car_preview_painted_cfm), with the Preview window's own textures, masks and palette
 * (car_preview_make) for the night view; NULL with `why` when it cannot be made. */
ModelScene *model_scene_open_painted(const PreviewIn *in, char *why, size_t why_cap);
void model_scene_close(ModelScene *m);
int32_t model_scene_radius(const ModelScene *m);
int model_scene_max_rows(void);
/* The car as `o` sees it, w x h RGBA (a clear background has alpha 0), in the light and lamps of `night` (formats/car_night.h, as the Preview window shows
 * its textures; NULL: by day, as the file has it): the palette tinted and lit, the lamp texels the masks name in their fixed colours, the police lights'
 * flash. The colours are the race's (6 bits a channel). 0 when the size is not one the rasterizer takes. */
int model_scene_render(ModelScene *m, const Orbit *o, int w, int h, const CarNightState *night, uint8_t *rgba);
/* the Preview's textures the scene was made with (NULL for a scene of the file as it is) */
const CarPreview *model_scene_preview(const ModelScene *m);
/* the texels of the high bank's texture `name` (4 letters) as the scene draws them, w x h; NULL when it has none */
const uint8_t *model_scene_texels(const ModelScene *m, const char *name, int *w, int *h);
/* how many pixels of the last picture show a lit lamp: 0 by day or without masks */
int model_scene_lamp_pixels(const ModelScene *m);

#ifdef __cplusplus
}
#endif

#endif
