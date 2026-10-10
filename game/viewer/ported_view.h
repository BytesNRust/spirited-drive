/* The track drawn by the ported renderer (game/race/track_render.h), for the
 * viewer, with the ported mappers and 3D engine (stand-ins in object_sink.c
 * where a mapper is still missing). */
#ifndef TNFS_PORTED_VIEW_H
#define TNFS_PORTED_VIEW_H

#include "formats/tri.h"
#include "game/race/track_art.h"
#include "game/race/car_draw.h"
#include "game/race/track_info.h"

#include "viewer/model_view.h"

#include <stdint.h>

enum { PORTED_VIEW_MODELS = 64 };

/* The FAM 3D objects, drawn through the renderer's model callback. */
void ported_view_set_models(const ViewModel *list, const int *ok);

/* A parked car (a CFM's two models) `ahead` nodes in front of the camera;
 * NULL removes it. */
void ported_view_set_car(const ViewModel *high, const ViewModel *low, int ahead);

/* A camera in the world: position 16.16, 24-bit angles, the road node it
 * draws the track from. */
typedef struct {
    int32_t position[3];
    int32_t yaw, pitch;
    int32_t node;
} PortedCamera;

/* The track from `cam`, with the car (set by ported_view_set_car) at `pose`
 * drawn with the row of `car_node`; car_node -1 leaves it out. */
void ported_view_render_from(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *cam,
                             const CarPose *pose, int32_t car_node, uint8_t *pixels);

/* yaw: extra turn, 24-bit angle. pixels: 640 x 480. */
void ported_view_render(const Tri *tri, const TrackArt *art, const TrackInfo *info, uint32_t node, int32_t yaw,
                        uint8_t *pixels);

#endif
