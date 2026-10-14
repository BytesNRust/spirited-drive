#include "viewer/ported_view.h"

#include "game/gfx/quad.h"
#include "game/race/horizon.h"
#include "game/race/sky_ring.h"
#include "game/race/track_render.h"
#include "game/gfx/sprite_scaled.h"
#include "game/race/view.h"
#include "game/race/car_draw.h"
#include "game/race/car_road.h"
#include "game/race/car_shadow.h"
#include "game/race/car_tag.h"
#include "game/race/car_lights.h"
#include "game/gfx/x86_int.h"
#include "game/race/car_wheels.h"
#include "game/gfx/flat.h"
#include "game/race/model_draw.h"
#include "game/race/row_cars.h"
#include "game/race/skid_marks.h"
#include "game/race/tyre_smoke.h"
#include "viewer/object_sink.h"
#include "viewer/raster.h"

#include <stdlib.h>
#include <string.h>

enum {
    WIDTH = 640,
    HEIGHT = 480,
    FOV = 96,              /* chase views (004b78a8, view 1) */
    WORLD_SHIFT = 11,      /* 004c52bc */
    EYE_HEIGHT = 111411,   /* camera mode 2 (004b6bb0): 1.7 m */
    SKY_WORLD_SHIFT = 8,   /* 004bb8d4 */
    SKY_POSITION_SHIFT = 10  /* 0043af80: -position >> 10 */
};

/* The race's default mapper set (00449b90): every wrapper passes textures
 * of type 0x45 to the transparent mapper and the rest to the opaque one. */
static int transparent(const FshEntry *texture)
{
    return texture->type == TRACK_TEXTURE_TRANSPARENT_TYPE;
}

/* 004f9e94 = 004254e8 -> 00425210: 00486950 / 00487180 */
static void draw_uv(void *user, const FshEntry *texture, const int32_t uv[8], const int32_t screen[8])
{
    quad_draw(user, texture, uv, screen, transparent(texture));
}

/* 004f9e9c = 00425590 -> 004251d0: the whole texture as two triangles */
static void draw_full(void *user, const FshEntry *texture, const int32_t screen[8])
{
    quad_draw_full_tri(user, texture, screen, transparent(texture));
}

/* 004f9e90 = 00425558 -> 00425250: the whole texture as one quad */
static void draw_full_alt(void *user, const FshEntry *texture, const int32_t screen[8])
{
    quad_draw_full(user, texture, screen, transparent(texture));
}

/* Scenery models through the ported 3D engine (00458c04), one object per
 * model as the race keeps them. */
static const ViewModel *models;
static Object3D objects[PORTED_VIEW_MODELS];
static int object_ok[PORTED_VIEW_MODELS];
static ModelView model_view;
static ObjectSink object_pixels;

static void draw_model(void *user, int model, const int32_t position[3], uint32_t heading)
{
    (void)user;
    if (model < 0 || model >= PORTED_VIEW_MODELS || !object_ok[model]) return;
    const Object3DSink sink = object_sink(&object_pixels);
    model_draw_object(&objects[model], position, (int32_t)heading, &model_view, &sink);
}

void ported_view_set_models(const ViewModel *list, const int *ok)
{
    if (list == models) return;
    for (int i = 0; i < PORTED_VIEW_MODELS; i++) {
        if (object_ok[i]) object3d_free(&objects[i]);
        object_ok[i] = list != NULL && ok[i] && object3d_create(&objects[i], &list[i].mesh);
        if (object_ok[i] && list[i].has_textures) object3d_bind(&objects[i], &list[i].textures);
    }
    models = list;
}

/* The cars on the road, each drawn with its row (0045a848). Slot 0 is
 * also the parked car of ported_view_render. */
typedef struct {
    Object3D objects[2];
    Object3D *high, *low;   /* objects, or the borrowed ones */
    CarModel model;
    int ok;
    int borrowed;
    CarPose pose;
    int32_t node;
    Car *car;
} ViewCar;

