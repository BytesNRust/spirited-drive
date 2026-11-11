#include "game/sys/bands.h"
#include "game/race/light_tint.h"
#include "game/gfx/raster_record.h"

#include "game/race/sky_stretch.h"
#include "game/viewer/reflect_pass.h"
#include "game/viewer/street_halo.h"
#include "game/viewer/view_lamps.h"
#include "game/viewer/view_screen.h"
#include "game/viewer/weather_hooks.h"
#include "game/viewer/screen_layout.h"
#include "game/race/weather_snow.h"
#include "game/viewer/follow_behind.h"
#include "viewer/sky_art_draw.h"
#include "game/sys/port_log.h"
#include "viewer/weather_draw.h"
#include "viewer/wiper_draw.h"
#include "viewer/drive_light.h"
#include "viewer/view_spray.h"
#include "viewer/view_cars.h"
#include "viewer/view_light.h"
#include "viewer/view_tables.h"
#include "viewer/view_sky.h"
#include "viewer/view_frame.h"
#include "viewer/view_models.h"
#include "viewer/view_weather.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>


/* The race's default mapper set (00449b90): every wrapper passes textures
 * of type 0x45 to the transparent mapper and the rest to the opaque one. */
int transparent(const FshEntry *texture)
{
    return texture->type == TRACK_TEXTURE_TRANSPARENT_TYPE;
}



/* 0051c7ac: the rows ahead of the camera holding a car (00459d70). */
uint8_t ahead_rows[ROW_CARS_AHEAD];
/* port (port_follow_camera): the rows behind the camera's node holding a car, for the arm of road behind it (row_cars.h), and whether the arm is drawing */
uint8_t behind_rows[ROW_CARS_BEHIND];
int arm_drawing;

void ported_view_frame_rows(uint8_t behind[MIRROR_CAR_ROWS])
{
    if (shadow_race == NULL) return;
    const SkidRing ring = skid_ring();
    skid_rows(&skid_marks, &ring, shadow_race->chase.node);
    car_rows_ahead(shadow_race->cars, shadow_race->car_count, shadow_race->segments, shadow_race->chase.node, ahead_rows,
                   behind);
    if (shadow_race->port_follow_camera)
        car_rows_behind(shadow_race->cars, shadow_race->car_count, shadow_race->segments, shadow_race->chase.node, behind_rows);
}

/* port: the mirror's rows again for the camera's node of this frame, the cars not moved on (mirror_car_rows_at) */
void ported_view_mirror_rows_again(uint8_t behind[MIRROR_CAR_ROWS])
{
    if (shadow_race == NULL) return;
    mirror_car_rows_at(shadow_race->cars, shadow_race->car_count, shadow_race->segments, shadow_race->chase.node, behind);
}

const Car *followed_car(void)
{
    return shadow_race->cars[race_slot(shadow_race->player)];
}



int32_t anim_tick;  /* 00513e70 */

void ported_view_set_tick(int32_t tick)
{
    anim_tick = tick;
}

/* 0043af80: the 3D sky ring of the tracks whose INFO asks for one. */
/* 0043af80's ring path: 0043b298 with the camera's yaw and roll (the follow
 * record's +0x44, +0x48); 0043b0c8 slopes the fills with the roll. */
/* 00512040..: the road direction the last frame's transform left (the mirror draws with it). */
int32_t last_near_dir[3] = { 0, 0, 0x1999 };   /* the main view's near plane (set by the render, used by render_point and the dust) */
int32_t last_near = 62259 >> WORLD_SHIFT;  /* *(00511fe8) +4 as the last frame's transform left it */

void ported_view_near_dir(int32_t out[3])
{
    memcpy(out, last_near_dir, sizeof last_near_dir);
}


/* The track from `cam`, with the car (set by ported_view_set_car) at `pose` drawn with the row of `car_node`; car_node -1 leaves it out. */
static void ported_view_render_from(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *pc,
                             const CarPose *pose, int32_t pose_node, uint8_t *pixels)
{
    const PortedCar one = { pose != NULL ? *pose : (CarPose){ { 0, 0, 0 }, 0, 0, 0, NULL, 1 },
                            pose != NULL ? pose_node : -1, NULL };
    ported_view_render_cars(tri, art, info, pc, &one, 1, pixels);
}

