/* `tnfs --track NAME`: development fly-through of a track (simdata/misc/NAME.TRI
 * with its simdata/ntrackfm/NAME_[TMR]01.FAM textures). Up/Down move along the
 * road, Left/Right turn the camera, Space switches between the ported
 * renderer and the earlier stand-in (which also draws the scenery; with
 * --screenshot, TNFS_STAND_IN=1 picks it). */
#include "viewer/track_viewer.h"

#include "formats/chunk.h"
#include "formats/tri.h"
#include "game/race/car_load.h"
#include "game/race/car_model.h"
#include "game/race/car_wheels.h"
#include "game/race/race_chase.h"
#include "game/race/race_models.h"
#include "game/race/race_sim.h"
#include "game/race/track_art.h"
#include "game/race/track_geometry.h"
#include "game/race/track_info.h"
#include "game/race/track_objects.h"
#include "platform/platform.h"
#include "viewer/camera.h"
#include "game/race/race_view.h"
#include "viewer/drive_hud.h"
#include "viewer/drive_palette.h"
#include "viewer/model_view.h"
#include "viewer/ported_view.h"
#include "viewer/raster.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SCREEN_W = 640,
    SCREEN_H = 480,
    DRAW_SEGMENTS = 40,   /* segments ahead of the camera */
    STRIPS = 10,          /* 5 on each side of the centre */
    SKY_COLOUR = 0,
    TRACK_MODELS = 64      /* 005150f0, 0x138 bytes each */
};

static const float UNIT = 65536.0f;       /* node positions are 16.16 */
static const float EYE_HEIGHT = 1.5f;

typedef struct {
    uint8_t *tri_file;
    uint8_t *fam_file;
    Tri tri;
    Chunk fam;
    TrackArt art;
    TrackObject *objects;   /* one per scenery record */
    uint32_t object_count;
    TrackInfo info;
    ViewModel models[TRACK_MODELS];
    int model_ok[TRACK_MODELS];
    uint8_t *car_file;      /* TNFS_CAR=NAME: CARFAMS/NAME.CFM parked ahead */
    ViewModel car[2];       /* high and low detail */
    int car_ok;
} Track;


static uint8_t *read_any(const char *data_root, const char *dir, const char *name, const char *const *suffixes,
                         size_t *size)
{
    for (int i = 0; suffixes[i] != NULL; i++) {
        char path[512];
        snprintf(path, sizeof path, "%s/SIMDATA/%s/%s%s", data_root, dir, name, suffixes[i]);
        uint8_t *data = platform_read_file(path, size);
        if (data != NULL) return data;
    }
    fprintf(stderr, "track_viewer: no %s file for %s in %s\n", dir, name, data_root);
    return NULL;
}

static int load_track(Track *t, const char *data_root, const char *name)
{
    static const char *const TRI_SUFFIX[] = { ".TRI", ".tri", NULL };
    static const char *const FAM_SUFFIX[] = { "_T01.FAM", "_M01.FAM", "_R01.FAM", NULL };
    size_t tri_size = 0, fam_size = 0;
    memset(t, 0, sizeof *t);
    t->tri_file = read_any(data_root, "MISC", name, TRI_SUFFIX, &tri_size);
    t->fam_file = read_any(data_root, "NTRACKFM", name, FAM_SUFFIX, &fam_size);
    if (t->tri_file == NULL || t->fam_file == NULL) return 0;
    TriResult tr = tri_open(&t->tri, t->tri_file, tri_size);
    ChunkResult cr = chunk_open(&t->fam, t->fam_file, fam_size);
    if (tr != TRI_OK || cr != CHUNK_OK || !track_art_load(&t->art, &t->fam)) {
        fprintf(stderr, "track_viewer: %s: tri %s, fam %s\n", name, tri_result_str(tr), chunk_result_str(cr));
        return 0;
    }
    char info_path[512];
    size_t info_size = 0;
    snprintf(info_path, sizeof info_path, "%s/SIMDATA/MISC/%sINFO", data_root, name);
    uint8_t *info_text = platform_read_file(info_path, &info_size);
    if (info_text == NULL || !track_info_parse(&t->info, (const char *)info_text, info_size))
        fprintf(stderr, "track_viewer: no sky settings in %s\n", info_path);
    platform_free_file(info_text);
    t->objects = calloc(t->tri.scenery_count + 1u, sizeof *t->objects);
    if (t->objects == NULL) return 0;
    Chunk objects, pair;
    if (chunk_child(&t->fam, FAM_CHILD_OBJECTS, &objects) == CHUNK_OK && objects.kind == CHUNK_KIND_HEADER)
        for (uint32_t i = 0; i < objects.count && i < TRACK_MODELS; i++)
            t->model_ok[i] = chunk_child(&objects, i, &pair) == CHUNK_OK && model_load(&t->models[i], &pair);
    for (uint32_t i = 0; i < t->tri.scenery_count; i++)
        if (track_object(&t->tri, i, &t->objects[t->object_count]) == TRI_OK) t->object_count++;
    return 1;
}