static ViewCar cars[PORTED_VIEW_CARS];
static int car_count, car_ahead;

static void release_car(ViewCar *c)
{
    if (c->ok) {
        car_model_free(&c->model);
        if (!c->borrowed) {
            object3d_free(&c->objects[0]);
            object3d_free(&c->objects[1]);
        }
    }
    c->ok = c->borrowed = 0;
    c->high = c->low = NULL;
}

void ported_view_set_car_slot(int slot, const ViewModel *high, const ViewModel *low)
{
    if (slot < 0 || slot >= PORTED_VIEW_CARS) return;
    ViewCar *c = &cars[slot];
    release_car(c);
    if (high == NULL || low == NULL) return;
    if (!object3d_create(&c->objects[0], &high->mesh)) return;
    if (!object3d_create(&c->objects[1], &low->mesh)) {
        object3d_free(&c->objects[0]);
        return;
    }
    if (high->has_textures) object3d_bind(&c->objects[0], &high->textures);
    if (low->has_textures) object3d_bind(&c->objects[1], &low->textures);
    c->high = &c->objects[0], c->low = &c->objects[1];
    c->ok = car_model_init(&c->model, c->high, c->low);
}

void ported_view_set_car_objects(int slot, Object3D *high, Object3D *low)
{
    if (slot < 0 || slot >= PORTED_VIEW_CARS) return;
    ViewCar *c = &cars[slot];
    release_car(c);
    if (high == NULL || low == NULL) return;
    c->high = high, c->low = low, c->borrowed = 1;
    c->ok = car_model_init(&c->model, high, low);
}

int ported_view_draw_car(int slot, const CarPose *pose, const ModelView *mv, int mirror, const QuadTarget *target,
                         const uint8_t palette[768])
{
    if (slot < 0 || slot >= PORTED_VIEW_CARS || !cars[slot].ok) return 0;
    static uint8_t shade[256];
    static const uint8_t *shaded;
    if (shaded != palette) object_sink_shade(palette, shade), shaded = palette;
    ObjectSink pixels = { target, palette, shade };
    const Object3DSink sink = object_sink(&pixels);
    return car_draw(&cars[slot].model, pose, mv, mirror, &sink);
}

Object3D *ported_view_car_object(int slot)
{
    return slot >= 0 && slot < PORTED_VIEW_CARS && cars[slot].ok ? cars[slot].high : NULL;
}

void ported_view_set_car(const ViewModel *high, const ViewModel *low, int ahead)
{
    ported_view_set_car_slot(0, high, low);
    car_ahead = ahead;
}

static const RaceState *shadow_race;
static const uint8_t *shadow_remap;

void ported_view_set_shadows(const RaceState *race, const uint8_t *remap)
{
    shadow_race = race, shadow_remap = remap;
}

static void shadow_contact(void *ctx, Car *car)
{
    (void)ctx;
    car_road_contact(car, shadow_race, 1);  /* 00428918(car, 1) */
}

/* 00517620..: the followed car's skid marks; 0051c7cf the rows holding one. */
static SkidMarks skid_marks;
static int32_t skid_node;   /* the camera's node (chase +0x38) */

enum { SKID_HIGH_RES = 1 };  /* 004fa0c0, as drive_hud's HIGH_RES */

static SkidRing skid_ring(void)
{
    return (SkidRing){ 0, (int32_t)shadow_race->tri->ring_nodes - 1, shadow_race->segments };
}

static SkidView skid_view(void)
{
    return (SkidView){ object_pixels.target, shadow_remap, &model_view, SKID_HIGH_RES, 0 };
}

/* 004596a8's 00459eb8(index): the newest marks, as they are laid. */
static void skid_draw_new(void *ctx, int32_t index)
{
    (void)ctx;
    const SkidView v = skid_view();
    const SkidRing ring = skid_ring();
    skid_draw(&v, &ring, &skid_marks, index, 0, 0, 0, NULL, NULL);
}