TrackRender render;

/* 0043f540 on the race's scenery: the knocked-over bytes are the race's (004b9098, 0040fc64 sets them), and a knock's
 * 0043f51c (00504218 = -1) streams the scenery again at once; 0043f48c's 00504218 goes back to the race. */
void stream(const Tri *tri, const TrackArt *art, int32_t camera_node)
{
    if (shadow_race != NULL && render.tri == tri) {
        render.build.knocked = shadow_race->scenery_state;
        if (shadow_race->scenery_stream_node < 0) render.build.first_node = -1;
    }
    track_render_set_light(&render, light_effects);  /* before the build: it fills the light with the vertices */
    const int relit = light_changed;
    if (relit) {   /* the light editor's reload: the segments already built hold the old rows */
        light_changed = 0;
        render.build.first_node = -1;
    }
    track_render_set_ring(&render, shadow_race != NULL && shadow_race->port_follow_ring ? TRACK_CACHE_RECORDS_MAX : TRACK_CACHE_RECORDS);   /* port (port_follow_ring) */
    if (shadow_race != NULL && shadow_race->port_follow_camera) follow_arm_window(&render, follow_arm_segments());   /* the window's reach behind before it is filled */
    track_render_build(&render, tri, art, camera_node);
    if (relit && light_effects != NULL) {   /* the editor's proof: the sum of the built vertices' light */
        uint64_t sum = 0;
        const int32_t n = render.build.world_next < TRACK_MAX_VERTICES ? render.build.world_next : TRACK_MAX_VERTICES;
        for (int32_t i = 0; i < n; i++) sum += render.light_vertex[i];
        port_log("light edit: the track built again with the new light (%d vertices, light sum %llu)\n", (int)n, (unsigned long long)sum);
    }
    if (shadow_race != NULL) {
        render.build.knocked = shadow_race->scenery_state;  /* again, for a first build that bound the track */
        shadow_race->scenery_stream_node = render.build.first_node;
    }
}

void ported_view_forget_track(void)
{
    render.tri = NULL;
    reflect_pass_forget_art();   /* port (reflectors): the next track's art may stand at this one's address; its textures are not these */
    ported_view_set_models(NULL, NULL, 0, 0, 0);  /* the next track's models sit at the same address, bound to these textures */
    horizon_smooth_cache_reset();           /* port: the sky bands kept for this track's bitmap */
    road_tunnels_forget();
    if (pool_fed != NULL) pool_fed->tunnel = NULL, pool_fed->node_xz = NULL;   /* the next track's first build asks the pool before headlights_update feeds it again */
}

void ported_view_set_reach(int32_t segments)
{
    track_render_set_reach(&render, segments);
}

int32_t ported_view_draw_segments(void) { return render.draw_segments; }

const TrackRender *ported_view_stream(const Tri *tri, const TrackArt *art, int32_t camera_node)
{
    stream(tri, art, camera_node);
    return &render;
}

/* port (port_follow_camera): which way the camera looks along the track decides who draws it. The exe's track pass draws
 * the segments ahead of the camera's node (its near plane pushes along the road direction, which runs away from a
 * camera that faces down the track); a camera that looks more across the road is drawn by follow_behind.h's, which
 * draws both sides of the node, the segments' rails, scenery and cars as the exe's pass does. */
enum { FOLLOW_EXE_BELOW = 0x120000 };   /* a 24-bit angle: 25.3 degrees off the road, past which the bottom of the view looks at ground behind the camera's node, which the exe's pass leaves bare */

static int32_t follow_off_road(const PortedCamera *pc)
{
    int32_t off = (int32_t)(((uint32_t)pc->yaw - (uint32_t)race_node(shadow_race, pc->node).orientation * 0x400u) & 0xffffff);
    if (off > 0x800000) off = 0x1000000 - off;
    return off;
}