enum { CAR_AHEAD = 2 };

/* A CFM's high (children 0, 1) and low (2, 3) detail models. */
static int load_car_half(ViewModel *m, const Chunk *cfm, uint32_t mesh, uint32_t bitmaps)
{
    Chunk orip, shpi;
    if (chunk_child(cfm, mesh, &orip) != CHUNK_OK || mesh_open(&m->mesh, &orip) != MESH_OK) return 0;
    m->has_textures = chunk_child(cfm, bitmaps, &shpi) == CHUNK_OK && shpi.kind == CHUNK_KIND_BITMAP &&
                      chunk_bitmaps(&shpi, &m->textures) == FSH_OK;
    return 1;
}

/* CARFAMS/NAME.CFM into `file` (kept: the models point into it), its top
 * chunk in `cfm`. */
static int load_cfm(const char *data_root, const char *name, uint8_t **file, Chunk *cfm, ViewModel models[2])
{
    static const char *const suffixes[] = { ".CFM", ".cfm", NULL };
    size_t size = 0;
    memset(cfm, 0, sizeof *cfm);
    *file = read_any(data_root, "CARFAMS", name, suffixes, &size);
    const int ok = *file != NULL && chunk_open(cfm, *file, size) == CHUNK_OK &&
                   load_car_half(&models[0], cfm, CFM_CHILD_MESH_HIGH, CFM_CHILD_BITMAPS_HIGH) &&
                   load_car_half(&models[1], cfm, CFM_CHILD_MESH_LOW, CFM_CHILD_BITMAPS_LOW);
    if (!ok) fprintf(stderr, "track_viewer: no car %s\n", name);
    if (!ok) memset(cfm, 0, sizeof *cfm);
    return ok;
}

static void load_car(Track *t, const char *data_root, const char *name)
{
    Chunk cfm;
    t->car_ok = load_cfm(data_root, name, &t->car_file, &cfm, t->car);
}

static void free_track(Track *t)
{
    platform_free_file(t->car_file);
    platform_free_file(t->tri_file);
    platform_free_file(t->fam_file);
    free(t->objects);
}

static Vec3 to_world(TrackPoint p)
{
    return (Vec3){ p.x / UNIT, p.y / UNIT, p.z / UNIT };
}

static Vec3 node_position(const Tri *tri, uint32_t index)
{
    TriNode n;
    if (tri_node(tri, index % tri->node_count, &n) != TRI_OK) return (Vec3){ 0, 0, 0 };
    return to_world((TrackPoint){ n.position.x, n.position.y, n.position.z });
}

static Vec3 normalise(Vec3 v)
{
    const float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    return len > 0 ? (Vec3){ v.x / len, v.y / len, v.z / len } : (Vec3){ 0, 0, 1 };
}

/* Eye above node `node`, looking down the road, turned by `yaw` radians. */
static Camera place_camera(const Tri *tri, uint32_t node, float yaw)
{
    const Vec3 here = node_position(tri, node), ahead = node_position(tri, node + 2);
    const float heading = atan2f(ahead.x - here.x, ahead.z - here.z) + yaw;
    Camera c;
    c.eye = (Vec3){ here.x, here.y + EYE_HEIGHT, here.z };
    c.forward = normalise((Vec3){ sinf(heading), 0, cosf(heading) });
    c.right = (Vec3){ c.forward.z, 0, -c.forward.x };
    c.up = (Vec3){ 0, 1, 0 };
    return c;
}

static RasterVertex to_camera(const Camera *c, Vec3 p, float u, float v)
{
    return camera_project(c, p, u, v);
}

/* Strip s joins chain points: 0-4 run from the centre along points 1..5,
 * 5-9 along points 6..10. */
static void strip_points(int strip, int *inner, int *outer)
{
    const int side = strip / 5, step = strip % 5;
    *inner = step == 0 ? 0 : 1 + side * 5 + step - 1;
    *outer = 1 + side * 5 + step;
}