static void shadow_followed(void *ctx, Car *car, const int32_t f[3], const int32_t s[3], const int32_t p[3])
{
    (void)ctx;
    const int32_t surface = car->surface;
    const SkidInputs in = { 0, shadow_race->tick, shadow_race->surfaces[surface].off_road };
    const SkidOps ops = { NULL, skid_draw_new };
    skid_record(&ops, &skid_marks, car, &in, f, s, p);
}

/* 0044b87d: 00459eb8(-999, camera node, row; s, t). */
static void draw_skids(void *user, int32_t row, const int32_t s[2], const int32_t t[2])
{
    (void)user;
    if (shadow_race == NULL || shadow_remap == NULL) return;
    const SkidView v = skid_view();
    const SkidRing ring = skid_ring();
    skid_draw(&v, &ring, &skid_marks, -999, skid_node, row, 0, s, t);
}

/* 0051c7ac: the rows ahead of the camera holding a car (00459d70). */
static uint8_t ahead_rows[ROW_CARS_AHEAD];

void ported_view_frame_rows(uint8_t behind[MIRROR_CAR_ROWS])
{
    if (shadow_race == NULL) return;
    const SkidRing ring = skid_ring();
    skid_rows(&skid_marks, &ring, shadow_race->chase.node);
    car_rows_ahead(shadow_race->cars, shadow_race->car_count, shadow_race->segments, shadow_race->chase.node, ahead_rows,
                   behind);
}

static const Car *followed_car(void)
{
    return shadow_race->cars[race_slot(shadow_race->player)];
}

/* 00513e88 / 00514298, *004c795c: NULL until the race has its smoke. */
static const PuffArt *puff_art;
static const PuffBlend *puff_blend;
static int32_t (*puff_rand)(void *ctx, int32_t n);
static void *puff_rand_ctx;
static CrashTrail *puff_trail;   /* 005146ac */

void ported_view_set_puffs(const PuffArt *art, const PuffBlend *blend, int32_t (*rand_below)(void *ctx, int32_t n),
                           void *ctx, CrashTrail *trail)
{
    puff_art = art, puff_blend = blend, puff_rand = rand_below, puff_rand_ctx = ctx, puff_trail = trail;
}

static PuffView puff_view(void)
{
    PuffView v = { object_pixels.target, puff_blend->rows, { 0 }, { 0 }, SKID_HIGH_RES, puff_rand_ctx, puff_rand };
    for (int k = 0; k < PUFF_SHAPES; k++) v.smoke[k] = puff_art->smoke[k], v.dirt[k] = puff_art->dirt[k];
    return v;
}

static void puff_1(void *ctx, int32_t x, int32_t y, int32_t size)
{
    (void)ctx;
    const PuffView v = puff_view();
    puff_smoke(&v, x, y, size);
}

static void puff_other(void *ctx, int32_t x, int32_t y, int32_t size)
{
    (void)ctx;
    const PuffView v = puff_view();
    puff_dirt(&v, x, y, size);
}

/* 004588ac */
static void smoke_puffs(void *ctx, int32_t corner, int32_t size)
{
    (void)ctx;
    const SkidPuffOps ops = { NULL, puff_1, puff_other };
    skid_puffs(&ops, &skid_marks, shadow_race->tick, corner, size);
}

/* 004587ac */
static int32_t smoke_view_angle(void *ctx, const Car *car)
{
    (void)ctx;
    return car_view_angle(car, shadow_race);
}

/* 00458988 */
static void smoke_draw(void *ctx, int in_car)
{
    const TyreSmokeOps ops = { puff_rand_ctx, smoke_view_angle, puff_rand, smoke_puffs };
    tyre_smoke(&ops, (const Car *)ctx, in_car, shadow_race->camera_roll_add);
}

/* 00490570 with *004c55e0 ('smok') */
static void trail_puff(void *ctx, int32_t scale, int32_t x, int32_t y)
{
    (void)ctx;
    sprite_scaled_blend_at(object_pixels.target, puff_blend->rows, puff_art->smok, scale, x, y);
}

