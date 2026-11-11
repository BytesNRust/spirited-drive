/* port (the cars on the road): how a car is drawn in the view, taken out of ported_view.c: the soft shadow and the lamp lights on the car, the name tags, the body's parts, the lamps and flares, each row's cars.
 * ported_view.c sets the cars up and calls row_prepare, row_draw and draw_cars; what this file takes of the view is in view_shared.h. */

#include "game/race/car_body_draw.h"
#include "game/race/light_tint.h"
#include "game/race/car_render.h"
#include "game/race/car_shadow.h"
#include "game/sys/port_settings.h"
#include "game/viewer/car_lamp_halo.h"
#include "game/viewer/follow_behind.h"
#include "game/viewer/view_lamps.h"
#include "viewer/body_shine.h"
#include "viewer/drive_lamps.h"
#include "viewer/drive_light.h"
#include "viewer/soft_shadow.h"
#include "viewer/view_spray.h"
#include "viewer/view_tags.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* port (car_soft_shadow): the soft shadow's flag and its half-way remap, made from the palette and the exe's shade table when either changes */
static int soft_shadow_on;

void ported_view_set_soft_shadow(int on) { soft_shadow_on = on != 0; }

static const uint8_t *soft_half(void)
{
    static uint8_t half[256], made_from[256], made_for[768];
    static int valid;
    if (!valid || memcmp(made_from, shadow_remap, sizeof made_from) != 0 || memcmp(made_for, object_pixels.palette, sizeof made_for) != 0) {
        soft_shadow_half(object_pixels.palette, shadow_remap, half);
        memcpy(made_from, shadow_remap, sizeof made_from), memcpy(made_for, object_pixels.palette, sizeof made_for);
        valid = 1;
    }
    return half;
}

/* port (lamp_shadow): the half-way remap made in the palette as it is shown (drive_light_palette), not the art's. The night palette leaves some entries (the lit board's cream) as they are while it darkens the rest,
 * so the entry nearest the half-way colour in the art's palette can be a bright one on screen: under a lamp, where the road's indexes are the bright ones, that made the shadow a flat cream box. */
static const uint8_t *soft_half_shown(void)
{
    static uint8_t half[256], made_from[256], made_for[768];
    static int valid;
    uint8_t shown[768];
    drive_light_palette(object_pixels.palette, shown);
    if (!valid || memcmp(made_from, shadow_remap, sizeof made_from) != 0 || memcmp(made_for, shown, sizeof made_for) != 0) {
        soft_shadow_half(shown, shadow_remap, half);
        memcpy(made_from, shadow_remap, sizeof made_from), memcpy(made_for, shown, sizeof made_for);
        valid = 1;
    }
    return half;
}

/* port (lamp_shadow): the car stands in a street lamp's light (the same test as the car's own lamp light, track_node_light_lamp_car) */
static int lamp_on_car(const Car *car)
{
    if (light_effects == NULL || light_effects->lamp_n <= 0) return 0;
    float lit[3] = { 1.0f, 1.0f, 1.0f };
    return track_node_light_lamp_car(light_effects, light_tint_ring_node(car->node, light_effects->nodes), car->x / 65536.0, car->z / 65536.0, lit);
}

/* port (tunnel look): the car stands in a tunnel whose look lights the road (a file's tunnel_tone): its road's indexes are the shown palette's bright ones too, so the half-way remap is made in the shown palette as lamp_shadow's is
 * (above full light the road was the palette's brightest entries and the shadow a white box) */
static int look_tunnel_on_car(const Car *car)
{
    if (light_effects == NULL) return 0;   /* the soft shadow needs no light (car_soft_shadow is its own switch): none loaded, no tunnel look */
    return track_node_light_look_tunnel(light_effects, light_tint_ring_node(car->node, light_effects->nodes));
}

