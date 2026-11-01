/* --drive's frame: 0044cc74's calls (frame_draw.h) on the ported renderer, the cockpit and the mirror. */
#include "viewer/drive_session.h"
#include "viewer/drive_light.h"
#include "game/race/car_wheels.h"
#include "game/race/car_scenery.h"
#include "game/race/race_chase.h"
#include "game/race/race_draw_setup.h"
#include "viewer/draw_rands.h"
#include "viewer/dash_box.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DrawRands draw_rands;

static int32_t race_rand_of(void *user, int32_t n)
{
    return race_rand_below(user, n);
}

int32_t drive_draw_rand(void *ctx, int32_t n)
{
    (void)ctx;
    return draw_rands_take(&draw_rands, n, race_rand_of, &drive_sim.race);
}
RaceViews drive_views;

/* The eye where 004046b0 last placed it, angles as 004496ac reads them. */
PortedCamera chase_camera(const RaceChase *chase, uint32_t ring_nodes)
{
    return (PortedCamera){ { chase->position[0], chase->position[1], chase->position[2] },
                           chase->angles[1], chase->angles[0],
                           (int32_t)((uint32_t)chase->node & (ring_nodes - 1u)), chase->angles[2] };
}

/* Only without the HUD's shade (drive_hud.ready 0): with it the rows draw
 * each car through 0045a848's 004591d4 and 0045a3e8 (ported_view's
 * draw_cars), and this pose is not read. 004591d4 takes the tilt about z
 * at 3/4. */
static CarPose car_pose_of(const Car *car)
{
    return (CarPose){ { car->x, car->y, car->z }, car->draw_pitch + car->tilt_x, car->draw_roll + car->tilt_z * 3 / 4,
                      car->heading, car->use_matrix ? car->matrix : NULL, 1 };
}

/* CarBindTexture for the drive: `object` is the slot, `frame` the ref
 * (tire1..4) whose shape the wheel ref draws from now on. */
static void bind_wheels(void *user, uint32_t object, int32_t slot, uint32_t frame)
{
    (void)user;
    Object3D *obj = ported_view_car_object((int)object);
    if (obj != NULL && frame < obj->texture_count) object3d_set_texture(obj, slot, obj->textures[frame].texture);
}

static void morph_wheels(void *user, uint32_t object, int32_t part, int32_t weight)
{
    (void)user;
    Object3D *obj = ported_view_car_object((int)object);
    if (obj != NULL) object3d_morph_part(obj, part, weight);
}

/* drive_mirror's 004591d4 pass 1 (car_render with mirror 1) in the mirror's view. */
static void mirror_car(void *user, Car *car, const ModelView *mv, const QuadTarget *target)
{
    const Track *t = user;
    ported_view_render_car(car, mv, 1, target, t->art.palette);
}

/* 0044cc74's words (frame_draw.h), 00511fec among them (DriveHud.view_changed points here). */
FrameDraw drive_frame;

enum {
    RATE_TICK_MS = 10           /* 00473334's unit: hundredths */
};

/* 0044d2c4 / 0044ca3c's calls in --drive: the surfaces, the track, its textures and the palette are the port's
 * own (start_drive), the camera follows the player; 00449be4 is the port's own: its 00511fb8 (no reader) and 00511fbc
 * (drive_frame's, zeroed and never set) stay 0, and the mappers it names are ported_view's; 00449430 has no effect; 004fa0c4 (the second camera's step) is taken as 0. */
static void ds_none(void *ctx) { (void)ctx; }
static void ds_none_i(void *ctx, int32_t a) { (void)ctx, (void)a; }
static void ds_palette_get(void *ctx, uint8_t rgb[DRAW_SETUP_PALETTE]) { (void)ctx, (void)rgb; }
static void ds_palette_set(void *ctx, int32_t f, int32_t n, const uint8_t *rgb) { (void)ctx, (void)f, (void)n, (void)rgb; }
static void ds_mode(void *ctx, int32_t w, int32_t h, int32_t bits) { (void)ctx, (void)w, (void)h, (void)bits; }
static int32_t ds_surface(void *ctx, int32_t w, int32_t h, int32_t flags) { (void)ctx, (void)w, (void)h, (void)flags; return 0; }
static void ds_layout(void *ctx, RaceCamera *camera, int32_t layout) { race_view_layout(ctx, camera, layout); }
static void ds_chase_layout(void *ctx, RaceCamera *camera, int32_t layout)
{
    (void)ctx;
    race_view_layout(&drive_views, camera, layout);
}