enum { CAR_SMOKE = 2 };  /* car +0x175 bit 1: the crash smoke runs (car_crash_tick.c's FLAG_SMOKE) */

static int32_t last_near_dir[3];
static int32_t last_near;

/* 004591d4 after the car: the crash smoke while the followed car's +0x175 bit 1 is set, outside replays; its puffs
 * moved by the model's 'smok' point on screen less the car's place on screen (0044a1c4), or not for the far model. */
static void draw_crash_trail(const Car *car, const CarModel *model)
{
    if (puff_trail == NULL || puff_art->smok == NULL || shadow_race->replay != 0 || !(car->flags_175 & CAR_SMOKE)) return;
    int32_t offset[2] = { 0, 0 };
    const int32_t point = car->smoke_point;
    if (car->far == 0 && point >= 0 && (uint32_t)point < model->body->vertex_count) {
        TrackCamera cam = { { 0 }, { 0 }, 0 };
        memcpy(cam.position, model_view.camera_position, sizeof cam.position);
        memcpy(cam.matrix, model_view.camera_matrix, sizeof cam.matrix);
        DrawContext dc = { 0 };
        dc.view = model_view.view, dc.world_shift = WORLD_SHIFT;
        memcpy(dc.near_dir, last_near_dir, sizeof dc.near_dir);
        dc.near = last_near;
        const int32_t place[3] = { car->x, car->y, car->z };
        int32_t camera[3], screen[2];
        track_point_screen(&cam, &dc, place, camera, screen);
        offset[0] = model->body->screen[point][0] - screen[0];
        offset[1] = model->body->screen[point][1] - screen[1];
    }
    const CrashTrailView v = { model_view.view, model_view.camera_position, model_view.camera_matrix, WORLD_SHIFT, NULL,
                               trail_puff };
    crash_trail_draw(puff_trail, offset, &v);
}

/* 0045a724 */
static void row_prepare(void *ctx, Car *car, int pass)
{
    (void)ctx;
    const CarShadowOps ops = { NULL, shadow_contact, shadow_followed };
    int32_t screen[8];
    car_prepare(&ops, car, &model_view, pass, car == followed_car(), object_pixels.target, shadow_remap, screen);
}

/* 0050b7d4 / 0050b7b4 */
static const uint8_t *tag_colours;
static int tag_colour_count;
static uint32_t tag_outline;

void ported_view_set_tags(const uint8_t *colours, int count, uint32_t outline)
{
    tag_colours = colours, tag_colour_count = count, tag_outline = outline;
}

static void tag_polygon(void *ctx, uint32_t colour, int count, const int32_t *points)
{
    (void)ctx;
    flat_polygon(object_pixels.target, colour, count, points);
}

static void tag_line(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t colour)
{
    (void)ctx;
    flat_line(object_pixels.target, x0, y0, x1, y1, colour);
}

enum { TAG_HIGH_LIFT = 0x3800, TAG_HIGH_GAP = 15, TAG_LOW_LIFT = 0x2000, TAG_LOW_GAP = 7 };

/* 004591d4's tail for a car other than the viewed one (or any in a replay): in races of type 1 and up, a racer
 * (slot below 004fb7dc) gets its mark (00459064) above its place on screen (0044a1c4), lifted by 0x3800 / z + 15
 * (0x2000 / z + 7 at low resolution). */