static void draw_quad(const RasterTarget *target, const Camera *cam, const TrackArt *art, const Vec3 q[4],
                      uint8_t slot)
{
    const TrackTexture *tex = slot < TRACK_TEXTURE_SLOTS ? &art->slots[slot][0] : NULL;
    const int textured = tex != NULL && tex->present;
    const float w = textured ? tex->bitmap.width : 1, h = textured ? tex->bitmap.height : 1;
    /* u runs along the road, v across the strip. */
    const RasterVertex v[4] = { to_camera(cam, q[0], 0, 0), to_camera(cam, q[1], 0, h), to_camera(cam, q[2], w, h),
                                to_camera(cam, q[3], w, 0) };
    raster_polygon(target, v, 4, textured ? &tex->bitmap : NULL, textured && tex->transparent, (uint8_t)(slot * 7));
}

/* One row-to-row band of the 10 strips; rows a (near) and b (far). */
static void draw_band(const RasterTarget *target, const Camera *cam, const TrackArt *art, const TrackPoint *a,
                      const TrackPoint *b, const uint8_t texture[10])
{
    for (int s = 0; s < STRIPS; s++) {
        int inner, outer;
        strip_points(s, &inner, &outer);
        const Vec3 q[4] = { to_world(a[inner]), to_world(a[outer]), to_world(b[outer]), to_world(b[inner]) };
        draw_quad(target, cam, art, q, texture[s]);
    }
}

static void draw_face(const RasterTarget *target, const Camera *cam, const TrackArt *art, const TrackPoint *c0,
                      const TrackPoint *c1, const TrackPoint *c2, const TrackPoint *c3, uint8_t resource)
{
    /* The resource id indexes 005100c4 in dwords, 4 per texture. */
    const TrackTexture *tex = resource / 4 < TRACK_SCENERY_TEXTURES ? &art->scenery[resource / 4] : NULL;
    if (tex == NULL || !tex->present) return;
    const float w = tex->bitmap.width, h = tex->bitmap.height;
    const RasterVertex v[4] = { to_camera(cam, to_world(*c0), 0, 0), to_camera(cam, to_world(*c1), w, 0),
                                to_camera(cam, to_world(*c2), w, h), to_camera(cam, to_world(*c3), 0, h) };
    raster_polygon(target, v, 4, &tex->bitmap, tex->transparent, 0);
}

/* Objects standing on the nodes of one segment, farthest node first. */
static void draw_objects(const RasterTarget *target, const Camera *cam, const Track *t, uint32_t segment)
{
    const uint32_t mask = t->tri.ring_nodes - 1;
    for (int row = TRACK_SEGMENT_ROWS - 1; row >= 0; row--) {
        const uint32_t node = (segment * TRI_NODES_PER_SEGMENT + (uint32_t)row) & mask;
        /* The original walks the records backwards (0044a4b0). */
        for (uint32_t i = t->object_count; i-- > 0;) {
            const TrackObject *o = &t->objects[i];
            if (o->node != node) continue;
            if (o->descriptor.type == TRACK_OBJECT_MODEL) {
                const uint8_t m = o->descriptor.resource_id;
                if (m < TRACK_MODELS && t->model_ok[m]) model_draw(&t->models[m], target, cam, o->origin, o->heading);
                continue;
            }
            if (o->corner_count == 0) continue;
            const TrackPoint *c = o->corners;
            draw_face(target, cam, &t->art, &c[0], &c[1], &c[2], &c[3], o->descriptor.resource_id);
            /* 0044a4b0: descriptor flag bit 1 starts the second face at corner 4. */
            if (o->corner_count == 6 && (o->descriptor.flags & 2) == 0)
                draw_face(target, cam, &t->art, &c[1], &c[4], &c[5], &c[2], o->descriptor.resource2_id);
            else if (o->corner_count == 6)
                draw_face(target, cam, &t->art, &c[4], &c[5], &c[2], &c[1], o->descriptor.resource2_id);
        }
    }
}

static void draw_horizon(uint8_t *pixels, const TrackArt *art, float heading)
{
    memset(pixels, SKY_COLOUR, SCREEN_W * SCREEN_H);
    if (!art->has_horizon) return;
    const FshEntry *h = &art->horizon;
    const int shift = (int)(heading / (2.0f * 3.14159265f) * h->width);
    const int top = SCREEN_H / 2 - h->height;
    /* Above the bitmap the sky continues its top row. */
    for (int y = -top; y < h->height; y++) {
        if (top + y < 0) continue;
        for (int x = 0; x < SCREEN_W; x++) {
            int sx = (x + shift) % h->width;
            if (sx < 0) sx += h->width;
            pixels[(top + y) * SCREEN_W + x] = h->payload[(size_t)(y < 0 ? 0 : y) * h->width + sx];
        }
    }
}