/* port: the followed car on a row behind the camera's node, which the track pass does not draw cars on */
static void draw_followed_car(const PortedCamera *pc, int ahead_drawn)
{
    const RaceState *race = shadow_race;
    Car *car = (Car *)followed_car();
    if (car == NULL || race->chase.kind == 0 || car->physics == 6) return;
    const int32_t ring = (int32_t)((uint32_t)race->segments << 2);
    const int32_t d = car->draw_node >= pc->node ? car->draw_node - pc->node : car->draw_node + ring - pc->node;
    if (ahead_drawn && d >= 0 && d < ROW_CARS_AHEAD && ahead_rows[d] != 0) return;   /* the pass drew it */
    row_prepare(NULL, car, 0);
    row_draw(NULL, car, 0);
}

/* port (weather): the rain over the drawn view, under the 2D layer: the speed-following streaks and the lens at night (rain_fx), else the plain streaks */
static void rain_over_view(uint8_t *pixels, const int32_t box[4], const View *view, int32_t rs, const WeatherTables *wt)
{
    WiperPass wiper_now;
    const WeatherFrame frame = weather_frame();
    const WiperPass *const wipers = wt != NULL ? weather_hooks_wiper_pass(&wiper_now, box, &frame) : NULL;   /* port (wipers) */
    if (wt != NULL && weather_wet(weather) && port_rain_fx()) {   /* port (rain_fx): streaks that follow the speed, lit by the headlights at night */
        const int32_t speed = shadow_race != NULL && followed_car() != NULL ? followed_car()->speed : 0;
        const char *forced = getenv("TNFS_RAIN_SPEED");   /* dev: the speed the streaks are drawn for (metres a second), for captures of a car the harness has not got up to speed */
        const int32_t shown = forced != NULL ? (int32_t)(atoi(forced)) << 16 : speed;
        const int mood_share = (drive_light_night() ? RAIN_MOOD_SHARE_NIGHT : RAIN_MOOD_SHARE) * rain_open / 16;   /* port (tunnel_dry): the grey goes with the weather: none inside a closed zone */
        if (!rain_wet_off() && mood_share > 0 && weather_fog_snow_mood(snow_level(), port_weather_fog())) {   /* port (rain v4): the grey, low-contrast scene of rain; rows are independent, so in bands */
            const RainMood mood = { pixels, view_screen_w, { box[0], box[1], box[2], box[3] }, wt->mood, mood_share };
            bands_rows(box[1], box[3], rain_mood_band, (void *)&mood);
        }
        rain_cars_paint(pixels, view_screen_w, box, wt);
        int32_t vanish[2] = { view->centre_x, view->centre_y };
        rain_vanishing_point(vanish);
        if (port_rain_v2()) vanish[1] -= (box[3] - box[1]) / RAIN_V2_RAISE_DIV;   /* port (rain_v2): the rain falls from above: the streaks leave a point higher up */
        if (snow_level() != SNOW_NONE && port_snow_fx())   /* port (snow_fx): flakes that lean with the speed, a swinging storm wind, white marks on the lens */
            weather_snow_draw_fx(pixels, view_screen_w, box, rs, (uint32_t)anim_tick, snow_level(), rain_open, wt->rain4, shown, vanish[0] - box[0], vanish[1] - box[1]);
        else if (snow_level() != SNOW_NONE)   /* port (snow_weather): flakes for streaks and none on the lens; the wet road under them is the rain's */
            weather_snow_draw(pixels, view_screen_w, box, rs, (uint32_t)anim_tick, snow_level(), rain_open, wt->rain4[WEATHER_RAIN_LEVELS - 1]);
        else
        weather_rain_draw_fx(pixels, view_screen_w, box, rs, (uint32_t)anim_tick, shown, vanish[0] - box[0], vanish[1] - box[1], (float)view->focal_x,
                             drive_light_night(), rain_open, wt->rain4, weather == WEATHER_STORM, port_rain_v2());
        if (snow_level() != SNOW_NONE && port_snow_fx())
            weather_snow_lens_draw(pixels, view_screen_w, box, (uint32_t)anim_tick, shown, vanish[0] - box[0], vanish[1] - box[1], wt->rain4[WEATHER_RAIN_LEVELS - 1], rain_open, wipers);
        if (snow_level() == SNOW_NONE)
        weather_lens_draw(pixels, view_screen_w, box, (uint32_t)anim_tick, shown, vanish[0] - box[0], vanish[1] - box[1], weather_rain_spray_level(drive_light_night()), wt->rain4, wt->lens, rain_open, wipers);   /* port (rain v3): drops on the lens */
    } else if (wt != NULL && weather_wet(weather) && snow_level() != SNOW_NONE) {   /* port (snow_weather): flakes without rain_fx too */
        if (port_snow_fx()) {   /* port (snow_fx): no speed here (no rain_fx), the view's middle for the vanishing point */
            weather_snow_draw_fx(pixels, view_screen_w, box, rs, (uint32_t)anim_tick, snow_level(), rain_open, wt->rain4, 0, (box[2] - box[0]) / 2, (box[3] - box[1]) / 2);
            weather_snow_lens_draw(pixels, view_screen_w, box, (uint32_t)anim_tick, 0, (box[2] - box[0]) / 2, (box[3] - box[1]) / 2, wt->rain4[WEATHER_RAIN_LEVELS - 1], rain_open, wipers);
        } else
            weather_snow_draw(pixels, view_screen_w, box, rs, (uint32_t)anim_tick, snow_level(), rain_open, wt->rain4[WEATHER_RAIN_LEVELS - 1]);
    }
    else if (wt != NULL && weather_wet(weather))   /* port (weather): the streaks over the whole view, under the 2D layer */
        weather_rain_draw(pixels, view_screen_w, box, rs, (uint32_t)anim_tick, wt->rain, weather == WEATHER_STORM, port_rain_v2());
    if (wipers != NULL) wiper_draw(pixels, view_screen_w, box, wipers, (uint32_t)anim_tick, weather_hooks_wiper_dark(object_pixels.palette));   /* port (wipers): over the drops, under the dash */
}