static void draw_tag(const Car *car)
{
    const RaceState *race = shadow_race;
    if (tag_colours == NULL || race->race_type < 1 || car->index > race->racers - 1 || car->index >= tag_colour_count) return;
    TrackCamera cam = { { 0 }, { 0 }, 0 };
    memcpy(cam.position, model_view.camera_position, sizeof cam.position);
    memcpy(cam.matrix, model_view.camera_matrix, sizeof cam.matrix);
    DrawContext dc = { 0 };
    dc.view = model_view.view, dc.world_shift = WORLD_SHIFT;
    memcpy(dc.near_dir, last_near_dir, sizeof dc.near_dir);
    dc.near = last_near;
    const int32_t place[3] = { car->x, car->y, car->z };
    int32_t camera[3], screen[2];
    track_point_screen(&cam, &dc, place, camera, screen);
    const int high = SKID_HIGH_RES != 0;
    const int32_t lift = x_div(high ? TAG_HIGH_LIFT : TAG_LOW_LIFT, camera[2]) + (high ? TAG_HIGH_GAP : TAG_LOW_GAP);
    const CarTagOps ops = { NULL, tag_polygon, tag_line };
    const CarTagView v = { tag_colours, tag_outline, SKID_HIGH_RES, race->humans, 0 };
    car_tag(&ops, &v, screen[0], screen[1] - lift, car->index, camera[2]);
}

/* 00476460 and 0051c798; 0051c7f8 / 004c5820 the police flash's last turn and phase */
static void (*light_write)(void *ctx, int32_t first, int32_t count, const uint8_t *colours);
static void *light_ctx;
static const uint8_t *light_slots;
static int32_t flash_tick, flash = 1;

void ported_view_set_lights(void (*write)(void *ctx, int32_t first, int32_t count, const uint8_t *colours), void *ctx,
                            const uint8_t *slot_colours)
{
    light_write = write, light_ctx = ctx, light_slots = slot_colours;
}

/* 00458d4c before the near model: the car's lights in the palette. */
static void set_lights(const Car *car)
{
    if (light_write == NULL || car->far != 0) return;
    const RaceState *race = shadow_race;
    const CarLightInputs in = { { race->stop_notice[0], race->stop_notice[1] }, light_slots, race->finish_cars, race->tick,
                                &flash_tick, &flash };
    const CarLightOps ops = { light_ctx, light_write };
    car_lights(&ops, &in, car);
}

/* 004591d4's +0x78e, set once the car is in view (0045a3e8's camera-space place, +0x391 / +0x399): the low detail
 * model past 0x1900 (not for slot 2), past 0xa00 in the mirror pass, which leaves out cars past 0x2100. */
static int set_far(Car *car, int pass)
{
    const int32_t x = car->camera_position[0], z = car->camera_position[2];
    const int64_t across = x < 0 ? -(int64_t)x : x;
    if (z < CAR_MIN_Z || z > CAR_MAX_Z || 2 * (int64_t)z < across || (pass && z > CAR_MIRROR_MAX_Z)) return 0;
    car->far = pass ? z > CAR_MIRROR_FAR_Z : z > CAR_FAR_Z && car->index != 2;
    return 1;
}

/* 004591d4: the car's model, at the pose the list gave it; for the followed car its tyre smoke and crash smoke. */
static void row_draw(void *ctx, Car *car, int pass)
{
    (void)ctx;
    const Object3DSink sink = object_sink(&object_pixels);
    if (set_far(car, pass) && pass == 0) set_lights(car);
    for (int i = 0; i < car_count; i++)
        if (cars[i].ok && cars[i].car == car) {
            const CarSmokeHook smoke = { car, smoke_draw };
            const int smoking = car == followed_car() && puff_art != NULL && puff_blend != NULL && puff_rand != NULL;
            const int drawn = car_draw_smoking(&cars[i].model, &cars[i].pose, &model_view, pass, &sink, smoking ? &smoke : NULL);
            if (!drawn) continue;
            if (car == followed_car() && shadow_race->replay == 0) {
                if (smoking) draw_crash_trail(car, &cars[i].model);
            } else {
                draw_tag(car);
            }
        }
}

/* 0045a768 */
static void row_unseen(void *ctx, Car *car)
{
    (void)ctx;
    const CarShadowOps ops = { NULL, shadow_contact, shadow_followed };
    car_skid_unseen(&ops, car);
}