static void dv_viewport(void *ctx, int32_t left, int32_t top, int32_t width, int32_t height, int32_t fov)
{
    view_init(&((RaceViews *)ctx)->view, left, top, width, height, fov, fov);
}
static void dv_none(void *ctx) { (void)ctx; }  /* 00475e44: view_init makes the reciprocals */
static int32_t dv_recip_table(void *ctx) { (void)ctx; return VIEW_RECIP_TABLE; }  /* 00475e30: the table's own word */

/* 00405630 on the race's first three cameras, --drive's views and the followed car's specs; the target y of view
 * kinds 0 and 10 is the in-car eye. */
static void ds_views(void *ctx)
{
    RaceState *race = ctx;
    const int32_t slot = race_slot(race->player);
    const Car *car = race->cars[slot];
    if (car == NULL || car->model < 0 || car->model >= VIEW_MODELS) return;  /* port guard: 004b790c is read unchecked */
    const CarSpecs *specs = &drive_sim.specs[slot];
    const RaceViewStartOps ops = { &drive_views, ds_layout, dv_viewport, dv_none, dv_recip_table };
    const RaceViewStartInputs in = { 0, specs->eye_height, specs->dash_drop, car->model, race_view_model_y };
    RaceViewStartOut out = { 0, { 0, 0 } };
    race_view_start(&ops, &drive_views, race->cameras, &in, &out);
    race->chase_kinds[10].target[1] = out.eye[0];
    race->chase_kinds[0].target[1] = out.eye[1];
}

enum { TRI_NODES_AT = 0x98c, TRI_NODE_BYTES = 36 };  /* the header's nodes (track_render.c's NODES_AT: 00505038) */

/* 0044c140 / 0044c148: mov [005104c8] / [005104cc], eax; ret (the rate's target, the automatic detail) */
static void ds_rate_target(void *ctx, int32_t v) { (void)ctx, drive_frame.rate_target = v; }
static void ds_auto_detail(void *ctx, int32_t v) { (void)ctx, drive_frame.auto_detail = v; }

static void ds_front_view(void *ctx, int32_t rear) { (void)ctx, (void)rear; }  /* 0040586c: the front is ported_view's */

/* 00404404 on the follow record and the TRI's nodes (00504208: ring_nodes - 1 on a circuit, -1 on a stage) */
static void ds_chase_start(void *ctx)
{
    RaceState *race = ctx;
    const Tri *tri = race->tri;
    if (tri == NULL) return;
    const int32_t mask = tri_is_circuit(tri) ? (int32_t)(tri->ring_nodes - 1u) : -1;
    const uint32_t node = (uint32_t)(race->chase.node & mask);
    if (node >= TRI_NODE_CAPACITY || TRI_NODES_AT + (size_t)(node + 1) * TRI_NODE_BYTES > tri->size) return;  /* port guard */
    const RaceChaseStartOps ops = { race, ds_views, ds_chase_layout, ds_front_view };
    race_chase_start_view(&ops, &race->chase, tri->data + TRI_NODES_AT, mask);
}

/* 0040540c on the race's first two cameras (004b78a8, 004b78c8) and --drive's views */
static void ds_screen(void *ctx, int32_t step, int32_t size)
{
    RaceState *race = ctx;
    const RaceViewScreenOps ops = { &drive_views, ds_layout };
    race_view_screen(&ops, &drive_views, &race->cameras[0], &race->cameras[1], step, size, &race->window_size);
}