static void render_stand_in(const Track *t, uint32_t node, float yaw, uint8_t *pixels)
{
    const Camera cam = place_camera(&t->tri, node, yaw);
    draw_horizon(pixels, &t->art, atan2f(cam.forward.x, cam.forward.z));
    const RasterTarget target = { pixels, SCREEN_W, SCREEN_H, SCREEN_W * 0.5f, 0.3f };
    const uint32_t first = node / TRI_NODES_PER_SEGMENT;
    const uint32_t segments = t->tri.segment_count;
    for (int k = DRAW_SEGMENTS; k >= -1; k--) {
        const int64_t index = (int64_t)first + k;
        if (!tri_is_circuit(&t->tri) && (index < 0 || index + 1 >= segments)) continue;
        TrackSegment seg, next;
        if (track_decode_segment(&t->tri, (uint32_t)((index + segments) % segments), &seg) != TRI_OK
            || track_decode_segment(&t->tri, (uint32_t)((index + 1 + segments) % segments), &next) != TRI_OK)
            continue;
        for (int r = TRACK_SEGMENT_ROWS - 1; r >= 0; r--) {
            const TrackPoint *far = r + 1 < TRACK_SEGMENT_ROWS ? seg.rows[r + 1] : next.rows[0];
            draw_band(&target, &cam, &t->art, seg.rows[r], far, seg.texture);
        }
        draw_objects(&target, &cam, t, (uint32_t)((index + segments) % segments));
    }
}

/* The ported renderer by default; the stand-in still draws the scenery. */
static void render(const Track *t, uint32_t node, float yaw, int stand_in, uint8_t *pixels)
{
    if (stand_in) {
        render_stand_in(t, node, yaw, pixels);
        return;
    }
    ported_view_set_models(t->models, t->model_ok);
    if (t->car_ok) ported_view_set_car(&t->car[0], &t->car[1], CAR_AHEAD);
    ported_view_render(&t->tri, &t->art, &t->info, node, (int32_t)(yaw / (2.0f * 3.14159265f) * 0x1000000), pixels);
}

static int write_ppm(const char *path, const uint8_t *pixels, const uint8_t *rgb)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
    int ok = fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H) > 0;
    for (int i = 0; ok && i < SCREEN_W * SCREEN_H; i++) ok = fwrite(rgb + 3 * pixels[i], 1, 3, f) == 3;
    return fclose(f) == 0 && ok;
}

static int run_window(const Track *t, uint8_t *pixels, uint32_t node)
{
    const PlatformConfig config = { SCREEN_W, SCREEN_H, "TNFS track viewer" };
    if (!platform_init(&config)) return 0;
    PlatformColor palette[256];
    for (int i = 0; i < 256; i++)
        palette[i] = (PlatformColor){ t->art.palette[3 * i], t->art.palette[3 * i + 1], t->art.palette[3 * i + 2] };
    float yaw = 0;
    int stand_in = 0;
    for (int running = 1; running && platform_poll_events();) {
        for (PlatformKey key; (key = platform_next_key()) != PLATFORM_KEY_NONE;) {
            if (key == PLATFORM_KEY_ESCAPE) running = 0;
            if (key == PLATFORM_KEY_SPACE) stand_in = !stand_in;
        }
        if (platform_key_down(0x48)) node = (node + 1) % t->tri.node_count;
        if (platform_key_down(0x50)) node = (node + t->tri.node_count - 1) % t->tri.node_count;
        if (platform_key_down(0x4B)) yaw -= 0.03f;
        if (platform_key_down(0x4D)) yaw += 0.03f;
        render(t, node, yaw, stand_in, pixels);
        platform_present(pixels, palette);
        platform_wait_vblank();
    }
    platform_shutdown();
    return 1;
}

/* Drive mode (`tnfs --drive NAME [--car N]`): the ported race loop with
 * the follow camera (race_chase) on the player's car. Keys as the game's
 * defaults: arrows steer and work the pedals, A and Z change gear (an
 * automatic starts in neutral: A for drive, Z for reverse; the gears work
 * after 5 seconds), Space the handbrake. C steps through the camera's 14
 * view kinds (the race turns to 13 while the car does not face the road:
 * turned more than 0x1e0000 from it, or pitched or rolled past 0x240000). */
enum {
    KEY_A = 0x1E,
    KEY_Z = 0x2C,
    KEY_SPACE = 0x39,
    KEY_UP = 0x48,
    KEY_DOWN = 0x50,
    KEY_LEFT = 0x4B,
    KEY_RIGHT = 0x4D,
    KEY_C = 0x2E,
    DRIVE_VIEW = 5            /* the kind the record takes attached to a car (004f8020) */
};

static RaceSim drive_sim;