/* 0045a848 in a race; else each listed car on the node. */
static void draw_cars(void *user, int32_t node)
{
    (void)user;
    if (shadow_race != NULL && shadow_remap != NULL) {
        const RaceState *race = shadow_race;
        const RowCarsOps ops = { NULL, row_prepare, row_draw, row_unseen };
        const RowCarsInputs in = { race->chase.node, race->segments, ahead_rows, race->drawn, race->drawn_count, followed_car(),
                                   race->chase.kind, 0 };
        cars_on_row(&ops, &in, node);
        return;
    }
    const Object3DSink sink = object_sink(&object_pixels);
    for (int i = 0; i < car_count; i++)
        if (cars[i].ok && cars[i].node == node) car_draw(&cars[i].model, &cars[i].pose, &model_view, 0, &sink);
}

typedef struct {
    const QuadTarget *quad;
    const RasterTarget *raster;
} SkyTargets;

/* 004f9e98 = 00425520 -> 004252ec */
static void sky_triangle(void *user, const FshEntry *texture, const int32_t uv[6], const int32_t screen[6])
{
    const SkyTargets *t = user;
    tri_draw(t->quad, texture, uv, screen, transparent(texture));
}

static void sky_fill(void *user, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t colour)
{
    const SkyTargets *t = user;
    raster_fill(t->raster, x, y, w, h, colour);
}

/* 0043af80: the 3D sky ring of the tracks whose INFO asks for one. */
/* 0043af80's ring path: 0043b298 with the camera's yaw and roll (the follow
 * record's +0x44, +0x48); 0043b0c8 slopes the fills with the roll. */
static PortedViewport viewport = { 0, 0, WIDTH, HEIGHT, FOV, 0x10000, 0, 0, WIDTH, HEIGHT };

/* 00512040..: the road direction the last frame's transform left (the mirror draws with it). */
static int32_t last_near_dir[3] = { 0, 0, 0x1999 };
static int32_t last_near = 62259 >> WORLD_SHIFT;  /* *(00511fe8) +4 as the last frame's transform left it */

void ported_view_near_dir(int32_t out[3])
{
    memcpy(out, last_near_dir, sizeof last_near_dir);
}

void ported_view_set_viewport(const PortedViewport *v)
{
    viewport = *v;
}

static void draw_sky_ring(const SkyTargets *targets, const View *view, const TrackArt *art, const TrackInfo *info,
                          const TrackCamera *cam, int32_t yaw, int32_t roll)
{
    static SkyRing ring;
    sky_ring_build(&ring, info, &art->horizon);
    if (!ring.enabled) return;
    SkyFrame frame = { view, cam->matrix, { 0 }, yaw, roll, 0, SKY_WORLD_SHIFT, 0, { 0 },
                       { viewport.scale, viewport.width, viewport.sky_left, viewport.sky_top, viewport.sky_width,
                         viewport.sky_height }, WIDTH, WIDTH, 0 };
    for (int i = 0; i < 3; i++) frame.position[i] = -cam->position[i] >> SKY_POSITION_SHIFT;
    horizon_point(view, WORLD_SHIFT, yaw, cam->matrix, frame.horizon);
    const SkySink sink = { (void *)targets, sky_triangle, sky_fill };
    sky_ring_draw(&ring, info, &frame, &art->horizon, &sink);
}

void ported_view_render_from(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *pc,
                             const CarPose *pose, int32_t pose_node, uint8_t *pixels)
{
    const PortedCar one = { pose != NULL ? *pose : (CarPose){ { 0, 0, 0 }, 0, 0, 0, NULL, 1 },
                            pose != NULL ? pose_node : -1, NULL };
    ported_view_render_cars(tri, art, info, pc, &one, 1, pixels);
}

static TrackRender render;

const TrackRender *ported_view_stream(const Tri *tri, const TrackArt *art, int32_t camera_node)
{
    track_render_build(&render, tri, art, camera_node);
    return &render;
}