static void ds_follow(void *ctx, int32_t kind)
{
    RaceState *race = ctx;
    const char *view = getenv("TNFS_VIEW");  /* port: the camera for the checks in place of the race's first */
    if (view != NULL) kind = atoi(view) % CHASE_KINDS;
    race_chase_follow(race, &race->chase, race->cars[race_slot(race->player)], kind);
}

/* 0044d2c4 from race_run: its words into drive_frame (the restart, 00511fc4, the draw distance asked for, the
 * mirror's count and state) and the HUD (004f9b3c). */
void drive_frame_setup(RaceState *race)
{
    const RaceDrawSetupOps ops = { race, ds_palette_get, ds_none, ds_palette_set, ds_none_i, ds_mode, ds_surface, ds_none,
                                   ds_none, ds_follow, ds_views, ds_chase_start, ds_rate_target, ds_auto_detail, ds_none, ds_none,
                                   ds_screen };
    RaceDrawSetup st = { 0 };
    st.restart = drive_frame.restart, st.mirror_state = drive_hud.mirror.state, st.use_parts = drive_hud.keys.use_parts;
    st.high = HIGH_SCREEN, st.width = SCREEN_W, st.height = SCREEN_H, st.back_buffer = 0;
    st.replay_kind = 0, st.chase_kind = race->chase.kind, st.detail = drive_keys.state.detail;
    drive_keys_race_begins(&drive_keys);
    st.pending_segments = drive_frame.pending_segments, st.window_size = (uint8_t)race->window_size;
    st.screen_kind = drive_keys.state.screen_kind, st.clear_view = drive_frame.clear_view;
    race_draw_start(&ops, &st);
    if (drive_keys.state.ultra != 0) st.pending_segments = DRAW_SEGMENTS_ULTRA;  /* port: the race starts on Ultra */
    drive_frame.restart = st.restart, drive_frame.unknown_511fc4 = st.unknown_511fc4;
    drive_frame.pending_segments = st.pending_segments, drive_frame.mirror_count = st.mirror_count;
    drive_frame.clear_view = st.clear_view;
    drive_hud.mirror.state = st.mirror_state, drive_hud.keys.use_parts = st.use_parts;
    if (drive_hud.dash_box != DASH_BOX_MAP) {  /* port: 0044d2c4's start (race_draw_setup) put the map on; the mode has it off */
        drive_hud.keys.use_parts = dash_box_start_parts((DashBox)drive_hud.dash_box);
        drive_hud.parts_pending = 1;
    }
}

/* render_drive's frame: what 0044cc74's calls need from --drive. */
typedef struct {
    const Track *t;
    uint8_t *pixels;
    RaceState *race;
    FrameDraw *d;
    int ticked;
    PortedCamera cam;
    DriveMirrorWorld mirror;
    DriveMirrorFrame mirror_pass;
    MirrorFrameOps mirror_ops;
    int mirror_on;
    uint32_t last_ms;
} DriveFrame;

static void df_none(void *ctx) { (void)ctx; }
static void df_none_i(void *ctx, int32_t a) { (void)ctx, (void)a; }
static void df_select(void *ctx, HudSurface s) { (void)ctx, (void)s; }  /* the targets are the ops' own */
static void df_net_send(void *ctx, int32_t to) { (void)ctx; if (drive_link_on) drive_link.deadline(to); }

/* 00410c18 with 004fcdac set: the timer stands still while it waits (0043b970), and a wait that fails drops the
 * game (0052b314) */
