/* `tnfs --track NAME`: development fly-through of a track (simdata/misc/NAME.TRI
 * with its simdata/ntrackfm/NAME_[TMR]01.FAM textures). Up/Down move along the
 * road, Left/Right turn the camera, Space switches between the ported
 * renderer and the earlier stand-in (which also draws the scenery; with
 * --screenshot, TNFS_STAND_IN=1 picks it). */
#include "viewer/track_viewer.h"

#include "formats/chunk.h"
#include "formats/tri.h"
#include "game/race/car_load.h"
#include "game/race/race_sim.h"
#include "game/race/track_art.h"
#include "game/race/track_geometry.h"
#include "game/race/track_info.h"
#include "game/race/track_objects.h"
#include "platform/platform.h"
#include "viewer/camera.h"
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

static void load_car(Track *t, const char *data_root, const char *name)
{
    static const char *const suffixes[] = { ".CFM", ".cfm", NULL };
    size_t size = 0;
    Chunk cfm;
    t->car_file = read_any(data_root, "CARFAMS", name, suffixes, &size);
    t->car_ok = t->car_file != NULL && chunk_open(&cfm, t->car_file, size) == CHUNK_OK &&
                load_car_half(&t->car[0], &cfm, CFM_CHILD_MESH_HIGH, CFM_CHILD_BITMAPS_HIGH) &&
                load_car_half(&t->car[1], &cfm, CFM_CHILD_MESH_LOW, CFM_CHILD_BITMAPS_LOW);
    if (!t->car_ok) fprintf(stderr, "track_viewer: no car %s\n", name);
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
 * one car and a stand-in chase camera. Keys as the game's defaults: arrows
 * steer and work the pedals, A and Z change gear (an automatic starts in
 * neutral: A for drive, Z for reverse; the gears work after 5 seconds),
 * Space the handbrake. */
enum {
    KEY_A = 0x1E,
    KEY_Z = 0x2C,
    KEY_SPACE = 0x39,
    KEY_UP = 0x48,
    KEY_DOWN = 0x50,
    KEY_LEFT = 0x4B,
    KEY_RIGHT = 0x4D,
    CHASE_BACK = 0x60000,     /* TODO(0045b24c): the original chase camera */
    CHASE_UP = 0x20000,
    CHASE_PITCH = 0x40000
};

static RaceSim drive_sim;

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

static PortedCamera chase_camera(const Car *car)
{
    const float a = (float)car->heading / 0x1000000 * 2.0f * 3.14159265f;
    PortedCamera cam = { { car->x - (int32_t)(sinf(a) * CHASE_BACK), car->y + CHASE_UP,
                           car->z - (int32_t)(cosf(a) * CHASE_BACK) },
                         car->heading, CHASE_PITCH, car->draw_node };
    return cam;
}

/* TODO(0045a3e8): the race's draw path builds these angles itself (with
 * the suspension of 0042ecd0); 004591d4 takes the tilt about z at 3/4. */
static CarPose car_pose_of(const Car *car)
{
    return (CarPose){ { car->x, car->y, car->z }, car->draw_pitch + car->tilt_x, car->draw_roll + car->tilt_z * 3 / 4,
                      car->heading, car->use_matrix ? car->matrix : NULL, 1 };
}

static void render_drive(const Track *t, uint8_t *pixels)
{
    const Car *car = drive_sim.race.cars[0];
    const PortedCamera cam = chase_camera(car);
    const CarPose pose = car_pose_of(car);
    ported_view_set_models(t->models, t->model_ok);
    ported_view_render_from(&t->tri, &t->art, &t->info, &cam, &pose,
                            (int32_t)((uint32_t)car->draw_node & (t->tri.ring_nodes - 1u)), pixels);
}

static int start_drive(Track *t, const char *data_root, const char *name, int model)
{
    char stem[16] = { 0 };
    for (int i = 0; car_model_names[model][i] != '\0' && i < 15; i++)
        stem[i] = (char)toupper((unsigned char)car_model_names[model][i]);
    load_car(t, data_root, stem);
    if (t->car_ok) ported_view_set_car(&t->car[0], &t->car[1], 0);
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
    RaceEntry entry;
    memset(&entry, 0, sizeof entry);
    strcpy(entry.name, "Player");
    entry.car = model;
    entry.automatic = 1;
    race_sim_start(&drive_sim, &t->tri, name, &specs, &entry, 2, 0);
    return 1;
}

static int drive_window(const Track *t, uint8_t *pixels)
{
    const PlatformConfig config = { SCREEN_W, SCREEN_H, "TNFS drive" };
    if (!platform_init(&config)) return 0;
    PlatformColor palette[256];
    for (int i = 0; i < 256; i++)
        palette[i] = (PlatformColor){ t->art.palette[3 * i], t->art.palette[3 * i + 1], t->art.palette[3 * i + 2] };
    int frame = 0;
    for (int running = 1; running && platform_poll_events();) {
        for (PlatformKey key; (key = platform_next_key()) != PLATFORM_KEY_NONE;)
            if (key == PLATFORM_KEY_ESCAPE) running = 0;
        if ((frame++ & 1) == 0) race_sim_tick(&drive_sim, read_input());
        render_drive(t, pixels);
        platform_present(pixels, palette);
        platform_wait_vblank();
    }
    platform_shutdown();
    return 1;
}

/* Headless: `ticks` steps in drive at full throttle, then a screenshot. */
static int drive_screenshot(const Track *t, uint8_t *pixels, uint32_t ticks, const char *screenshot)
{
    for (uint32_t i = 0; i < ticks; i++) {
        const int tap = drive_sim.race.tick >= 320 && drive_sim.race.tick < 328;
        race_sim_tick(&drive_sim, 0x20 | (tap ? 0x1000u : 0));
    }
    const Car *car = drive_sim.race.cars[0];
    printf("tick %d: node %d, speed %.1f m/s, gear %d\n", drive_sim.race.tick, car->node, car->speed / 65536.0, car->gear);
    render_drive(t, pixels);
    return write_ppm(screenshot, pixels, t->art.palette);
}

int track_viewer_drive(const char *data_root, const char *name, int model, uint32_t ticks, const char *screenshot)
{
    static Track t;
    int ok = model >= 0 && model < CAR_MODELS && load_track(&t, data_root, name);
    ok = ok && start_drive(&t, data_root, name, model);
    uint8_t *pixels = ok ? malloc(SCREEN_W * SCREEN_H) : NULL;
    ok = ok && pixels != NULL;
    if (ok) ok = screenshot != NULL ? drive_screenshot(&t, pixels, ticks, screenshot) : drive_window(&t, pixels);
    free(pixels);
    ported_view_set_car(NULL, NULL, 0);
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