void ported_view_render_cars(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *pc,
                             const PortedCar *list, int count, uint8_t *pixels)
{
    static View view;
    static HorizonTables horizon;
    const PortedViewport *vp = &viewport;
    view_init(&view, vp->left, vp->top, vp->width, vp->height, vp->fov, vp->fov);
    horizon_tables(&horizon, vp->width, vp->fov, vp->scale);
    RasterTarget target = { pixels, WIDTH, HEIGHT, 0, 0 };
    QuadTarget quad = { pixels, WIDTH, vp->left, vp->top, vp->left + vp->width, vp->top + vp->height };
    DrawContext ctx = { &view, { vp->left, vp->top, vp->left + vp->width, vp->top + vp->height }, WORLD_SHIFT,
                        { 0, 0, 0x1999 },
                        { &quad, draw_uv, draw_full, draw_full_alt, draw_model, draw_cars, draw_skids }, 0 };
    TrackCamera cam = { { pc->position[0], pc->position[1], pc->position[2] }, { 0 }, pc->node };
    track_camera_matrix(pc->pitch, pc->yaw, pc->roll, 0, cam.matrix);
    /* 0044cc74 clears nothing (00473680 picks the surface): the horizon's
     * fills cover the view; the view (only: the rest of the surface holds
     * 004218d0's tiles and frame) cleared here for the frames without one. */
    for (int32_t y = vp->top < 0 ? 0 : vp->top; y < vp->top + vp->height && y < HEIGHT; y++) {
        const int32_t x0 = vp->left < 0 ? 0 : vp->left, x1 = vp->left + vp->width > WIDTH ? WIDTH : vp->left + vp->width;
        if (x1 > x0) memset(pixels + (size_t)y * WIDTH + x0, 0, (size_t)(x1 - x0));
    }
    if (art->has_horizon && info->ring != 0) {
        const SkyTargets targets = { &quad, &target };
        draw_sky_ring(&targets, &view, art, info, &cam, pc->yaw, pc->roll);
    } else if (art->has_horizon) {
        const HorizonTarget ht = { pixels, WIDTH };
        horizon_draw_flat(&horizon, &ht, &ctx.clip, 0, &art->horizon, pc->yaw, 0,
                          horizon_line(&view, WORLD_SHIFT, pc->yaw, cam.matrix));
    }
    track_render_build(&render, tri, art, pc->node);
    track_render_transform(&render, &cam, &ctx);
    memcpy(last_near_dir, ctx.near_dir, sizeof last_near_dir);
    last_near = ctx.near;
    model_view = (ModelView){ &view, cam.position, cam.matrix, 0 };
    car_count = count < 0 ? 0 : count > PORTED_VIEW_CARS ? PORTED_VIEW_CARS : count;
    for (int i = 0; i < car_count; i++) {
        cars[i].pose = list[i].pose;
        cars[i].car = list[i].car;
        cars[i].node = list[i].node;
    }
    static uint8_t shade[256];
    object_sink_shade(art->palette, shade);
    object_pixels = (ObjectSink){ &quad, art->palette, shade };
    skid_node = pc->node;
    track_render_draw(&render, &cam, &ctx, 0, 0);
}

void ported_view_render(const Tri *tri, const TrackArt *art, const TrackInfo *info, uint32_t node, int32_t yaw,
                        uint8_t *pixels)
{
    TriNode n, cn;
    if (tri_node(tri, node, &n) != TRI_OK) return;
    const PortedCamera cam = { { n.position.x, n.position.y + EYE_HEIGHT, n.position.z }, n.orientation * 0x400 + yaw,
                               0, (int32_t)node, 0 };
    const int32_t parked = (int32_t)((node + (uint32_t)car_ahead) & (tri->ring_nodes - 1u));
    if (cars[0].ok && tri_node(tri, (uint32_t)parked, &cn) == TRI_OK) {
        const CarPose pose = { { cn.position.x, cn.position.y, cn.position.z }, 0, 0, cn.orientation * 0x400, NULL, 1 };
        ported_view_render_from(tri, art, info, &cam, &pose, parked, pixels);
    } else {
        ported_view_render_from(tri, art, info, &cam, NULL, -1, pixels);
    }
}