static int32_t df_net_wait(void *ctx, int32_t kind)
{
    (void)ctx;
    if (!drive_link_on) return 1;
    const uint32_t start = platform_ticks_ms();
    const int32_t came = drive_link.wait(kind);
    drive_pause.paused_ms += platform_ticks_ms() - start;
    if (came == 0) *drive_link.lost = 1;
    return came;
}
static void df_chase_tick(void *ctx) { DriveFrame *f = ctx; race_chase_tick(f->race, &f->race->chase); }
static void df_restream(void *ctx) { race_scenery_restream(((DriveFrame *)ctx)->race); }
static void df_tiles(void *ctx) { drive_hud_tiles(&drive_hud, ((DriveFrame *)ctx)->race); }
static void df_map(void *ctx) { drive_hud_map(&drive_hud, ((DriveFrame *)ctx)->race); }
static void df_hud_clear(void *ctx) { (void)ctx; drive_hud_clear(&drive_hud); }
static void df_clip(void *ctx, int32_t l, int32_t r, int32_t t, int32_t b) { (void)ctx, (void)l, (void)r, (void)t, (void)b; }
static void df_text_colour(void *ctx, int32_t colour, int32_t back) { (void)ctx, (void)colour, (void)back; }
static void df_text(void *ctx, int32_t x, int32_t y, int32_t node, int32_t letter) { (void)ctx, (void)x, (void)y, (void)node, (void)letter; }
static void df_line(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t c) { (void)ctx, (void)x0, (void)y0, (void)x1, (void)y1, (void)c; }
static void df_show(void *ctx, HudSurface s) { (void)ctx, (void)s; }  /* run_draw presents the frame */

/* 0043f540: the renderer's cache and list part (ported_view_stream), then the mirror's rows (drive_mirror_begin). */
static void df_stream(void *ctx)
{
    DriveFrame *f = ctx;
    const Track *t = f->t;
    RaceState *race = f->race;
    f->mirror = (DriveMirrorWorld){ &t->tri, &t->art, race, ported_view_stream(&t->tri, &t->art, f->cam.node),
                                    &race->chase.flags, { 0, 0, 0 }, race->cars[race_slot(race->player)], (void *)t,
                                    mirror_car, ported_view_modern() };
    ported_view_near_dir(f->mirror.near_dir);  /* as the front view last left them */
    drive_mirror_zoom(&drive_hud.mirror, drive_wide_mirror_zoom(&drive_hud.mirror));
    f->mirror_on = drive_mirror_begin(&drive_hud.mirror, &f->mirror, &f->mirror_pass);
    if (f->mirror_on) f->mirror_ops = drive_mirror_ops(&f->mirror_pass);
}

/* 0040586c: the front view is the port's own (ported_view_render_cars), the rear the mirror's. */
static void df_view(void *ctx, int rear)
{
    DriveFrame *f = ctx;
    if (rear && f->mirror_on) f->mirror_ops.view(f->mirror_ops.ctx, 1);
}

static void df_view_choose(void *ctx, int32_t view, int32_t size)
{
    DriveFrame *f = ctx;
    if (race_view_choose(&drive_views, f->race->chase.camera, view, size)) f->d->changed = 1;  /* 00511fec */
    drive_views.changed = 0;
}

/* 00459c50 and 00459d70 (ported_view_frame_rows does both): once a tick, as the window draws two frames a tick
 * (the second would put the cars a row further back, under the road); the other frame keeps the rows. */
static void df_ahead(void *ctx, int32_t node)
{
    (void)node;
    if (((DriveFrame *)ctx)->ticked) ported_view_frame_rows(drive_hud.mirror.car_rows);
    else ported_view_mirror_rows_again(drive_hud.mirror.car_rows);  /* port: by this frame's camera node, not the tick's */
}

static void df_mirror_transform(void *ctx) { DriveFrame *f = ctx; if (f->mirror_on) f->mirror_ops.transform(f->mirror_ops.ctx); }
static void df_mirror_select(void *ctx) { DriveFrame *f = ctx; if (f->mirror_on) f->mirror_ops.select(f->mirror_ops.ctx); }
static void df_mirror_sky(void *ctx) { DriveFrame *f = ctx; if (f->mirror_on) f->mirror_ops.sky(f->mirror_ops.ctx); }
static void df_mirror_flip(void *ctx) { DriveFrame *f = ctx; if (f->mirror_on) f->mirror_ops.flip(f->mirror_ops.ctx); }

static void df_mirror_scene(void *ctx, int32_t count)
{
    DriveFrame *f = ctx;
    if (f->mirror_on) f->mirror_ops.scene(f->mirror_ops.ctx, count);
}

