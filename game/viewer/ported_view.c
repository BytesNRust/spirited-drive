#include "viewer/ported_view.h"

#include "game/gfx/quad.h"
#include "game/race/horizon.h"
#include "game/race/sky_ring.h"
#include "game/race/track_render.h"
#include "game/race/view.h"
#include "game/race/car_draw.h"
#include "game/race/model_draw.h"
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

static void draw_cars(void *user, int32_t node)
{
    (void)user;
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
static void draw_sky_ring(const SkyTargets *targets, const View *view, const TrackArt *art, const TrackInfo *info,
                          const TrackCamera *cam, int32_t yaw)
{
    static SkyRing ring;
    sky_ring_build(&ring, info, &art->horizon);
    if (!ring.enabled) return;
    SkyFrame frame = { view, cam->matrix, { 0 }, yaw, 0, 0, SKY_WORLD_SHIFT, 0, { 0 },
                       { 0x10000, WIDTH, 0, 0, WIDTH, HEIGHT }, WIDTH, WIDTH, 0 };
    for (int i = 0; i < 3; i++) frame.position[i] = -cam->position[i] >> SKY_POSITION_SHIFT;
    horizon_point(view, WORLD_SHIFT, yaw, cam->matrix, frame.horizon);
    const SkySink sink = { (void *)targets, sky_triangle, sky_fill };
    sky_ring_draw(&ring, info, &frame, &art->horizon, &sink);
}

void ported_view_render_from(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *pc,
                             const CarPose *pose, int32_t pose_node, uint8_t *pixels)
{
    const PortedCar one = { pose != NULL ? *pose : (CarPose){ { 0, 0, 0 }, 0, 0, 0, NULL, 1 },
                            pose != NULL ? pose_node : -1 };
    ported_view_render_cars(tri, art, info, pc, &one, 1, pixels);
}

void ported_view_render_cars(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *pc,
                             const PortedCar *list, int count, uint8_t *pixels)
{
    static TrackRender render;
    static View view;
    static HorizonTables horizon;
    view_init(&view, 0, 0, WIDTH, HEIGHT, FOV, FOV);
    horizon_tables(&horizon, WIDTH, FOV, 0x10000);
    RasterTarget target = { pixels, WIDTH, HEIGHT, 0, 0 };
    QuadTarget quad = { pixels, WIDTH, 0, 0, WIDTH, HEIGHT };
    DrawContext ctx = { &view, { 0, 0, WIDTH, HEIGHT }, WORLD_SHIFT, { 0, 0, 0x1999 },
                        { &quad, draw_uv, draw_full, draw_full_alt, draw_model, draw_cars } };
    TrackCamera cam = { { pc->position[0], pc->position[1], pc->position[2] }, { 0 }, pc->node };
    track_camera_matrix(pc->pitch, pc->yaw, pc->roll, 0, cam.matrix);
    /* TODO(00473680): the frame is cleared first; colour 0 for now. */
    memset(pixels, 0, WIDTH * HEIGHT);
    if (art->has_horizon && info->ring != 0) {
        const SkyTargets targets = { &quad, &target };
        draw_sky_ring(&targets, &view, art, info, &cam, pc->yaw);
    } else if (art->has_horizon) {
        const HorizonTarget ht = { pixels, WIDTH };
        horizon_draw_flat(&horizon, &ht, &ctx.clip, 0, &art->horizon, pc->yaw, 0,
                          horizon_line(&view, WORLD_SHIFT, pc->yaw, cam.matrix));
    }
    track_render_build(&render, tri, art, pc->node);
    track_render_transform(&render, &cam, &ctx);
    model_view = (ModelView){ &view, cam.position, cam.matrix, 0 };
    car_count = count < 0 ? 0 : count > PORTED_VIEW_CARS ? PORTED_VIEW_CARS : count;
    for (int i = 0; i < car_count; i++) {
        cars[i].pose = list[i].pose;
        cars[i].node = list[i].node;
    }
    static uint8_t shade[256];
    object_sink_shade(art->palette, shade);
    object_pixels = (ObjectSink){ &quad, art->palette, shade };
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
