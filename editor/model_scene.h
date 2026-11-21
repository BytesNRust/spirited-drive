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
/* The wheels as the race frame `frame` has them: 0 still, 1 and 2 the slow pair, 3 the fast blur (car_scene_wheels); a scene made again starts at 0. 1 when set. */
int model_scene_wheels(ModelScene *m, int frame);
/* how many pixels of the last picture show a lit lamp: 0 by day or without masks */
int model_scene_lamp_pixels(const ModelScene *m);
/* The body shine as the race draws it (raster/shine_pass.h): on or off, its envmap the PNG at `envmap` (a kit's car_envmap.png, as a mod's replaces the
 * game's) or the game's procedural one when it is NULL or cannot be read. The tables are the race's (car_shine_tables) on the palette the picture is
 * shown in (Day / Night): the car's own, where the race has the screen's. The masks' draft is read the first time it is on, from `in` (the one the
 * scene was opened with): a car looked at with the shine off never reads it. Returns 1 when the car shines (a painted scene whose masks mark body texels). */
int model_scene_shine(ModelScene *m, int on, const PreviewIn *in, const char *envmap);
/* the high bank's textures the shine knows (a class map each), 0 for none (or none drafted yet) */
int model_scene_shine_textures(const ModelScene *m);

#ifdef __cplusplus
}
#endif

#endif