/* 0044b510 with 004496ac, 0044aa94, 0044a804 and 0043af80 before it: the port's front view, cars and all. */
static void df_near(void *ctx, int32_t segments, int32_t node)
{
    (void)node;
    DriveFrame *f = ctx;
    if (segments != 0) ported_view_set_reach(segments);  /* the distance the frame took (0044cd29); 0 before the race set it */
    const Track *t = f->t;
    PortedCar list[RACE_CARS];
    for (int i = 0; i < drive_sim.count; i++) {
        const Car *c = f->race->cars[i];
        list[i] = (PortedCar){ car_pose_of(c), (int32_t)((uint32_t)c->draw_node & (t->tri.ring_nodes - 1u)), f->race->cars[i] };
    }
    uint8_t *view = drive_hud_view(&drive_hud, f->pixels);
    if (drive_wide_active()) {  /* port: the 3D at the screen's width (and render scale), its 640 box left LAYER_KEY */
        const int s = drive_wide.scale;
        drive_wide_key_box(f->race, view);
        memset(drive_wide.pixels, 0, (size_t)drive_wide.width * s * SCREEN_H * s);
        const PortedViewport unscaled = drive_wide_viewport(f->race);
        const PortedViewport vp = ported_view_scaled(&unscaled, s);
        ported_view_set_screen(drive_wide.width * s, SCREEN_H * s);
        ported_view_set_viewport(&vp);
        ported_view_render_cars(&t->tri, &t->art, &t->info, &f->cam, list, drive_sim.count, drive_wide.pixels);
        drive_light_world_done(drive_wide.pixels, (size_t)drive_wide.width * s * SCREEN_H * s);   /* port: before the 2D layer */
        ported_view_set_screen(SCREEN_W, SCREEN_H);
        drive_wide_set_viewport(f->race);
        return;
    }
    ported_view_render_cars(&t->tri, &t->art, &t->info, &f->cam, list, drive_sim.count, view);
    drive_light_world_done(view, (size_t)SCREEN_W * SCREEN_H);   /* port: before the HUD */
}

/* The redline: the player's specs' +0xac, for the cockpit's car and the HUD's alike. */
static int32_t drive_redline(const RaceState *race)
{
    return drive_sim.specs[race_slot(race->player)].unknown_0ac;
}

static void df_cockpit(void *ctx, Car *car)
{
    DriveFrame *f = ctx;
    drive_hud.cockpit_world.unknown_4f9b48 = drive_sim.police_lights;  /* 004f9b48: 00472560 toggles it in the tick */
    drive_hud.mirror.state = f->d->mirror_state;  /* 00511ff4 as the mirror block left it; 0044c060 reads it */
    drive_hud_in_car(&drive_hud, f->race, car, drive_redline(f->race), f->pixels);
}

static void df_hud(void *ctx, Car *car, int32_t changed)
{
    DriveFrame *f = ctx;
    drive_hud.cockpit_world.unknown_4f9b48 = drive_sim.police_lights;
    drive_hud_outside(&drive_hud, f->race, car, drive_redline(f->race), f->pixels, changed);
}

static int32_t df_time(void *ctx)
{
    DriveFrame *f = ctx;
    const uint32_t now = platform_ticks_ms(), elapsed = (now - f->last_ms) / RATE_TICK_MS;
    f->last_ms += elapsed * RATE_TICK_MS;
    return (int32_t)elapsed;
}

static void df_format_rate(void *ctx, char *text, int32_t whole, int32_t tenth)
{
    (void)ctx;
    snprintf(text, FRAME_TEXT, "%2d.%1d", (int)whole, (int)tenth);
}

static const FrameDrawOps drive_frame_ops = {
    NULL, df_chase_tick, df_none, df_net_send, df_net_wait, df_restream, df_stream, df_view, df_none, df_select,
    df_view_choose, df_tiles, df_none, df_none, df_none_i, df_ahead, df_mirror_transform, df_mirror_select,
    df_mirror_sky, df_mirror_scene, df_mirror_flip, df_none, df_clip, df_none, df_none_i, df_near, df_map, df_none,
    df_text_colour, df_text, df_hud_clear, df_cockpit, df_line, df_hud, df_show, df_none, df_none, df_time,
    df_format_rate, df_none };

