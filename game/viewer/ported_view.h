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
#include "game/race/psx_lgs.h"
#include "game/race/psx_lamps.h"
#include "game/race/psx_light.h"

#include "viewer/model_view.h"

#include <stdint.h>

enum { PORTED_VIEW_MODELS = 64, PORTED_VIEW_CARS = 9 };

/* The FAM 3D objects, drawn through the renderer's model callback. */
void ported_view_set_models(const ViewModel *list, const int *ok, int mirrored, int two_sided, int close_houses);  /* mirrored: race->mirrored; two_sided: port_two_sided_scenery's */

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
/* port: the PS1 look's light (psx_light.h) and the ambient colour's tint (per channel, 1 neutral; copied); NULL, none */
void ported_view_set_light(const PsxLight *light, const float tint[3]);
/* port (the light editor): the light's rows were changed in place; the built track is built again with them (the next frame) */
void ported_view_light_changed(void);

/* port (the PS1 look's light): the track's tunnel lamps (psx_lamps.h), `count` of them (copied; NULL or 0 none): the colours of each
 * lamp's fitting in its texture are not darkened by the light. Needs ported_view_set_light's light. */
/* port (night, psx_headlights): the street lamp's board (TrackArt.scenery[scenery]) with its three entries the light tables leave alone; NULL entry: none. */
void ported_view_set_street_lamps(const int scenery[], int n, const uint8_t entry[3]);   /* the lamp boards (at most 3) whose entries the tables leave alone; NULL / 0 none */

/* port (night; window_lights.h): the scenery boards whose lit windows' `entry` the light tables leave alone (at most WINDOW_BOARDS_MAX); none with n 0 */
enum { WINDOW_BOARDS_MAX = 12 };
void ported_view_set_window_boards(const int scenery[], int n, uint8_t entry);

/* port (reflectors, reflect_pass.h): where the track's masks come from (track_mask.h's track_mask_for: a texture's w x h bytes, 1 reflective, or NULL); NULL: none, and nothing shines. The default is track_mask_for (a test or a tool sets its own) */
void ported_view_set_reflect_masks(const uint8_t *(*mask_for)(const TrackTexture *t));
void ported_view_set_psx_lamps(const PsxLamp *lamps, int count);

/* port (port.cfg psx_shadow): 1, the cars' shadow is the PS1 look's soft one (soft_shadow.h), weaker where the light is dark; 0, the exe's quad. */
void ported_view_set_soft_shadow(int on);
/* port (psx_flares): a soft halo round a lit brake light in low light and in fog (lamp_flare.h); 0 the exe's */
void ported_view_set_flares(int on);

/* port: the PS1 look's ambient light on the cars (psx_lgs.h): each car is drawn through a colour table of its segment's ambient
 * colour over the world's `tint`, so a context's colour (a tunnel's, a bridge's) shows on the cars and the world's mean does not
 * double. NULL, none (the cars take the world's tint as before). `lgs` is copied. Needs ported_view_set_light's light. */
void ported_view_set_psx_ambient(const PsxLgs *lgs, uint32_t ring_nodes, const float tint[3]);
/* port: 1 when the light given to ported_view_set_light was made from the road (psx_light_synth.h) and has no script: the cars and the dash take its tunnel
 * lamp alternation (psx_light_synth_car) in place of a script's ratio. 0 (the default) otherwise. */
void ported_view_set_psx_synth(int on);
/* port (psx_light, police_lights.h): 1 for the police car's lights to flash red and blue while it chases; 0 (the default) the exe's. Set at the race's start. */
void ported_view_set_police_lights(int on);
/* The ratio the PS1 light scales `car`'s colours by at its place (psx_light_car_ratio), 1 and `out` set; 0 with no light script. The dash takes the player's. */
int ported_view_car_ratio(const Car *car, float out[3]);

struct PsxSkyDraw;
/* port: the PS1 sky (psx_sky_draw.h) drawn in place of the track's sky and ring, NULL for the track's own. Not owned. */
void ported_view_set_psx_sky(struct PsxSkyDraw *sky);
/* port (weather.h): the race's weather, WEATHER_CLEAR (the default) draws as it always did; fog needs Modern 3D on */
void ported_view_set_weather(int weather);
/* port (wipers): the blades of the dash view's wipers in the 3D buffer's pixels (WiperBlade, wiper.h), set each frame before the draw: none (count 0) when the view is not the dash, the setting is off or the wide path is not on */
struct WiperBlade;
void ported_view_set_wipers(const struct WiperBlade *blades, int count);
/* port (wipers): the dash layer's glass flags (640x480, 1 = glass; NULL for none) the blades are clipped to, and where the layer's origin and scale are in the 3D buffer's pixels, set with the blades */
void ported_view_set_wiper_glass(const uint8_t *glass, float x, float y, float scale);
/* port (wipers): 1 when this frame's wipers are on (blades set and rain or snow falling): the dash's drawn blades are then hidden (dash_mask.h BLADE) */
int ported_view_wipers_active(void);
void ported_view_set_snow(int snow);   /* port (snow_weather): the race's track is a snowy one with the switch on: the rain falls as snow (weather_snow.h) */
void ported_view_set_modern(int on);
/* port (weather): the mirror's road style for the weather (0: clear weather, nothing set) and its sky, drawn by then */
int ported_view_mirror_weather(const TrackArt *art);
/* 1 when the weather draws something in the mirror (not clear, Modern 3D on): the sky's recorder is only stopped and restarted then */
int ported_view_mirror_weather_on(void);
/* 1 when the weather is rain or storm (the lamps and the gauges follow it: rain_lamps, gauge_exterior) */
int ported_view_rain(void);
/* port (weather_zones): how much weather there is at `node`, 0..16 (0 inside a closed zone: the light's tunnels and Coastal's galleries; 16 in the open) */
int ported_view_weather_open(uint32_t node);
void ported_view_mirror_sky(const TrackArt *art, const QuadTarget *target, const View *view, const int32_t matrix[9]);
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
/* port (the PS1 light in the rear mirror's follow pass): begin sets the light hook from the tables the last render made and gives 1, or gives 0
 * (hook untouched) with no light; end clears it. `sinks` are the main pass's own lit draws for the mirror's target to use between the two. */
/* The PS1 look's sky (or the PC's alternative) as a horizon picture in the race palette's colours, for the draws without a smooth horizon; NULL without one */
const FshEntry *ported_view_sky_in_palette(void);
/* The INFO of the sky set with ported_view_set_psx_sky when it is the PC's alternative one (its ring); NULL for the PS1's own */
void ported_view_set_psx_sky_info(const TrackInfo *info);
void ported_view_set_dark_fog(int dark);   /* port (the PS1 look): Lost Vegas' fog dark (modern_look_set_dark) */
int ported_view_mirror_light_begin(void);
/* port: the PS1 light of a road point (the mirror's rows: segment x 4's node) and 1 in `tunnel` when its node is in one; 255 and 0 without a light */
uint8_t ported_view_light_at(uint32_t node, int point, uint8_t *tunnel);
void ported_view_mirror_light_end(void);
void ported_view_mirror_sinks(void (**uv)(void *, const FshEntry *, const int32_t[8], const int32_t[8]),
                              void (**full)(void *, const FshEntry *, const int32_t[8]),
                              void (**full_alt)(void *, const FshEntry *, const int32_t[8]),
                              void (**camera)(void *, const View *, const FshEntry *, const int32_t[8], const int32_t *const[4]));

void ported_view_render(const Tri *tri, const TrackArt *art, const TrackInfo *info, uint32_t node, int32_t yaw,
                        uint8_t *pixels);

#endif
