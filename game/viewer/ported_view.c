#include "viewer/ported_view.h"
#include "game/sys/bands.h"
#include "game/gfx/raster_record.h"

#include "game/gfx/quad.h"
#include "game/race/horizon.h"
#include "game/race/sky_ring.h"
#include "game/race/track_render.h"
#include "game/gfx/sprite_scaled.h"
#include "game/race/view.h"
#include "game/viewer/screen_layout.h"
#include "game/race/car_draw.h"
#include "game/race/car_road.h"
#include "game/race/car_shadow.h"
#include "game/race/car_tag.h"
#include "game/race/car_lights.h"
#include "game/race/car_body_draw.h"
#include "game/race/car_render.h"
#include "game/gfx/x86_int.h"
#include "game/race/car_wheels.h"
#include "game/gfx/flat.h"
#include "game/race/model_draw.h"
#include "game/race/row_cars.h"
#include "game/race/skid_marks.h"
#include "game/race/tyre_smoke.h"
#include "game/viewer/follow_arm.h"
#include "game/viewer/follow_behind.h"
#include "viewer/horizon_smooth.h"
#include "viewer/modern_look.h"
#include "viewer/modern_road.h"
#include "viewer/object_sink.h"
#include "viewer/raster.h"
#include "viewer/sky_smooth.h"

#include <math.h>
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

static int modern;  /* port: EXTRAS' Modern 3D (ported_view_set_modern) */

void ported_view_set_modern(int on) { modern = on != 0; }
int ported_view_modern(void) { return modern; }

static int look_filter, look_fog;  /* port: EXTRAS' Texture Filter and Fog (ported_view_set_look) */
static ModernLook look;
static HorizonSmooth smooth;       /* port: the Texture Filter's table for the sky's palette (horizon_smooth.h) */

void ported_view_set_look(int filter, int fog) { look_filter = filter != 0, look_fog = fog != 0; }
int ported_view_filter_on(void) { return modern && look_filter; }
int ported_view_fog_on(void) { return modern && look_fog; }

/* port: the view the 3D objects' pixels clip and project by when Modern 3D is on, else NULL (the exe's mappers) */
static const View *modern_view(const View *view) { return modern ? view : NULL; }