/* port: the sky over the view before the track covers it: the horizon art (or the console version sky, or the ring), stretched up and down where the switches say, drawn in bands */
static void draw_sky(const TrackArt *art, const TrackInfo *info, const PortedCamera *pc, uint8_t *pixels, QuadTarget *quad, RasterTarget *target, View *view, HorizonTables *horizon,
                     DrawContext *ctx, TrackCamera *cam, const int32_t box[4])
{
    const int sky_filter = art->has_horizon && modern && look_filter;  /* port: EXTRAS' Texture Filter on the sky */
    if (sky_filter) horizon_smooth_palette(&smooth, art->palette);
    SkyArtDraw *const own = psx_sky;   /* port: the console version sky (sky_art_draw.h) in place of the track's, on the flat horizon */
    if (own != NULL) {
        uint8_t shown[768];
        drive_light_palette(art->palette, shown);
        sky_art_draw_target(own, shown);
    }
    /* the PC's alternative sky (drive_light.c) is the morning / evening 3D ring of its INFO, on its own horizon */
    const int alt_ring = own != NULL && psx_sky_info != NULL && psx_sky_info->ring != 0;
    const TrackInfo *const sky_info = alt_ring ? psx_sky_info : info;
    const FshEntry *const ring_horz = alt_ring ? &own->horz : &art->horizon;
    const int ring = alt_ring || (own == NULL && art->has_horizon && info->ring != 0);
    if (ring) sky_ring_build(&sky_ring, sky_info, ring_horz);
    SkyStage stage = { pixels, quad, target, view, horizon, &ctx->clip, art, sky_info, cam, pc,
                       own != NULL ? &own->smooth : sky_filter ? &smooth : NULL, own != NULL ? &own->horz : &art->horizon, ring_horz,
                       box[0], box[2], 0, ring && sky_ring.enabled, own != NULL || (art->has_horizon && !ring), bands_threads() != 0 };
    if (stage.flat) stage.line = horizon_line(view, WORLD_SHIFT, pc->yaw, cam->matrix);
    static SkyStretch stretch;   /* port (sky_fill): the art reaching the view's top, built once a frame for all the bands */
    /* port (night): the flat colour over the art would be the day's navy beside the night's sky; at night the art is carried up to the top of the view as the
     * sky_fill switch does (every column takes its top rows' colour), whether the switch is on or not */
    const int night_fill = drive_light_night();
    if (stage.flat && (port_sky_fill() || night_fill))
        stage.horz = sky_stretch(&stretch, stage.horz, horizon->scale, stage.line, ctx->clip.y0,
                                 night_fill && port_night_stretch_clean() ? SKY_FILL_COLUMN_QUIET : port_sky_fill() ? port_sky_fill_mode() : SKY_FILL_COLUMN,
                                 night_fill && port_night_stretch_clean() && own != NULL ? own->bitmap.palette : art->palette);   /* the sky's texels index its own palette */   /* port (night_stretch_clean): no star carried up as a stripe */
    static SkyStretch below;   /* port (sky_fill_below): and downward, to the view's bottom, in 16:9 and 21:9 */
    if (stage.flat && port_sky_fill_below()) stage.horz = sky_stretch_below(&below, stage.horz, horizon->scale, stage.line, ctx->clip.y1);
    bands_rows(box[1] < 0 ? 0 : box[1], box[3] < view_screen_h ? box[3] : view_screen_h, sky_stage_band, &stage);
}