/* The drive's cars: each slot's CFM (drawn) and PDN (its body). */
typedef struct {
    uint8_t *file[RACE_CARS];   /* each slot's CFM as read: the loader works in it */
    CarLoad load[RACE_CARS];    /* 00457d40's results */
    int loaded[RACE_CARS];
    RacePalette tint;           /* the tint the loader applies (0050b774): neutral */
    uint8_t *carpal;            /* CARFAMS/CARPAL.FSH */
    CarPdn pdns[RACE_CARS];
    const char *data_root;
    int ended;
    int ticked;                 /* a tick since the last frame drawn */
} DriveCars;

static DriveCars drive_cars;
static DriveHud drive_hud;
static RaceViews drive_views;

enum { HIGH_SCREEN = 1 };  /* 00512018 for 640 x 480 */

/* 00405210 for the race (race->select_view): the camera takes its view's
 * layout for the window size. */
static void select_view(void *user, RaceCamera *camera, int32_t view, int32_t size)
{
    race_view_select(user, camera, view, size);
}

/* The camera in use's layout as the 3D view's place on the frame. */
static void set_drive_viewport(const RaceState *race)
{
    const RaceCamera *camera = race->chase.camera;
    const ViewLayout *l = camera != NULL ? camera->layout_row : NULL;
    if (l == NULL || l->width <= 0 || l->height <= 0) return;
    const PortedViewport vp = { l->left + l->x, l->top + l->y, l->width, l->height, camera->fov, l->scale,
                                l->left, l->top, l->unknown_20, l->unknown_24 };
    ported_view_set_viewport(&vp);
}

enum {
    SINGLE_RACE = 2,        /* RaceField.race_type */
    DRIVE_SEED = 0x2b67,    /* 004fb7f4's stand-in (the frontend's is not read) */
    FULL_FIELD = 8          /* entries[1].car asking 00457870 for the eight-car field */
};

static uint32_t read_input(void)
{
    uint32_t steer = 0x20, pedal = 0x1e, keys = 0;
    if (platform_key_down(KEY_LEFT)) steer = 0;
    if (platform_key_down(KEY_RIGHT)) steer = 0x3f;
    if (platform_key_down(KEY_UP)) pedal = 0;
    if (platform_key_down(KEY_DOWN)) pedal = 0x3f;
    if (platform_key_down(KEY_A)) keys |= 0x1000;
    if (platform_key_down(KEY_Z)) keys |= 0x2000;
    if (platform_key_down(KEY_SPACE)) keys |= 0x8000;
    return steer | pedal << 6 | keys;
}

/* The eye where 004046b0 last placed it, angles as 004496ac reads them. */
static PortedCamera chase_camera(const RaceChase *chase, uint32_t ring_nodes)
{
    return (PortedCamera){ { chase->position[0], chase->position[1], chase->position[2] },
                           chase->angles[1], chase->angles[0],
                           (int32_t)((uint32_t)chase->node & (ring_nodes - 1u)), chase->angles[2] };
}

/* TODO(0045a3e8): the race's draw path builds these angles itself (with
 * the suspension of 0042ecd0); 004591d4 takes the tilt about z at 3/4. */
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

static void render_drive(const Track *t, uint8_t *pixels)
{
    /* 00458d4c: 00458614 and 00458818 on cars[004fbb08] as it draws it.
     * The wheel frames only after a tick: the window draws two frames a
     * tick, and a frame without one sees the car still (its front wheels
     * would stop). TODO: only when that car is drawn this frame. */
    RaceState *race = &drive_sim.race;
    Car *followed = race->cars[race_slot(race->player)];
    const int32_t player = race_slot(race->player);
    if (followed != NULL && drive_cars.loaded[player] && race->models[player] >= 200) {
        if (drive_cars.ticked) car_wheel_frames(followed, race, bind_wheels, NULL);
        car_steer_wheels(followed, race, morph_wheels, NULL);
    }
    drive_cars.ticked = 0;
    set_drive_viewport(race);
    const PortedCamera cam = chase_camera(&drive_sim.race.chase, t->tri.ring_nodes);
    PortedCar list[RACE_CARS];
    for (int i = 0; i < drive_sim.count; i++) {
        const Car *c = drive_sim.race.cars[i];
        list[i] = (PortedCar){ car_pose_of(c), (int32_t)((uint32_t)c->draw_node & (t->tri.ring_nodes - 1u)) };
    }
    ported_view_set_models(t->models, t->model_ok);
    /* 0044ce4b, before the 3D view in 0044cc74: the mirror's picture, from the last frame's road direction */
    DriveMirrorWorld mirror = { &t->tri, &t->art, race, { 0, 0, 0 } };
    ported_view_near_dir(mirror.near_dir);
    drive_mirror_frame(&drive_hud.mirror, &mirror);
    ported_view_render_cars(&t->tri, &t->art, &t->info, &cam, list, drive_sim.count, drive_hud_view(&drive_hud, race, pixels));
    drive_hud_draw(&drive_hud, race, drive_sim.specs[race_slot(race->player)].unknown_0ac, pixels, SCREEN_W, SCREEN_H);
}