/* port: the near road clipped, unsnapped and perspective-correct (Modern 3D) */
static void draw_camera(void *user, const View *view, const FshEntry *texture, const int32_t uv[8],
                        const int32_t *const cam[4])
{
    modern_road_cell(user, view, texture, uv, cam, transparent(texture));
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
static int models_mirrored;

static void draw_model(void *user, int model, const int32_t position[3], uint32_t heading)
{
    (void)user;
    if (model < 0 || model >= PORTED_VIEW_MODELS || !object_ok[model]) return;
    const Object3DSink sink = object_sink(&object_pixels);
    model_draw_object(&objects[model], position, (int32_t)heading, &model_view, &sink);
}

void ported_view_set_models(const ViewModel *list, const int *ok, int mirrored)
{
    if (list == models && mirrored == models_mirrored) return;
    for (int i = 0; i < PORTED_VIEW_MODELS; i++) {
        if (object_ok[i]) object3d_free(&objects[i]);
        object_ok[i] = list != NULL && ok[i] && object3d_create(&objects[i], &list[i].mesh);
        if (object_ok[i] && list[i].has_textures) object3d_bind(&objects[i], &list[i].textures);
        if (object_ok[i] && mirrored) object3d_mirror(&objects[i]);   /* 004576f0 on 004fbad8 */
    }
    models = list, models_mirrored = mirrored;
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

Object3D *ported_view_car_object(int slot)
{
    return slot >= 0 && slot < PORTED_VIEW_CARS && cars[slot].ok ? cars[slot].high : NULL;
}

void ported_view_set_car(const ViewModel *high, const ViewModel *low, int ahead)
{
    ported_view_set_car_slot(0, high, low);
    car_ahead = ahead;
}

static RaceState *shadow_race;
static const uint8_t *shadow_remap;

void ported_view_set_shadows(RaceState *race, const uint8_t *remap)
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

SkidMarks *ported_view_skids(void)
{
    return &skid_marks;
}
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

/* port: the mirror's rows again for the camera's node of this frame, the cars not moved on (mirror_car_rows_at) */
void ported_view_mirror_rows_again(uint8_t behind[MIRROR_CAR_ROWS])
{
    if (shadow_race == NULL) return;
    mirror_car_rows_at(shadow_race->cars, shadow_race->car_count, shadow_race->segments, shadow_race->chase.node, behind);
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


static int32_t last_near_dir[3];
static int32_t last_near;

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
    flat_polygon(ctx, colour, count, points);
}

static void tag_line(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t colour)
{
    flat_line(ctx, x0, y0, x1, y1, colour);
}

/* 004591d4 / 00458d4c on the slot's CarModel: car +0x52e / +0x666 hold no handles in the port, so the model is the
 * far one while +0x78e is set, and the parts come in 00458d4c's order (inside, surface, outside, convex, those below
 * 8), as CarModel keeps its sections. */
typedef struct {
    ViewCar *vc;
    Car *car;
    const Object3DSink *sink;
    const ModelView *mv;        /* the main view's, or the mirror's (0040586c sets 00511fe0.. for either) */
    const QuadTarget *target;   /* where the marks go */
    int32_t eye[3];
    int part;
} BodyCtx;

static const int32_t IDENTITY[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };

static Object3D *body_model(const BodyCtx *b) { return b->car->far != 0 ? b->vc->model.far : b->vc->model.body; }

static void body_eye(void *ctx, const int32_t eye[3]) { memcpy(((BodyCtx *)ctx)->eye, eye, sizeof ((BodyCtx *)ctx)->eye); }
static void body_matrix(void *ctx, uint32_t object, const void *m) { (void)object, object3d_set_matrix(body_model(ctx), m); }
static void body_transform(void *ctx, uint32_t object)
{
    (void)object, object3d_transform(body_model(ctx), IDENTITY, ((BodyCtx *)ctx)->eye, 0);
}
static void body_project(void *ctx, uint32_t object) { (void)object, object3d_project(body_model(ctx), ((BodyCtx *)ctx)->mv->view); }
static void body_sort(void *ctx, uint32_t object) { (void)object, object3d_sort(body_model(ctx)); }
static void body_draw(void *ctx, uint32_t object)
{
    (void)object, object3d_draw(body_model(ctx), MODEL_NEAR_Z, ((BodyCtx *)ctx)->sink);
}

static int32_t part_of(const Car *car, int k)
{
    const int32_t parts[CAR_SECTIONS] = { car->inside_part, car->surface_part, car->outside_part, car->convex_part };
    return parts[k];
}

/* the next part below 8 from b->part on (004918e1 and 00491c8a come in pairs) */
static Object3DList *body_list(BodyCtx *b)
{
    while (b->part < CAR_SECTIONS && (uint32_t)part_of(b->car, b->part) >= 8) b->part++;
    return b->part < CAR_SECTIONS ? &b->vc->model.sections[b->part] : NULL;
}
static void body_sort_list(void *ctx, uint32_t object, int32_t count, uint32_t list)
{
    (void)object, (void)count, (void)list;
    Object3DList *l = body_list(ctx);
    if (l != NULL && l->order != NULL) object3d_sort_list(body_model(ctx), l);
}
static void body_draw_list(void *ctx, uint32_t object, int32_t count, uint32_t list)
{
    (void)object, (void)count, (void)list;
    BodyCtx *b = ctx;
    const Object3DList *l = body_list(b);
    if (l != NULL && l->order != NULL) object3d_draw_list(body_model(b), l, MODEL_NEAR_Z, b->sink);
    b->part++;
}

/* 00458614 / 00458818: the viewer's hooks (track_viewer.c keeps when the wheels turn). */
static void (*wheel_frames_hook)(void *ctx, Car *car);
static void (*steer_hook)(void *ctx, Car *car);
static void *wheel_ctx;

void ported_view_set_wheels(void (*frames)(void *ctx, Car *car), void (*steer)(void *ctx, Car *car), void *ctx)
{
    wheel_frames_hook = frames, steer_hook = steer, wheel_ctx = ctx;
}

static void body_wheels(void *ctx, Car *car) { (void)ctx; if (wheel_frames_hook != NULL) wheel_frames_hook(wheel_ctx, car); }
static void body_steer(void *ctx, Car *car) { (void)ctx; if (steer_hook != NULL) steer_hook(wheel_ctx, car); }

static void body_smoke(void *ctx, Car *car, int32_t in_car)
{
    (void)ctx;
    if (puff_art != NULL && puff_blend != NULL && puff_rand != NULL) smoke_draw(car, in_car);
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

static void body_palette(void *ctx, int32_t first, int32_t count, const uint8_t *colours)
{
    (void)ctx;
    if (light_write != NULL) light_write(light_ctx, first, count, colours);
}

static const uint8_t NO_SLOT_COLOURS[RACE_CARS + 1];

/* 00458d4c(car, 0051c774, +0x391) */
static void render_body(void *ctx, Car *car, const int32_t matrix[9], const int32_t position[3])
{
    BodyCtx *b = ctx;
    const RaceState *race = shadow_race;
    const CarBodyOps ops = { b, body_eye, body_matrix, body_transform, body_project, body_sort, body_draw, body_sort_list,
                             body_draw_list, body_wheels, body_steer, body_smoke, body_palette };
    const CarLightInputs in = { { race->stop_notice[0], race->stop_notice[1] },
                                light_slots != NULL ? light_slots : NO_SLOT_COLOURS, race->finish_cars, race->tick,
                                &flash_tick, &flash };
    b->part = 0;
    car_body_draw(&ops, &in, car, followed_car(), matrix, position);
}

/* the object's +0x10 bit 1, on both models (the port's handles do not tell them apart) */
static void render_near(void *ctx, uint32_t object, int on)
{
    (void)object;
    const BodyCtx *b = ctx;
    Object3D *models[2] = { b->vc->model.body, b->vc->model.far };
    for (int i = 0; i < 2; i++) {
        if (models[i] == NULL) continue;
        if (on) models[i]->flags |= OBJECT3D_FLAG_NEAR;
        else models[i]->flags &= ~(uint32_t)OBJECT3D_FLAG_NEAR;
    }
}

/* the near model's projected point (its +0x7c) */
static void render_vertex(void *ctx, uint32_t object, int32_t index, int32_t out[2])
{
    (void)object;
    const Object3D *body = ((BodyCtx *)ctx)->vc->model.body;
    const int in_range = index >= 0 && (uint32_t)index < body->vertex_count;
    out[0] = in_range ? body->screen[index][0] : 0;
    out[1] = in_range ? body->screen[index][1] : 0;
}

/* 0044a1c4 with the pass's camera; the near plane is the main view's last (the mirror's own is not kept here) */
static void render_point(void *ctx, Car *car, int32_t camera[3], int32_t screen[2])
{
    const ModelView *mv = ((BodyCtx *)ctx)->mv;
    TrackCamera cam = { { 0 }, { 0 }, 0 };
    memcpy(cam.position, mv->camera_position, sizeof cam.position);
    memcpy(cam.matrix, mv->camera_matrix, sizeof cam.matrix);
    DrawContext dc = { 0 };
    dc.view = mv->view, dc.world_shift = WORLD_SHIFT;
    memcpy(dc.near_dir, last_near_dir, sizeof dc.near_dir);
    dc.near = last_near;
    const int32_t place[3] = { car->x, car->y, car->z };
    track_point_screen(&cam, &dc, place, camera, screen);
}

/* 00456f34(005146ac, offset) */
static void render_trail(void *ctx, const int32_t offset[2])
{
    const ModelView *mv = ((BodyCtx *)ctx)->mv;
    if (puff_trail == NULL || puff_art == NULL || puff_art->smok == NULL) return;
    const CrashTrailView v = { mv->view, mv->camera_position, mv->camera_matrix, WORLD_SHIFT, NULL, trail_puff };
    crash_trail_draw(puff_trail, offset, &v);
}

static uint8_t tags_hidden;  /* 004fb78b */

void ported_view_set_tags_hidden(int hidden)
{
    tags_hidden = (uint8_t)hidden;
}

/* 00459064 */
static void render_tag(void *ctx, int32_t x, int32_t y, int32_t slot, int32_t z)
{
    if (tag_colours == NULL || slot < 0 || slot >= tag_colour_count) return;
    const CarTagOps ops = { (void *)((BodyCtx *)ctx)->target, tag_polygon, tag_line };
    const CarTagView v = { tag_colours, tag_outline, SKID_HIGH_RES, shadow_race->humans, tags_hidden };
    car_tag(&ops, &v, x, y, slot, z);
}

/* 0051c774 */
static int32_t car_world[9];

/* port: how far the car's model reaches from its centre (car_render.h's port_reach) */
static int32_t car_reach(const Object3D *o)
{
    int64_t most = 0;
    for (uint32_t v = 0; v < o->vertex_count; v++) {
        const int64_t x = o->model[v][0], y = o->model[v][1], z = o->model[v][2];
        if (x * x + y * y + z * z > most) most = x * x + y * y + z * z;
    }
    return (int32_t)sqrt((double)most) + 1;
}

static void render_slot(ViewCar *vc, Car *car, int pass, const ModelView *mv, const Object3DSink *sink,
                        const QuadTarget *target)
{
    BodyCtx b = { vc, car, sink, mv, target, { 0, 0, 0 }, 0 };
    const RaceState *race = shadow_race;
    const CarRenderOps ops = { &b, render_body, render_near, render_vertex, render_point, render_trail, render_tag };
    const CarRenderInputs in = { mv->camera_matrix, race->player, race->replay, race->race_type, race->racers, SKID_HIGH_RES,
                                 mv->port_all_sides ? car_reach(vc->model.body) : 0 };
    car_render(&ops, &in, car, pass, car_world);
}

/* 004591d4 */
static void row_draw(void *ctx, Car *car, int pass)
{
    (void)ctx;
    const Object3DSink sink = object_sink(&object_pixels);
    for (int i = 0; i < car_count; i++)
        if (cars[i].ok && cars[i].car == car) render_slot(&cars[i], car, pass, &model_view, &sink, object_pixels.target);
}

int ported_view_render_car(Car *car, const ModelView *mv, int mirror, const QuadTarget *target, const uint8_t palette[768])
{
    if (shadow_race == NULL) return 0;
    static uint8_t shade[256];
    static const uint8_t *shaded;
    if (shadow_remap == NULL && shaded != palette) object_sink_shade(palette, shade), shaded = palette;
    ObjectSink pixels = { target, palette, shadow_remap != NULL ? shadow_remap : shade, modern_view(mv->view) };
    const Object3DSink sink = object_sink(&pixels);
    const int32_t slot = car->index;  /* the slots are the race's (ported_view_set_car_slot) */
    if (slot < 0 || slot >= PORTED_VIEW_CARS || !cars[slot].ok) return 0;
    render_slot(&cars[slot], car, mirror, mv, &sink, target);
    return 1;
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
    const QuadTarget *quad;        /* the box (a band's rows of it when banded) */
    const RasterTarget *raster;
    const HorizonSmooth *smooth;   /* port: EXTRAS' Texture Filter on the ring's slices (sky_smooth.h), NULL off */
    const QuadTarget *full;        /* port (render bands): the whole box, for the draws whose edges a narrowed clip would move */
    int banded, top, bottom;       /* and, when banded, the rows this band stores */
} SkyTargets;

/* 004f9e98 = 00425520 -> 004252ec */
static void sky_triangle(void *user, const FshEntry *texture, const int32_t uv[6], const int32_t screen[6])
{
    const SkyTargets *t = user;
    if (t->smooth != NULL && !transparent(texture)) sky_smooth_triangle(t->smooth, t->quad, texture, uv, screen);
    else tri_draw(t->quad, texture, uv, screen, transparent(texture));
}

static void sky_fill(void *user, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t colour)
{
    const SkyTargets *t = user;
    if (t->banded) {   /* a rectangle's rows: the band's part of it */
        const int64_t y0 = y < t->top ? t->top : y, y1 = (int64_t)y + h > t->bottom ? t->bottom : (int64_t)y + h;
        if (y1 <= y0) return;
        y = (int32_t)y0, h = (int32_t)(y1 - y0);
    }
    raster_fill(t->raster, x, y, w, h, colour);
}

/* 00484389 */
static void sky_polygon(void *user, uint8_t colour, const int32_t points[8])
{
    const SkyTargets *t = user;
    if (t->banded) flat_polygon_rows(t->full, colour, 4, points, t->top, t->bottom);
    else flat_polygon(t->quad, colour, 4, points);
}

static int sky_off;  /* 004fb786 */

/* 004fbad8, the race's: the "m" tracks drawn turned over (004496ac, 00449f0c, 0043ae64, 0043b298) */
static int view_mirrored(void) { return shadow_race != NULL && shadow_race->mirrored != 0; }

void ported_view_set_sky_off(int off)
{
    sky_off = off;
}

static int32_t anim_tick;  /* 00513e70 */

void ported_view_set_tick(int32_t tick)
{
    anim_tick = tick;
}

/* 0043af80: the 3D sky ring of the tracks whose INFO asks for one. */
/* 0043af80's ring path: 0043b298 with the camera's yaw and roll (the follow
 * record's +0x44, +0x48); 0043b0c8 slopes the fills with the roll. */
static PortedViewport viewport = { 0, 0, WIDTH, HEIGHT, FOV, 0x10000, 0, 0, WIDTH, HEIGHT, 0, 0, 1 };

/* port: the frame the 3D view draws into, 640 x 480 but on a wide screen (ported_view_set_screen) */
static int32_t screen_w = WIDTH, screen_h = HEIGHT;

void ported_view_set_screen(int width, int height)
{
    screen_w = width, screen_h = height;
}

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

static SkyRing sky_ring;   /* built once a frame by the caller, read by every band */

static void draw_sky_ring(const SkyTargets *targets, const View *view, const TrackArt *art, const TrackInfo *info,
                          const TrackCamera *cam, int32_t yaw, int32_t roll)
{
    const SkyRing *ring = &sky_ring;
    const int32_t rs = viewport.render_scale > 0 ? viewport.render_scale : 1;
    SkyFrame frame = { view, cam->matrix, { 0 }, yaw, roll, view_mirrored(), SKY_WORLD_SHIFT, 0, { 0 },
                       { viewport.scale, viewport.width, viewport.sky_left, viewport.sky_top, viewport.sky_width,
                         viewport.sky_height }, screen_w / rs, screen_w, 0, sky_off, 1, rs };   /* port: wide fields, render scale */
    for (int i = 0; i < 3; i++) frame.position[i] = -cam->position[i] >> SKY_POSITION_SHIFT;
    horizon_point(view, WORLD_SHIFT, yaw, cam->matrix, frame.horizon);
    const SkySink sink = { (void *)targets, sky_triangle, sky_fill, sky_polygon };
    sky_ring_draw(ring, info, &frame, &art->horizon, &sink);
}

/* port (render bands): the view's clear and the sky, one band's rows of them (everything they read is built before) */
typedef struct {
    uint8_t *pixels;
    const QuadTarget *quad;
    const RasterTarget *target;
    const View *view;
    const HorizonTables *horizon;
    const ClipRect *clip;
    const TrackArt *art;
    const TrackInfo *info;
    const TrackCamera *cam;
    const PortedCamera *pc;
    const HorizonSmooth *smooth;
    int32_t box_left, box_right, line;
    int ring, flat, banded;
} SkyStage;

static void sky_stage_band(void *user, int top, int bottom)
{
    const SkyStage *st = user;
    for (int32_t y = top; y < bottom; y++) {   /* 0044cc74 clears nothing; the view only, for the frames without one */
        const int32_t x0 = st->box_left < 0 ? 0 : st->box_left, x1 = st->box_right > screen_w ? screen_w : st->box_right;
        if (x1 > x0) memset(st->pixels + (size_t)y * screen_w + x0, 0, (size_t)(x1 - x0));
    }
    QuadTarget quad = *st->quad;   /* the box's rows this band owns */
    ClipRect clip = *st->clip;
    if (st->banded) {
        if (top > quad.clip_top) quad.clip_top = top, clip.y0 = top;
        if (bottom < quad.clip_bottom) quad.clip_bottom = bottom, clip.y1 = bottom;
        if (quad.clip_top >= quad.clip_bottom) return;
    }
    if (st->ring) {  /* the circuits' 3D ring */
        const SkyTargets targets = { &quad, st->target, st->smooth, st->quad, st->banded, top, bottom };
        draw_sky_ring(&targets, st->view, st->art, st->info, st->cam, st->pc->yaw, st->pc->roll);
    } else if (st->flat) {
        const HorizonTarget ht = { st->pixels, screen_w };
        if (st->smooth != NULL) {  /* the magnified flat sky blended */
            horizon_draw_flat_smooth(st->smooth, st->horizon, &ht, &clip, &st->art->horizon, st->pc->yaw, view_mirrored(),
                                     sky_off, st->line);
        } else if (st->banded) {  /* the box is the whole clip's, only the stores are the band's */
            horizon_draw_flat_rows(st->horizon, &ht, st->clip, &st->art->horizon, st->pc->yaw, view_mirrored(), sky_off,
                                   st->line, top, bottom);
        } else {
            horizon_draw_flat(st->horizon, &ht, &clip, &st->art->horizon, st->pc->yaw, view_mirrored(), sky_off, st->line);
        }
    }
}

void ported_view_render_from(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *pc,
                             const CarPose *pose, int32_t pose_node, uint8_t *pixels)
{
    const PortedCar one = { pose != NULL ? *pose : (CarPose){ { 0, 0, 0 }, 0, 0, 0, NULL, 1 },
                            pose != NULL ? pose_node : -1, NULL };
    ported_view_render_cars(tri, art, info, pc, &one, 1, pixels);
}

static TrackRender render;

/* 0043f540 on the race's scenery: the knocked-over bytes are the race's (004b9098, 0040fc64 sets them), and a knock's
 * 0043f51c (00504218 = -1) streams the scenery again at once; 0043f48c's 00504218 goes back to the race. */
static void stream(const Tri *tri, const TrackArt *art, int32_t camera_node)
{
    if (shadow_race != NULL && render.tri == tri) {
        render.build.knocked = shadow_race->scenery_state;
        if (shadow_race->scenery_stream_node < 0) render.build.first_node = -1;
    }
    track_render_build(&render, tri, art, camera_node);
    if (shadow_race != NULL) {
        render.build.knocked = shadow_race->scenery_state;  /* again, for a first build that bound the track */
        shadow_race->scenery_stream_node = render.build.first_node;
    }
}

void ported_view_forget_track(void)
{
    render.tri = NULL;
    ported_view_set_models(NULL, NULL, 0);  /* the next track's models sit at the same address, bound to these textures */
    horizon_smooth_cache_reset();           /* port: the sky bands kept for this track's bitmap */
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

void ported_view_render_cars(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *pc,
                             const PortedCar *list, int count, uint8_t *pixels)
{
    static View view;
    static HorizonTables horizon;
    const PortedViewport *vp = &viewport;
    view_init(&view, vp->left, vp->top, vp->width, vp->height, vp->fov, vp->fov_y != 0 ? vp->fov_y : vp->fov);
    /* port: the exe's rows scale grows with the width (0043a028); past 640 it keeps the 640's zoom */
    const int32_t rs = vp->render_scale > 0 ? vp->render_scale : 1;   /* port (render scale): the zoom of the unscaled width */
    horizon_tables(&horizon, vp->width, vp->fov, screen_horizon_scale(vp->scale, vp->width / rs));
    RasterTarget target = { pixels, screen_w, screen_h, 0, 0 };
    /* the box (00449f50: the layout's left, top, +0x20, +0x24); left + x, top + y only place the projection */
    const int32_t box[4] = { vp->sky_left, vp->sky_top, vp->sky_left + vp->sky_width, vp->sky_top + vp->sky_height };
    QuadTarget quad = { pixels, screen_w, box[0], box[1], box[2], box[3] };
    DrawContext ctx = { &view, { box[0], box[1], box[2], box[3] }, WORLD_SHIFT,
                        { 0, 0, 0x1999 },
                        { &quad, draw_uv, draw_full, draw_full_alt, draw_model, draw_cars, draw_skids,
                          modern ? draw_camera : NULL, draw_camera }, 0,
                        vp->near_scale };
    TrackCamera cam = { { pc->position[0], pc->position[1], pc->position[2] }, { 0 }, pc->node };
    track_camera_matrix(pc->pitch, pc->yaw, pc->roll, view_mirrored(), vp->scale, cam.matrix);  /* the layout's +4 (00449670) */
    /* 0044cc74 clears nothing (00473680 picks the surface): the horizon's
     * fills cover the view; the view (only: the rest of the surface holds
     * 004218d0's tiles and frame) cleared here for the frames without one. */
    const int sky_filter = art->has_horizon && modern && look_filter;  /* port: EXTRAS' Texture Filter on the sky */
    if (sky_filter) horizon_smooth_palette(&smooth, art->palette);
    const int ring = art->has_horizon && info->ring != 0;
    if (ring) sky_ring_build(&sky_ring, info, &art->horizon);
    SkyStage stage = { pixels, &quad, &target, &view, &horizon, &ctx.clip, art, info, &cam, pc, sky_filter ? &smooth : NULL,
                       box[0], box[2], 0, ring && sky_ring.enabled, art->has_horizon && !ring, bands_threads() != 0 };
    if (stage.flat) stage.line = horizon_line(&view, WORLD_SHIFT, pc->yaw, cam.matrix);
    bands_rows(box[1] < 0 ? 0 : box[1], box[3] < screen_h ? box[3] : screen_h, sky_stage_band, &stage);
    stream(tri, art, pc->node);
    modern_road_set_style(modern ? modern_look_style(&look, art, look_filter, look_fog, render.draw_segments) : NULL);
    track_render_transform(&render, &cam, &ctx);
    memcpy(last_near_dir, ctx.near_dir, sizeof last_near_dir);
    last_near = ctx.near;
    model_view = (ModelView){ &view, cam.position, cam.matrix,
                              shadow_race != NULL && model_draw_half_side(shadow_race->track, shadow_race->track_variant), 1 };
    car_count = count < 0 ? 0 : count > PORTED_VIEW_CARS ? PORTED_VIEW_CARS : count;
    for (int i = 0; i < car_count; i++) {
        cars[i].pose = list[i].pose;
        cars[i].car = list[i].car;
        cars[i].node = list[i].node;
    }
    static uint8_t shade[256];
    if (shadow_remap == NULL) object_sink_shade(art->palette, shade);
    object_pixels = (ObjectSink){ &quad, art->palette, shadow_remap != NULL ? shadow_remap : shade, modern_view(&view) };
    skid_node = pc->node;
    /* port (render bands P2, P3): with threads the track's draws (road, scenery, cars, shadows, skid marks, smoke) are
     * recorded by the rasterizers, which find the recorder in the target, and replayed by bands at the end, in order */
    static struct RasterRecorder recorder;
    if (bands_threads() != 0) {
        raster_record_set_runner(bands_rows);
        raster_record_begin(&recorder, pixels);
    }
    int ahead = 1;   /* the exe's pass draws the track */
    if (shadow_race != NULL && shadow_race->port_follow_camera) {   /* port: the camera may look along the track either way */
        follow_arm_window(&render, FOLLOW_ARM_SEGMENTS);
        ahead = follow_off_road(pc) <= FOLLOW_EXE_BELOW;
        if (!ahead) {
            const FollowArm arm = { &render, &cam, &ctx, tri_is_circuit(tri), anim_tick, view_mirrored(), 0 };
            follow_behind_draw(&arm, FOLLOW_ARM_SEGMENTS, render.draw_segments - 2);
        }
    }
    if (ahead) track_render_draw(&render, &cam, &ctx, view_mirrored(), anim_tick);
    if (shadow_race != NULL && shadow_race->port_follow_camera) draw_followed_car(pc, ahead);
    if (raster_active == &recorder) raster_record_end(&recorder);
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