/* port: the track pass, by which way the camera looks (follow_off_road): the exe's pass ahead, or follow_behind's; returns whether the exe's pass was the one drawn */
static int draw_track(const Tri *tri, const PortedCamera *pc, TrackCamera *cam, DrawContext *ctx)
{
    int ahead = 1;   /* the exe's pass draws the track */
    if (shadow_race != NULL && shadow_race->port_follow_camera) {   /* port: the camera may look along the track either way */
        follow_arm_window(&render, follow_arm_segments());
        ahead = follow_off_road(pc) <= FOLLOW_EXE_BELOW;
        if (!ahead) {
            const FollowArm arm = { &render, cam, ctx, tri_is_circuit(tri), anim_tick, view_mirrored(), 0 };
            arm_drawing = 1;
            follow_behind_draw(&arm, follow_arm_segments(), render.draw_segments - 2);
            arm_drawing = 0;
        }
    }
    view_lamps_frame_start();
    street_halo_frame_begin();
    if (ahead) track_render_draw(&render, cam, ctx, view_mirrored(), anim_tick);
    return ahead;
}

/* port (render bands P2, P3): the frame's recorder */
struct RasterRecorder recorder;


void ported_view_render_cars(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *pc,
                             const PortedCar *list, int count, uint8_t *pixels)
{
    static View view;
    static HorizonTables horizon;
    const PortedViewport *vp = &view_viewport;
    view_init(&view, vp->left, vp->top, vp->width, vp->height, vp->fov, vp->fov_y != 0 ? vp->fov_y : vp->fov);
    /* port: the exe's rows scale grows with the width (0043a028); past 640 it keeps the 640's zoom */
    const int32_t rs = vp->render_scale > 0 ? vp->render_scale : 1;   /* port (render scale): the zoom of the unscaled width */
    horizon_tables(&horizon, vp->width, vp->fov, screen_horizon_scale(vp->scale, vp->width / rs));
    RasterTarget target = { pixels, view_screen_w, view_screen_h, 0, 0 };
    /* the box (00449f50: the layout's left, top, +0x20, +0x24); left + x, top + y only place the projection */
    const int32_t box[4] = { vp->sky_left, vp->sky_top, vp->sky_left + vp->sky_width, vp->sky_top + vp->sky_height };
    QuadTarget quad = { pixels, view_screen_w, box[0], box[1], box[2], box[3] };
    DrawContext ctx = { &view, { box[0], box[1], box[2], box[3] }, WORLD_SHIFT,
                        { 0, 0, 0x1999 },
                        { &quad, draw_uv, draw_full, draw_full_alt, draw_model, draw_cars, draw_skids,
                          modern ? draw_camera : NULL, draw_camera }, 0,
                        vp->near_scale };
    track_port_wide_board = wide_board_on();
    track_port_close_boxes = port_close_boxes(shadow_race != NULL && (shadow_race->port_reverse != 0 || shadow_race->port_follow_camera != 0));
    TrackCamera cam = { { pc->position[0], pc->position[1], pc->position[2] }, { 0 }, pc->node };
    track_camera_matrix(pc->pitch, pc->yaw, pc->roll, view_mirrored(), vp->scale, cam.matrix);  /* the layout's +4 (00449670) */
    /* 0044cc74 clears nothing (00473680 picks the surface): the horizon's
     * fills cover the view; the view (only: the rest of the surface holds
     * 004218d0's tiles and frame) cleared here for the frames without one. */
    rain_open = weather != WEATHER_CLEAR && shadow_race != NULL ? rain_open_at(shadow_race->chase.node) : 16;   /* port (weather_zones): once a frame, before anything of the weather */
    draw_sky(art, info, pc, pixels, &quad, &target, &view, &horizon, &ctx, &cam, box);
    const WeatherTables *wt = weather != WEATHER_CLEAR ? weather_tables(art) : NULL;
    cars_weather_sky(wt, pixels, box, &view, &cam);
    cars_stream_style(tri, art, pc, wt);
    cars_transform(&cam, &ctx, &view);
    cars_take_list(art, list, count, &quad, &view);
    skid_node = pc->node;
    /* port (render bands P2, P3): with threads the track's draws (road, scenery, cars, shadows, skid marks, smoke) are
     * recorded by the rasterizers, which find the recorder in the target, and replayed by bands at the end, in order */
    cars_frame_begin(art, wt, pixels);
    const int ahead = draw_track(tri, pc, &cam, &ctx);
    view_spray_frame_end();
    track_light_hook = NULL;
    lit_active = 0;
    if (shadow_race != NULL && shadow_race->port_follow_camera) draw_followed_car(pc, ahead);
    if (raster_active == &recorder) raster_record_end(&recorder);
    view_lamps_draw(&quad, art->palette);
    street_halo_draw(&quad, art->palette, drive_light_night());
    rain_over_view(pixels, box, &view, rs, wt);
}