static void car_stem(int model, char stem[16])
{
    memset(stem, 0, 16);
    for (int i = 0; car_model_names[model][i] != '\0' && i < 15; i++)
        stem[i] = (char)toupper((unsigned char)car_model_names[model][i]);
}

/* A file the original names in mixed case ("TSupra.cfm"); the CD's are
 * upper case, so a case-sensitive file system gets a second try. */
static uint8_t *read_named(const char *path, size_t dir_length, size_t *size)
{
    uint8_t *file = platform_read_file(path, size);
    if (file != NULL) return file;
    char upper[CAR_LOAD_PATH];
    snprintf(upper, sizeof upper, "%s", path);
    for (size_t i = dir_length; upper[i] != '\0'; i++) upper[i] = (char)toupper((unsigned char)upper[i]);
    return platform_read_file(upper, size);
}

static int load_pdn(const char *path, size_t dir_length, CarPdn *pdn)
{
    size_t size = 0;
    uint8_t *file = read_named(path, dir_length, &size);
    const int ok = file != NULL && car_pdn_read(pdn, file, size);
    platform_free_file(file);
    if (!ok) fprintf(stderr, "track_viewer: no body %s\n", path);
    return ok;
}

/* RaceField.load: 00457d40 for the slot (the CFM its model names, read
 * here), its objects to the view, then its PDN. */
static const CarPdn *drive_load(void *user, Car *car, RaceState *race)
{
    (void)user;
    const int slot = car->index;
    if (slot < 0 || slot >= RACE_CARS) return NULL;
    char dir[CAR_LOAD_PATH], path[CAR_LOAD_PATH], stem[16];
    snprintf(dir, sizeof dir, "%s/SIMDATA/CARFAMS/", drive_cars.data_root);
    if (!car_load_path(car, race, dir, path, stem)) return NULL;
    size_t size = 0;
    drive_cars.file[slot] = read_named(path, strlen(dir), &size);
    const CarLoadResult r = drive_cars.file[slot] == NULL ? CAR_LOAD_FILE
        : car_load_setup(car, race, &drive_cars.tint, NULL, &drive_cars.load[slot], drive_cars.file[slot], size);
    drive_cars.loaded[slot] = r == CAR_LOAD_OK;
    if (!drive_cars.loaded[slot]) {
        fprintf(stderr, "track_viewer: car %s not loaded (%d)\n", path, (int)r);
        return NULL;
    }
    car->body_object = (uint32_t)slot;  /* bind_wheels' token for the slot's object */
    ported_view_set_car_objects(slot, &drive_cars.load[slot].object[0], &drive_cars.load[slot].object[1]);
    car_load_pdn_path(dir, stem, path);
    return load_pdn(path, strlen(dir), &drive_cars.pdns[slot]) ? &drive_cars.pdns[slot] : NULL;
}

/* Slot 0 the human's `model`, then the other race models (0..7) as
 * computer racers (a single race, as test_race runs it); racers 1 drives
 * alone. race_type 0..3: the cars 0042702c sets for it, the police and
 * traffic (slots past the racers) by their drawn models. */
