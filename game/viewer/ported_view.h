/* The track drawn by the ported renderer (game/race/track_render.h), for the
 * viewer, with the ported mappers and 3D engine (stand-ins in object_sink.c
 * where a mapper is still missing). */
#ifndef TNFS_PORTED_VIEW_H
#define TNFS_PORTED_VIEW_H

#include "game/race/skid_marks.h"
#include "game/race/puffs.h"
#include "formats/tri.h"
#include "game/race/track_art.h"
#include "game/gfx/quad.h"
#include "game/race/car_draw.h"
#include "game/race/race_state.h"
#include "game/race/mirror_scene.h"
#include "game/race/track_info.h"
#include "game/race/track_render.h"

#include "viewer/model_view.h"

#include <stdint.h>

enum { PORTED_VIEW_MODELS = 64, PORTED_VIEW_CARS = 9 };

/* The FAM 3D objects, drawn through the renderer's model callback. */
void ported_view_set_models(const ViewModel *list, const int *ok, int mirrored);  /* mirrored: race->mirrored */

/* Where the 3D view sits on the 640 x 480 frame (a race camera's 004b7080
 * row: left + x, top + y, width, height for the projection; its horizon
 * scale; the box +0x18..+0x24 the view is clipped to and the sky ring
 * fills) and the camera's fov. The default is the whole
 * frame at fov 96. */
typedef struct {
    int32_t left, top, width, height, fov, scale;
    int32_t sky_left, sky_top, sky_width, sky_height;
    int32_t fov_y;              /* port: the vertical field when it differs (a wide screen), 0 the same as fov */
    int32_t near_scale;         /* port: the road's near plane, 16.16 (screen_near_scale); 0 the exe's */
    int32_t render_scale;       /* port (render scale): the positions and sizes above are already this many times the frame's; 1 */
} PortedViewport;

/* port (render scale, re/notes/render-scale-design.md): `v` for a frame `s` times as wide and tall. The box, the
 * projection's place and its size grow by s; the fields of view stay, so the focal lengths (view_init takes them from the
 * size) grow by s and the picture is the same with s times the pixels. */
PortedViewport ported_view_scaled(const PortedViewport *v, int s);

/* port: the segments of track drawn (track_render_set_reach): DRAW_SEGMENTS_ULTRA for port.cfg's view_ultra */
void ported_view_set_reach(int32_t segments);
int32_t ported_view_draw_segments(void);   /* the segments the frames draw now (track_render's draw_segments) */

/* port: EXTRAS' Modern 3D: the near road clipped against the near plane, projected without pixel snapping and
 * textured perspective-correct (modern_road.h) instead of 00425718's subdivision into the affine mapper; 0 the exe's */
void ported_view_set_modern(int on);
int ported_view_modern(void);
/* port: EXTRAS' Texture Filter and Fog over Modern 3D (modern_look.h); nothing while Modern 3D is off */
void ported_view_set_look(int filter, int fog);
/* the Texture Filter and Fog as the frames draw them: 0 without Modern 3D, whatever the switches say */
int ported_view_filter_on(void);
int ported_view_fog_on(void);

/* port: the frame the 3D view draws into (640 x 480 unless a wide screen's race sets its width) */
void ported_view_set_screen(int width, int height);

void ported_view_set_viewport(const PortedViewport *viewport);

/* 004fb786 (F4, 004381f8): the sky off, flat colours in place of the horizon's bitmap or ring. */
void ported_view_set_sky_off(int off);
/* 004fb78b (F6, 004381f8): no marks over the other racers (00459064) */
void ported_view_set_tags_hidden(int hidden);
/* 00513e70, the race's tick: 0044a4b0's animated boards step on it (tick / +9 % +8); 0 still */
void ported_view_set_tick(int32_t tick);

/* 004591d4 on the race's `car` (its slot's model) with the view `mv`: mirror 1 for the rear view mirror's pass.
 * 0 when the car has no model here. */
int ported_view_render_car(Car *car, const ModelView *mv, int mirror, const QuadTarget *target, const uint8_t palette[768]);

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

/* 0043f540's cache and list part: the segment cache's window and the point
 * list follow the camera as in the race loop, which runs it every frame (the
 * headless ticks too). Returns the renderer whose cache (005041d8) and
 * segment mask (00504210) the mirror reads. */
const TrackRender *ported_view_stream(const Tri *tri, const TrackArt *art, int32_t camera_node);
/* The track freed: the next stream binds its successor afresh, and its
 * models too (a stage's next segment loads into the same Tri and the same
 * model list, so the pointer alone does not tell). */
void ported_view_forget_track(void);

/* A car's pose and the road node whose row draws it; node -1 leaves it out. */
typedef struct {
    CarPose pose;
    int32_t node;
    Car *car;                   /* the race's car, for its shadow (NULL: none) */
} PortedCar;

/* The cars' shadows (0045a724's 0045a3e8 pass 0 and 00482794): the race whose road planes they take and the remap
 * they darken with (004c6604), which the objects' shaded polygons take too; NULL draws none. The race's scenery
 * state too: the objects it knocked over lie flat, streamed again at the knock (0043f540). */
void ported_view_set_shadows(RaceState *race, const uint8_t *remap);

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

/* 00458d4c's 00458614 and 00458818 for the followed car as it is drawn. */
void ported_view_set_wheels(void (*frames)(void *ctx, Car *car), void (*steer)(void *ctx, Car *car), void *ctx);

void ported_view_set_puffs(const PuffArt *art, const PuffBlend *blend, int32_t (*rand_below)(void *ctx, int32_t n),
                           void *ctx, CrashTrail *trail);

/* 0044ce32 / 0044ce3f, before the mirror and the 3D view: the rows holding
 * skid marks (00459c50) and cars (00459d70: those ahead kept here, those
 * behind into `behind`, 0051c7a1); nothing without ported_view_set_shadows'
 * race. */
/* The followed car's skid marks (00517620), for 00457594's reset at the race's start. */
SkidMarks *ported_view_skids(void);

void ported_view_frame_rows(uint8_t behind[MIRROR_CAR_ROWS]);
void ported_view_mirror_rows_again(uint8_t behind[MIRROR_CAR_ROWS]);   /* port: mirror_car_rows_at for this frame */

/* The track from `cam` with `count` cars, list[i] drawn with slot i's model. */
void ported_view_render_cars(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *cam,
                             const PortedCar *list, int count, uint8_t *pixels);

/* yaw: extra turn, 24-bit angle. pixels: 640 x 480. */
void ported_view_render(const Tri *tri, const TrackArt *art, const TrackInfo *info, uint32_t node, int32_t yaw,
                        uint8_t *pixels);

#endif