/* The race's words 0044cc74 reads, from --drive's state; 00511fec taken over from a camera's view select. */
static void drive_frame_in(FrameDraw *d, RaceState *race)
{
    d->chase = &race->chase;
    d->camera = race->chase.camera;
    d->cars = race->cars;
    d->followed = race->player;
    d->viewed = race->viewed;
    d->unknown_513e60 = race->unknown_513e60;
    if (drive_views.changed) d->changed = 1, drive_views.changed = 0;
    d->window_size = race->window_size;
    d->screen_kind = drive_keys.state.screen_kind;  /* 004fb789, F2's */
    d->mirror_state = drive_hud.mirror.state;
    d->net_start = race->unknown_4fb7d4;
    d->unknown_4f9b7c = drive_hud.keys.dash_camera;
    d->width = SCREEN_W, d->height = SCREEN_H;
    drive_hud.view_changed = &d->changed;
}

/* 00458d4c's 00458614 and 00458818 on cars[004fbb08] as it draws it (models 200 and up have the wheel refs). The
 * wheel frames only after a tick: the window draws two frames a tick, and a frame without one sees the car still
 * (its front wheels would stop). */
static int render_ticked;

static int has_wheels(const Car *car)
{
    const int32_t slot = car->index;
    return slot >= 0 && slot < RACE_CARS && drive_cars.loaded[slot] && drive_sim.race.models[slot] >= 200;
}

static void drive_wheel_frames(void *ctx, Car *car)
{
    (void)ctx;
    if (render_ticked && has_wheels(car)) car_wheel_frames(car, &drive_sim.race, bind_wheels, NULL);
}

static void drive_steer(void *ctx, Car *car)
{
    (void)ctx;
    if (has_wheels(car)) car_steer_wheels(car, &drive_sim.race, morph_wheels, NULL);
}

void render_drive(const Track *t, uint8_t *pixels)
{
    RaceState *race = &drive_sim.race;
    render_ticked = drive_cars.ticked;
    ported_view_set_wheels(drive_wheel_frames, drive_steer, NULL);
    const int ticked = drive_cars.ticked;
    drive_cars.ticked = 0;
    draw_rands_frame(&draw_rands, ticked);
    /* port: the open models of the scenery are drawn from behind too in a reversed or 360 camera race; both are fixed for the race (extras.cfg is read at its start, the pause menu cannot change them), and the models are made once per track */
    ported_view_set_models(t->models, t->model_ok, race->mirrored, port_two_sided_scenery(race->port_reverse != 0 || race->port_follow_camera != 0));
    ported_view_set_shadows(race, drive_hud.ready ? drive_hud.shade : NULL);
    ported_view_set_tick(race->tick);
    if (drive_hud.ready) ported_view_set_tags(drive_hud.car_colours, HUD_MAP_COLOURS, drive_hud.strip_colour);
    if (drive_hud.palette != NULL) ported_view_set_lights(drive_hud_palette_write, &drive_hud, drive_hud.palette->slot_colours);
    static uint32_t last_ms;
    DriveFrame f = { 0 };
    f.t = t, f.pixels = pixels, f.race = race, f.d = &drive_frame, f.ticked = ticked, f.last_ms = last_ms;
    drive_frame_in(&drive_frame, race);
    drive_wide_set_viewport(race);
    f.cam = chase_camera(&race->chase, t->tri.ring_nodes);
    FrameDrawOps ops = drive_frame_ops;
    ops.ctx = &f;
    frame_draw(&ops, &drive_frame);
    drive_hud.mirror.state = drive_frame.mirror_state;
    last_ms = f.last_ms;
    const int bumper = drive_bumper.mirror && drive_bumper_view(race->chase.kind) && drive_hud.police.screen == NULL
                       && drive_bumper_mirror_render(&f.mirror, drive_frame.mirror_count);
    if (drive_wide_active()) drive_wide_compose(race, pixels);
    drive_bumper_draw(race, pixels, bumper);
    drive_wide_finish();   /* port (render scale): the 2D layer over the enlarged 3D, bumper pieces and all */
}