static int start_drive(Track *t, const char *data_root, const char *name, int model, int racers, int race_type)
{
    char stem[16];
    car_stem(model, stem);
    char path[512];
    snprintf(path, sizeof path, "%s/SIMDATA/CARSPECS/%s.PBS", data_root, stem);
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    CarSpecs specs;
    const int ok = file != NULL && car_specs_read(&specs, file, size);
    platform_free_file(file);
    if (!ok) {
        fprintf(stderr, "track_viewer: no specs %s\n", path);
        return 0;
    }
    RaceField field;
    memset(&field, 0, sizeof field);
    field.racers = racers;
    field.race_type = racers > 1 ? SINGLE_RACE : 0;
    if (race_type >= 0) {
        field.race_type = race_type;
        race_field_counts(&field, race_sim_track_index(name), 0, 0);
        racers = field.racers;
    }
    field.seed = DRIVE_SEED;
    field.load = drive_load;
    drive_cars.data_root = data_root;
    drive_cars.tint.tint = (RacePaletteTint){ 0x10000, 0x100, 0, 0x100, 0x100 };
    drive_cars.tint.tint_ready = 1;
    for (int i = 0, next = 0; i < racers; i++) {
        if (i > 0 && next == model) next++;
        const int m = i == 0 ? model : next++;  /* the others: the 8 race models in order */
        if (i == 0) strcpy(field.entries[i].name, "Player");
        else snprintf(field.entries[i].name, sizeof field.entries[i].name, "%s", car_model_names[m]);
        field.entries[i].car = m;
        field.entries[i].automatic = 1;
    }
    /* The frontend's stand-in: a single race with the other entry's car 8
     * gets 00457870's field of eight different cars (004c5628); any other
     * car there would race seven copies of it. */
    if (field.race_type == SINGLE_RACE && racers > 1) field.entries[1].car = FULL_FIELD;
    race_sim_start_field(&drive_sim, &t->tri, name, &specs, &field, 2, 0);
    memcpy(drive_views.layouts, race_view_layouts, sizeof drive_views.layouts);
    memcpy(drive_views.rows, race_view_rows, sizeof drive_views.rows);
    drive_views.mode = HIGH_SCREEN;
    drive_sim.race.select_view = select_view;
    drive_sim.race.view_user = &drive_views;
    drive_sim.race.window_size = 0;
    race_chase_follow(&drive_sim.race, &drive_sim.race.chase, drive_sim.race.cars[0], DRIVE_VIEW);
    drive_cars.ended = 0;
    static const char *const CARPAL[] = { ".FSH", ".fsh", NULL };
    size_t carpal_size = 0;
    drive_cars.carpal = read_any(data_root, "CARFAMS", "CARPAL", CARPAL, &carpal_size);
    CarLoad *cars[RACE_CARS] = { 0 };
    for (int i = 0; i < RACE_CARS; i++) cars[i] = drive_cars.loaded[i] ? &drive_cars.load[i] : NULL;
    if (!drive_hud_read(&drive_hud, data_root)) fprintf(stderr, "track_viewer: no HUD\n");
    /* 0041f9a0 from 004400f4, before 00455a5c builds the palette (0041f818 maps the dash onto it) */
    drive_hud_cockpit(&drive_hud, &drive_sim.race, data_root, stem);
    if (!drive_palette_build(&drive_sim.race, &t->fam, cars, drive_cars.carpal, &drive_hud, t->art.palette))
        fprintf(stderr, "track_viewer: the race palette was not built; the track's own is used\n");
    return 1;
}

static void free_drive(void)
{
    for (int i = 0; i < RACE_CARS; i++) {
        ported_view_set_car_slot(i, NULL, NULL);
        car_load_free(&drive_cars.load[i]);
        drive_cars.loaded[i] = 0;
        platform_free_file(drive_cars.file[i]);
        drive_cars.file[i] = NULL;
    }
    platform_free_file(drive_cars.carpal);
    drive_cars.carpal = NULL;
    drive_hud_end(&drive_hud, &drive_sim.race);
    drive_hud_free(&drive_hud);
}

/* One step; at the race's end the results (once), printed. */
static void drive_tick(uint32_t input)
{
    race_sim_tick(&drive_sim, input);
    drive_cars.ticked = 1;
    if (drive_cars.ended || !race_sim_over(&drive_sim)) return;
    race_sim_end(&drive_sim);
    drive_cars.ended = 1;
    printf("race over (%d):", drive_sim.race.end_request);
    for (int i = 0; i < drive_sim.count; i++) {
        const int slot = drive_sim.race.results_order[i];
        const Car *c = slot >= 0 && slot < drive_sim.count ? drive_sim.race.cars[slot] : NULL;
        printf(" %d.%s", i + 1, c == NULL ? "?" : slot == 0 ? "Player" : car_model_names[c->model & 7]);
    }
    printf("\n");
}

/* The next of the 14 view kinds, chosen as 00404450 does (next_kind, the
 * kind race_camera_pick keeps): the kind after the one chosen. */
static void next_view(void)
{
    RaceState *race = &drive_sim.race;
    race_chase_follow(race, &race->chase, race->cars[0], (race->chase.next_kind + 1) % CHASE_KINDS);
    printf("view kind %d (shown %d)\n", race->chase.next_kind, race->chase.kind);
}

static int drive_window(const Track *t, uint8_t *pixels)
{
    const PlatformConfig config = { SCREEN_W, SCREEN_H, "TNFS drive" };
    if (!platform_init(&config)) return 0;
    PlatformColor palette[256];
    int frame = 0, c_down = 0;
    for (int running = 1; running && platform_poll_events();) {
        for (PlatformKey key; (key = platform_next_key()) != PLATFORM_KEY_NONE;)
            if (key == PLATFORM_KEY_ESCAPE) running = 0;
        const int c = platform_key_down(KEY_C);
        if (c && !c_down) next_view();
        c_down = c;
        if ((frame++ & 1) == 0) drive_tick(read_input());
        render_drive(t, pixels);
        for (int i = 0; i < 256; i++)  /* the police screen writes the palette (00476460) */
            palette[i] = (PlatformColor){ t->art.palette[3 * i], t->art.palette[3 * i + 1], t->art.palette[3 * i + 2] };
        platform_present(pixels, palette);
        platform_wait_vblank();
    }
    platform_shutdown();
    return 1;
}