/* 0045a724 */
void row_prepare(void *ctx, Car *car, int pass)
{
    (void)ctx;
    const CarShadowOps ops = { NULL, shadow_contact, shadow_followed, NULL, 0 };
    int32_t screen[8];
    if (soft_shadow_on && shadow_remap != NULL && object_pixels.palette != NULL) {   /* port: the quad's corners, the shade not drawn (remap NULL) */
        const CarShadowOps cut = { NULL, shadow_contact, shadow_followed, follow_arm_clip_quad, 1 };   /* and the quad cut at the near plane, not lost */
        if (car_prepare(&cut, car, &model_view, pass, car == followed_car(), object_pixels.target, NULL, screen))
            soft_shadow_draw(object_pixels.target, shadow_remap, lamp_on_car(car) || look_tunnel_on_car(car) ? soft_half_shown() : soft_half(), soft_shadow_level(track_node_light_node(light_effects, (uint32_t)car->node)), screen);
        return;
    }
    car_prepare(&ops, car, &model_view, pass, car == followed_car(), object_pixels.target, shadow_remap, screen);
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
    object3d_update_normals(body_model(ctx));   /* port (body shine): none kept unless the flag is on */
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

/* port (handbrake_lamps): the brake lamps are lit by the brake above LAMP_BRAKE_ON, but not by one that is the handbrake's alone (the rally's handbrake is a full brake, car_physics.c) */
static int brake_lamp(const Car *car)
{
    if (car->brake <= LAMP_BRAKE_ON) return 0;
    const int by_handbrake = shadow_race != NULL && car->index >= 0 && car->index < RACE_CARS && shadow_race->port_hb_lamp[car->index];
    return !(by_handbrake && port_handbrake_lamps());
}

/* port (emissive lamps): the car's lamp texels as the light at the car is (the day's, or glowing, lit by its brake), before it is drawn */
void ported_view_set_flares(int on) { view_lamps_set_flares(on); }

static void lamp_state(const Car *car)
{
    float ratio[3] = { 1, 1, 1 }, tint[3] = { 1, 1, 1 };
    ratio_of_car(car, ratio);
    drive_light_tint(tint);
    if (view_lamps_flares() && weather == WEATHER_FOG)   /* port (lamp_flares): the lamps glow in fog too */
        drive_lamps_frame(car->index, 1, brake_lamp(car), car->gear == LAMP_REVERSE_GEAR);
    else   /* port (rain_lamps): and in rain, day included: by day or dusk only the texels (force 2), no halo; at night as the light says */
        drive_lamps_light_forced(car->index, car->node, tint, ratio, brake_lamp(car), car->gear == LAMP_REVERSE_GEAR,
                                 weather_wet(weather) && port_rain_lamps() ? (drive_light_night() ? 1 : 2) : 0);
}

/* 00458d4c(car, 0051c774, +0x391) */
static unsigned body_draws;   /* port: the bodies drawn so far; a pass that drew none left no screen corners to flare from */
static void render_body(void *ctx, Car *car, const int32_t matrix[9], const int32_t position[3])
{
    BodyCtx *b = ctx;
    const RaceState *race = shadow_race;
    const CarBodyOps ops = { b, body_eye, body_matrix, body_transform, body_project, body_sort, body_draw, body_sort_list,
                             body_draw_list, body_wheels, body_steer, body_smoke, body_palette };
    const CarLightInputs in = { { race->stop_notice[0], race->stop_notice[1] },
                                light_slots != NULL ? light_slots : NO_SLOT_COLOURS, race->finish_cars, race->tick,
                                &flash_tick, &flash };
    lamp_state(car);
    body_draws++;
    if (car->index >= 0 && car->index < RACE_CARS) body_drawn_tick[car->index] = race->tick + 1;   /* port (headlight_beams): this car was drawn: its pool may light the road */
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
void render_point(void *ctx, Car *car, int32_t camera[3], int32_t screen[2])
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
    const CrashTrailView v = { mv->view, mv->camera_position, mv->camera_matrix, WORLD_SHIFT, NULL, view_spray_trail_puff };
    crash_trail_draw(puff_trail, offset, &v);
}

/* 00459064 */
static void render_tag(void *ctx, int32_t x, int32_t y, int32_t slot, int32_t z)
{
    view_tag_draw(((BodyCtx *)ctx)->target, x, y, slot, z);
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

static void draw_flare(const ViewCar *vc, const Car *car, const QuadTarget *t, const uint8_t palette[768])
{
    const Object3D *body = car->far != 0 ? vc->model.far : vc->model.body;   /* the model this pass drew: the other one's screen corners are another view's */
    if (!view_lamps_flares()) return;
    car_lamp_halo_draw(body, car, shadow_race, police_flash_on, t, palette);
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
void row_draw(void *ctx, Car *car, int pass)
{
    (void)ctx;
    const Object3DSink sink = object_sink(&object_pixels);
    for (int i = 0; i < car_count; i++)
        if (cars[i].ok && cars[i].car == car) {
            object_sink_set_light(car_table(car, object_pixels.palette));
            body_shine_begin(object_pixels.palette, model_view.camera_matrix);
            const unsigned drawn = body_draws;
            render_slot(&cars[i], car, pass, &model_view, &sink, object_pixels.target);
            body_shine_end();
            if (body_draws != drawn) draw_flare(&cars[i], car, object_pixels.target, object_pixels.palette);
            object_sink_set_light(NULL);
        }
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
    object_sink_set_light(car_table(car, palette));
    body_shine_begin(palette, mv->camera_matrix);
    const unsigned drawn = body_draws;
    render_slot(&cars[slot], car, mirror, mv, &sink, target);
    body_shine_end();
    if (body_draws != drawn) draw_flare(&cars[slot], car, target, palette);
    object_sink_set_light(NULL);
    return 1;
}

/* 0045a768 */
static void row_unseen(void *ctx, Car *car)
{
    (void)ctx;
    const CarShadowOps ops = { NULL, shadow_contact, shadow_followed, NULL, 0 };
    car_skid_unseen(&ops, car);
}

/* 0045a848 in a race; else each listed car on the node. */
void draw_cars(void *user, int32_t node)
{
    (void)user;
    if (shadow_race != NULL && shadow_remap != NULL) {
        const RaceState *race = shadow_race;
        const RowCarsOps ops = { NULL, row_prepare, row_draw, row_unseen };
        const RowCarsInputs in = { race->chase.node, race->segments, ahead_rows, race->drawn, race->drawn_count, followed_car(),
                                   race->chase.kind, 0 };
        view_spray_row(node, 1);   /* the spray and the dust farther off than their car, first */
        cars_on_row(&ops, &in, node);
        if (arm_drawing) {   /* port: the rows behind the camera's node, which the exe's pass reads as the end of the ring */
            const RowCarsBehindInputs back = { race->chase.node, race->segments, behind_rows, race->drawn, race->drawn_count, followed_car() };
            cars_on_row_behind(&ops, &back, node);
        }
        view_spray_row(node, 0);   /* and the nearer, after the cars */
        return;
    }
    const Object3DSink sink = object_sink(&object_pixels);
    for (int i = 0; i < car_count; i++)
        if (cars[i].ok && cars[i].node == node) {
            object_sink_set_light(car_light(node, NULL, -1, object_pixels.palette));
            car_draw(&cars[i].model, &cars[i].pose, &model_view, 0, &sink);
            object_sink_set_light(NULL);
        }
}