int ported_view_wipers_active(void) { return weather_hooks_wiper_blades() > 0 && weather_wet(weather); }

uint8_t ported_view_light_at(uint32_t node, int point, uint8_t *tunnel)
{
    *tunnel = (uint8_t)track_node_light_tunnel(light_effects, node);
    return track_node_light_at(light_effects, node, point);
}

/* port (the baked light in the rear mirror's follow pass): the same sinks the main pass draws with, and the hook set for the pass, from the tables
 * the last render made. Without a light (or before the first render) begin gives 0 and the mirror draws as it did. */
int ported_view_mirror_light_begin(void)
{
    if (light_effects == NULL || !tables_made) return 0;
    track_light_hook = light_hook;
    lit_level = 0;
    memset(lit_corner, 255, sizeof lit_corner);
    return 1;
}

void ported_view_mirror_light_end(void)
{
    track_light_hook = NULL;
    lit_active = 0;
    track_light_tunnel = 0;
}

void ported_view_mirror_sinks(void (**uv)(void *, const FshEntry *, const int32_t[8], const int32_t[8]),
                              void (**full)(void *, const FshEntry *, const int32_t[8]),
                              void (**full_alt)(void *, const FshEntry *, const int32_t[8]),
                              void (**camera)(void *, const View *, const FshEntry *, const int32_t[8], const int32_t *const[4]))
{
    *uv = draw_uv, *full = draw_full, *full_alt = draw_full_alt, *camera = draw_camera;
}

void ported_view_render(const Tri *tri, const TrackArt *art, const TrackInfo *info, uint32_t node, int32_t yaw,
                        uint8_t *pixels)
{
    TriNode n, cn;
    if (tri_node(tri, node, &n) != TRI_OK) return;
    const PortedCamera cam = { { n.position.x, n.position.y + EYE_HEIGHT, n.position.z }, n.orientation * 0x400 + yaw,
                               0, (int32_t)node, 0 };
    const int32_t parked = (int32_t)light_tint_ring_node((int32_t)node + car_ahead, tri->ring_nodes);   /* a modulo: the rings are not powers of two */
    if (cars[0].ok && tri_node(tri, (uint32_t)parked, &cn) == TRI_OK) {
        const CarPose pose = { { cn.position.x, cn.position.y, cn.position.z }, 0, 0, cn.orientation * 0x400, NULL, 1 };
        ported_view_render_from(tri, art, info, &cam, &pose, parked, pixels);
    } else {
        ported_view_render_from(tri, art, info, &cam, NULL, -1, pixels);
    }
}