/* Headless: `ticks` steps in drive at full throttle, then a screenshot
 * (TNFS_VIEW=N: from view kind N; TNFS_STEER=left / right: full lock at the
 * end; TNFS_NOTICE=1 / 2: that police stop notice up for the last 10 ticks,
 * TNFS_NOTICE=1off / 2off: and down again for the last 5). */
static int drive_screenshot(const Track *t, uint8_t *pixels, uint32_t ticks, const char *screenshot)
{
    const char *view = getenv("TNFS_VIEW");
    if (view != NULL) race_chase_follow(&drive_sim.race, &drive_sim.race.chase, drive_sim.race.cars[0], atoi(view) % CHASE_KINDS);
    /* TNFS_STEER=left / right: that lock over the last 20 ticks. */
    const char *steer_env = getenv("TNFS_STEER");
    const uint32_t lock = steer_env == NULL ? 0x20 : strcmp(steer_env, "left") == 0 ? 0 : 0x3f;
    for (uint32_t i = 0; i < ticks; i++) {
        const int tap = drive_sim.race.tick >= 320 && drive_sim.race.tick < 328;
        drive_tick((i + 20 >= ticks ? lock : 0x20) | (tap ? 0x1000u : 0));
        const char *notice = getenv("TNFS_NOTICE");
        if (notice != NULL && (notice[0] == '1' || notice[0] == '2') && i + 10 >= ticks) {
            drive_sim.race.stop_notice[notice[0] - '1'] = !(notice[1] == 'o' && i + 5 >= ticks);
            render_drive(t, pixels);  /* whole frames, so the screen comes and goes as on screen */
            continue;
        }
        drive_hud_draw(&drive_hud, &drive_sim.race, 0, NULL, 0, 0);  /* the HUD keeps drawing every frame */
    }
    const Car *car = drive_sim.race.cars[0];
    const RaceChase *chase = &drive_sim.race.chase;
    printf("tick %d: node %d, speed %.1f m/s, gear %d; view kind %d, eye node %d (%.1f, %.1f, %.1f)\n",
           drive_sim.race.tick, car->node, car->speed / 65536.0, car->gear, chase->kind, chase->node,
           chase->position[0] / 65536.0, chase->position[1] / 65536.0, chase->position[2] / 65536.0);
    render_drive(t, pixels);
    return write_ppm(screenshot, pixels, t->art.palette);
}

int track_viewer_drive(const char *data_root, const char *name, int model, int racers, int race_type, uint32_t ticks,
                       const char *screenshot)
{
    static Track t;
    int ok = model >= 0 && model < CAR_MODELS && racers >= 1 && racers <= RACE_PLAYERS && race_type <= 3 &&
             load_track(&t, data_root, name);
    ok = ok && start_drive(&t, data_root, name, model, racers, race_type);
    uint8_t *pixels = ok ? malloc(SCREEN_W * SCREEN_H) : NULL;
    ok = ok && pixels != NULL;
    if (ok) ok = screenshot != NULL ? drive_screenshot(&t, pixels, ticks, screenshot) : drive_window(&t, pixels);
    free(pixels);
    free_drive();
    free_track(&t);
    return ok;
}

int track_viewer_main(const char *data_root, const char *name, uint32_t node, const char *screenshot)
{
    Track t;
    int ok = load_track(&t, data_root, name);
    if (ok && getenv("TNFS_CAR") != NULL) load_car(&t, data_root, getenv("TNFS_CAR"));
    uint8_t *pixels = ok ? malloc(SCREEN_W * SCREEN_H) : NULL;
    ok = ok && pixels != NULL;
    if (ok) {
        printf("%s: %u segments, %s\n", name, t.tri.segment_count, tri_is_circuit(&t.tri) ? "circuit" : "stage");
        node %= t.tri.node_count;
        if (screenshot != NULL) {
            render(&t, node, 0, getenv("TNFS_STAND_IN") != NULL, pixels);
            ok = write_ppm(screenshot, pixels, t.art.palette);
        } else {
            ok = run_window(&t, pixels, node);
        }
    }
    free(pixels);
    free_track(&t);
    return ok;
}
