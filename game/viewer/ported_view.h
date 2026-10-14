/* The track drawn by the ported renderer (game/race/track_render.h), for the
 * viewer, with the ported mappers and 3D engine (stand-ins in object_sink.c
 * where a mapper is still missing). */
#ifndef TNFS_PORTED_VIEW_H
#define TNFS_PORTED_VIEW_H

#include "game/race/puffs.h"
#include "formats/tri.h"
#include "game/race/track_art.h"
#include "game/gfx/quad.h"
#include "game/race/car_draw.h"
#include "game/race/race_state.h"
#include "game/race/mirror_scene.h"
#include "game/race/track_info.h"

#include "viewer/model_view.h"

#include <stdint.h>

enum { PORTED_VIEW_MODELS = 64, PORTED_VIEW_CARS = 9 };

/* The FAM 3D objects, drawn through the renderer's model callback. */
void ported_view_set_models(const ViewModel *list, const int *ok);

/* Where the 3D view sits on the 640 x 480 frame (a race camera's 004b7080
 * row: left + x, top + y, width, height; its horizon scale; the sky ring's
 * rectangle +0x18..+0x24) and the camera's fov. The default is the whole
 * frame at fov 96. */
typedef struct {
    int32_t left, top, width, height, fov, scale;
    int32_t sky_left, sky_top, sky_width, sky_height;
} PortedViewport;

void ported_view_set_viewport(const PortedViewport *viewport);

/* 004591d4 for a car slot's model at `pose` into `target` (the mirror's pass with mirror 1). 0 when not drawn. */
int ported_view_draw_car(int slot, const CarPose *pose, const ModelView *mv, int mirror, const QuadTarget *target,
                         const uint8_t palette[768]);

/* 00512040..00512048 as the last frame's track transform left them (004497d8). */
void ported_view_near_dir(int32_t out[3]);

/* A parked car (a CFM's two models) `ahead` nodes in front of the camera;
 * NULL removes it. */
void ported_view_set_car(const ViewModel *high, const ViewModel *low, int ahead);

/* The car drawn in `slot` (0 .. PORTED_VIEW_CARS - 1) by
 * ported_view_render_cars; NULL removes it. */
void ported_view_set_car_slot(int slot, const ViewModel *high, const ViewModel *low);

/* The car drawn in `slot` from objects made elsewhere (the car loader's,
 * 00457d40), borrowed: they must outlive the slot; NULL removes it. */
void ported_view_set_car_objects(int slot, Object3D *high, Object3D *low);

/* The high detail object of the car in `slot` (its texture refs can be
 * rebound, the wheel frames), or NULL. */
Object3D *ported_view_car_object(int slot);

/* A camera in the world: position 16.16, 24-bit angles, the road node it
 * draws the track from. */
typedef struct {
    int32_t position[3];
    int32_t yaw, pitch;
    int32_t node;
    int32_t roll;
} PortedCamera;

/* The track from `cam`, with the car (set by ported_view_set_car) at `pose`
 * drawn with the row of `car_node`; car_node -1 leaves it out. */
void ported_view_render_from(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *cam,
                             const CarPose *pose, int32_t car_node, uint8_t *pixels);

/* 0043f540 on a frame that is not drawn (the headless ticks): the segment
 * cache's window and the point list follow the camera as in the race loop,
 * which runs it every frame. */
void ported_view_stream(const Tri *tri, const TrackArt *art, int32_t camera_node);

/* A car's pose and the road node whose row draws it; node -1 leaves it out. */
typedef struct {
    CarPose pose;
    int32_t node;
    Car *car;                   /* the race's car, for its shadow (NULL: none) */
} PortedCar;

/* The cars' shadows (0045a724's 0045a3e8 pass 0 and 00482794): the race whose road planes they take and the remap
 * they darken with (004c6604); NULL draws none. */
void ported_view_set_shadows(const RaceState *race, const uint8_t *remap);

/* The tyre smoke of the followed car (00458d4c's 00458988) and its crash
 * smoke (004591d4's 00456f34 on `trail`): the shapes and blend, and the
 * draws of race_rand the tyres take (the caller counts them once a tick);
 * art NULL draws none. */
/* The marks over the other racers (00459064): their fill colours by slot
 * (0050b7d4, `count` of them) and the outline (0050b7b4); NULL draws none. */
void ported_view_set_tags(const uint8_t *colours, int count, uint32_t outline);

/* The cars' lights in the palette (00458d4c's 00476460 calls): the write,
 * and the slots' own entries (0051c798); write NULL leaves them. */
void ported_view_set_lights(void (*write)(void *ctx, int32_t first, int32_t count, const uint8_t *colours), void *ctx,
                            const uint8_t *slot_colours);

void ported_view_set_puffs(const PuffArt *art, const PuffBlend *blend, int32_t (*rand_below)(void *ctx, int32_t n),
                           void *ctx, CrashTrail *trail);

/* 0044ce32 / 0044ce3f, before the mirror and the 3D view: the rows holding
 * skid marks (00459c50) and cars (00459d70: those ahead kept here, those
 * behind into `behind`, 0051c7a1); nothing without ported_view_set_shadows'
 * race. */
void ported_view_frame_rows(uint8_t behind[MIRROR_CAR_ROWS]);

/* The track from `cam` with `count` cars, list[i] drawn with slot i's model. */
void ported_view_render_cars(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *cam,
                             const PortedCar *list, int count, uint8_t *pixels);

/* yaw: extra turn, 24-bit angle. pixels: 640 x 480. */
void ported_view_render(const Tri *tri, const TrackArt *art, const TrackInfo *info, uint32_t node, int32_t yaw,
                        uint8_t *pixels);

#endif
