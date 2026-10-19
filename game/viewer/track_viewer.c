/* `tnfs --track NAME`: development fly-through of a track (simdata/misc/NAME.TRI
 * with its simdata/ntrackfm/NAME_[TMR]01.FAM textures). Up/Down move along the
 * road, Left/Right turn the camera, Space switches between the ported
 * renderer and the earlier stand-in (which also draws the scenery; with
 * --screenshot, TNFS_STAND_IN=1 picks it). */
#include "viewer/track_viewer.h"

#include "formats/chunk.h"
#include "formats/tri.h"
#include "game/gfx/blit.h"
#include "game/gfx/palette_match.h"
#include "game/race/car_load.h"
#include "game/race/car_model.h"
#include "game/race/car_wheels.h"
#include "game/race/car_draw_setup.h"
#include "game/race/car_scenery.h"
#include "game/race/frame_draw.h"
#include "game/race/race_camera.h"
#include "game/race/race_chase.h"
#include "game/race/race_draw_setup.h"
#include "game/race/race_models.h"
#include "game/race/race_palette.h"
#include "game/race/race_replay.h"
#include "game/race/race_replay_image.h"
#include "game/race/race_run.h"
#include "game/race/race_sound.h"
#include "game/race/race_sim.h"
#include "game/race/track_art.h"
#include "game/race/track_fam.h"
#include "game/race/track_geometry.h"
#include "game/race/track_info.h"
#include "game/race/track_objects.h"
#include "game/sys/resource.h"
#include "platform/platform.h"
#include "viewer/camera.h"
#include "game/race/race_view.h"
#include "viewer/drive_audio.h"
#include "viewer/drive_hud.h"
#include "viewer/drive_input.h"
#include "viewer/drive_joy.h"
#include "viewer/drive_keys.h"
#include "viewer/drive_menu.h"
#include "viewer/drive_pause.h"
#include "viewer/drive_replay.h"
#include "viewer/drive_palette.h"
#include "viewer/drive_smoke.h"
#include "viewer/model_view.h"
#include "viewer/ported_view.h"
#include "viewer/raster.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    RacePaletteTint tint;   /* 0050b774: 0043a370's 004471ac for the time of day */
    ViewModel models[TRACK_MODELS];
    int model_ok[TRACK_MODELS];
    uint8_t *car_file;      /* TNFS_CAR=NAME: CARFAMS/NAME.CFM parked ahead */
    ViewModel car[2];       /* high and low detail */
    int car_ok;
} Track;


/* 0043a370's file: the INFO text read whole (the port's name, not "%s%sInfo"). */
typedef struct {
    const char *p, *end;
    Track *t;
} InfoFile;

static int32_t info_open(void *ctx, const char *name) { (void)name; return ((InfoFile *)ctx)->p < ((InfoFile *)ctx)->end; }
static void info_close(void *ctx, int32_t file) { (void)ctx, (void)file; }

static int info_gets(void *ctx, char *line, int32_t size, int32_t file)
{
    (void)file;
    InfoFile *f = ctx;
    if (f->p >= f->end) return 0;
    int32_t n = 0;
    while (f->p < f->end && n + 1 < size) {
        const char c = *f->p++;
        line[n++] = c;
        if (c == '\n') break;
    }
    line[n] = '\0';
    return 1;
}

static void info_tint(void *ctx, int32_t red, int32_t green, int32_t blue, int32_t saturation, int32_t brightness)
{
    static RacePalette scratch;  /* 004471ac on the tint's words alone */
    race_palette_set_tint(&scratch, red, green, blue, saturation, brightness);
    ((InfoFile *)ctx)->t->tint = scratch.tint;
}

/* 0043a370 for the track `name` (its stem lower case as the frontend gives it), the time of day (004fa0ec), 640 x 480. */
static void load_info(Track *t, const char *name, int time_of_day, const char *text, size_t size)
{
    char stem[16] = { 0 };
    for (size_t i = 0; name[i] != '\0' && i + 1 < sizeof stem; i++) stem[i] = (char)tolower((unsigned char)name[i]);
    InfoFile f = { text, text + size, t };
    t->tint = (RacePaletteTint){ 0x10000, 0x100, 0, 0x100, 0x100 };
    const TrackInfoOps ops = { &f, info_open, info_gets, info_close, info_tint };
    const TrackInfoInputs in = { stem, "", 1, time_of_day, 1 };
    memset(&t->info, 0, sizeof t->info);
    track_info_load(&ops, &in, &t->info);
}

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

static int32_t le32_at(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

/* 004510e8's FAM for the time of day and the variant (track_fam.h) under data_root, its ".fam" as the CD's capitals
 * on a second try. Port guard: a FAM the CD does not hold (TR4 has no _T01) falls back to the first of NTRACKFM's. */
static uint8_t *read_fam(const char *data_root, const char *name, const TrackFamInputs *look, size_t *size)
{
    char dir[400], dir_midday[400], path[512];
    snprintf(dir, sizeof dir, "%s/SIMDATA/NTRACKFM", data_root);
    snprintf(dir_midday, sizeof dir_midday, "%s/SIMDATA/%s", data_root, resource_german() ? "GTRACKFM" : "ETRACKFM");
    TrackFamInputs in = *look;
    in.stem = name, in.dir = dir, in.dir_midday = dir_midday;
    track_fam_name(&in, 1, path, sizeof path);
    uint8_t *data = platform_read_file(path, size);
    const size_t n = strlen(path);
    if (data == NULL && n >= 3) {
        for (size_t i = n - 3; i < n; i++) path[i] = (char)toupper((unsigned char)path[i]);
        data = platform_read_file(path, size);
    }
    if (data != NULL) return data;
    static const char *const FAM_SUFFIX[] = { "_T01.FAM", "_M01.FAM", "_R01.FAM", NULL };
    fprintf(stderr, "track_viewer: no %s, the first FAM there is\n", path);
    return read_any(data_root, "NTRACKFM", name, FAM_SUFFIX, size);
}

/* The track `name` with 004510e8's art and 0043a370's sky for `look` (its time of day, variants and track). */
static int load_track(Track *t, const char *data_root, const char *name, const TrackFamInputs *look)
{
    static const char *const TRI_SUFFIX[] = { ".TRI", ".tri", NULL };
    size_t tri_size = 0, fam_size = 0;
    memset(t, 0, sizeof *t);
    t->tri_file = read_any(data_root, "MISC", name, TRI_SUFFIX, &tri_size);
    t->fam_file = read_fam(data_root, name, look, &fam_size);
    if (t->tri_file == NULL || t->fam_file == NULL) return 0;
    TriResult tr = tri_open(&t->tri, t->tri_file, tri_size);
    ChunkResult cr = chunk_open(&t->fam, t->fam_file, fam_size);
    if (tr != TRI_OK || cr != CHUNK_OK || !track_art_load(&t->art, &t->fam)) {
        fprintf(stderr, "track_viewer: %s: tri %s, fam %s\n", name, tri_result_str(tr), chunk_result_str(cr));
        return 0;
    }
    track_art_load_animated(&t->art, &t->fam, t->tri.data);  /* 0044c36c's "ga%d0" */
    char info_path[512];
    size_t info_size = 0;
    snprintf(info_path, sizeof info_path, "%s/SIMDATA/MISC/%sINFO", data_root, name);
    uint8_t *info_text = platform_read_file(info_path, &info_size);
    if (info_text == NULL) fprintf(stderr, "track_viewer: no %s, the built-in sky settings\n", info_path);
    load_info(t, name, look->time_of_day, (const char *)info_text, info_text != NULL ? info_size : 0);
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
    ported_view_forget_track();
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
 * after 5 seconds), Space and H the 0x8000 / 0x4000 bits. The race's key
 * commands (004381f8, drive_keys): C the next view kind (0040594c; the race
 * turns to 13 while the car does not face the road), Alt + Ctrl + 1..8
 * another car, F1 the next view row, F2 the window size, F3 the detail,
 * F4 / F6 their flags, F5 the sound and music off and on. F9, a
 * development key standing in for the menus: the director's replay, and
 * during it back to the race (drive_replay.h). Headless, TNFS_REPLAY_AT and
 * TNFS_REPLAY_BACK press it on those frames. */
enum {
    DRIVE_VIEW = 5            /* the kind the record takes attached to a car (004f8020) */
};

static RaceSim drive_sim;
static DriveInput drive_input;
static TrackViewerLink drive_link;  /* a link race's, taken by the next drive */
static int drive_link_on;
static RaceRunWords *drive_link_run;  /* the running race's words (004c5598) the inputs' messages write */
static DriveKeys drive_keys;
static DriveReplay drive_replay;
enum { KEY_F9 = 0x43 };
static DrivePause drive_pause;
static DriveMenu drive_menu;

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
static DriveSmoke drive_smoke;

/* race_rand taken while drawing (the tyre smoke), counted once a tick: the window draws two frames a tick, the first
 * after a tick draws and logs, the others replay the log. */
enum { DRAW_RANDS = 256 };
static struct {
    int32_t value[DRAW_RANDS];
    int count, at;
} draw_rands;

static int32_t draw_rand(void *ctx, int32_t n)
{
    (void)ctx;
    if (draw_rands.at < draw_rands.count) return draw_rands.value[draw_rands.at++];
    const int32_t r = race_rand_below(&drive_sim.race, n);
    if (draw_rands.count < DRAW_RANDS) draw_rands.value[draw_rands.count++] = r, draw_rands.at = draw_rands.count;
    return r;
}
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
    HEAD_TO_HEAD = 1,       /* RaceField.race_type */
    SINGLE_RACE = 2,
    DRIVE_SEED = 0x2b67,    /* 004fb7f4's stand-in for --drive without the menus */
    REPLAY_KIND = 1,        /* 00524e9f for a replay played back (0042c498's 1..3): any set */
    FULL_FIELD = 8          /* entries[1].car asking 00457870 for the eight-car field */
};

/* The eye where 004046b0 last placed it, angles as 004496ac reads them. */
static PortedCamera chase_camera(const RaceChase *chase, uint32_t ring_nodes)
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
static FrameDraw drive_frame;

enum {
    RATE_TICK_MS = 10           /* 00473334's unit: hundredths */
};

/* 0044d2c4 / 0044ca3c's calls in --drive: the surfaces, the track, its textures and the palette are the port's
 * own (start_drive), the camera follows the player; 00449be4 is not ported (TODO: 00511fb8 / 00511fbc = 0; the
 * mappers it names are ported_view's own); 00449430 has no effect; 004fa0c4 (the second camera's step) is taken as 0. */
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
    race_chase_follow(race, &race->chase, race->cars[race_slot(race->player)], kind);
}

/* 0044d2c4 from race_run: its words into drive_frame (the restart, 00511fc4, the draw distance asked for, the
 * mirror's count and state) and the HUD (004f9b3c). */
static void drive_frame_setup(RaceState *race)
{
    const RaceDrawSetupOps ops = { race, ds_palette_get, ds_none, ds_palette_set, ds_none_i, ds_mode, ds_surface, ds_none,
                                   ds_none, ds_follow, ds_views, ds_chase_start, ds_rate_target, ds_auto_detail, ds_none, ds_none,
                                   ds_screen };
    RaceDrawSetup st = { 0 };
    st.restart = drive_frame.restart, st.mirror_state = drive_hud.mirror.state, st.use_parts = drive_hud.keys.use_parts;
    st.high = HIGH_SCREEN, st.width = SCREEN_W, st.height = SCREEN_H, st.back_buffer = 0;
    st.replay_kind = 0, st.chase_kind = race->chase.kind, st.detail = 0, st.pending_segments = drive_frame.pending_segments;
    st.screen_kind = drive_frame.screen_kind, st.clear_view = drive_frame.clear_view;
    race_draw_start(&ops, &st);
    drive_frame.restart = st.restart, drive_frame.unknown_511fc4 = st.unknown_511fc4;
    drive_frame.pending_segments = st.pending_segments, drive_frame.mirror_count = st.mirror_count;
    drive_frame.clear_view = st.clear_view;
    drive_hud.mirror.state = st.mirror_state, drive_hud.keys.use_parts = st.use_parts;
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
                                    mirror_car };
    ported_view_near_dir(f->mirror.near_dir);  /* as the front view last left them */
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
    (void)segments, (void)node;  /* the port's renderer keeps its own distance */
    DriveFrame *f = ctx;
    const Track *t = f->t;
    PortedCar list[RACE_CARS];
    for (int i = 0; i < drive_sim.count; i++) {
        const Car *c = f->race->cars[i];
        list[i] = (PortedCar){ car_pose_of(c), (int32_t)((uint32_t)c->draw_node & (t->tri.ring_nodes - 1u)), f->race->cars[i] };
    }
    ported_view_render_cars(&t->tri, &t->art, &t->info, &f->cam, list, drive_sim.count, drive_hud_view(&drive_hud, f->pixels));
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

static void render_drive(const Track *t, uint8_t *pixels)
{
    RaceState *race = &drive_sim.race;
    render_ticked = drive_cars.ticked;
    ported_view_set_wheels(drive_wheel_frames, drive_steer, NULL);
    const int ticked = drive_cars.ticked;
    drive_cars.ticked = 0;
    if (ticked) draw_rands.count = 0;
    draw_rands.at = 0;
    ported_view_set_models(t->models, t->model_ok);
    ported_view_set_shadows(race, drive_hud.ready ? drive_hud.shade : NULL);
    ported_view_set_tick(race->tick);
    if (drive_hud.ready) ported_view_set_tags(drive_hud.car_colours, HUD_MAP_COLOURS, drive_hud.strip_colour);
    if (drive_hud.palette != NULL) ported_view_set_lights(drive_hud_palette_write, &drive_hud, drive_hud.palette->slot_colours);
    static uint32_t last_ms;
    DriveFrame f = { 0 };
    f.t = t, f.pixels = pixels, f.race = race, f.d = &drive_frame, f.ticked = ticked, f.last_ms = last_ms;
    drive_frame_in(&drive_frame, race);
    set_drive_viewport(race);
    f.cam = chase_camera(&race->chase, t->tri.ring_nodes);
    FrameDrawOps ops = drive_frame_ops;
    ops.ctx = &f;
    frame_draw(&ops, &drive_frame);
    drive_hud.mirror.state = drive_frame.mirror_state;
    last_ms = f.last_ms;
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
static TrackViewerMenus menus;
static int menus_on;
static const ReplayRace *replay_of;   /* the replay played back, its file replay_file; NULL a race */
static const uint8_t *replay_file;
static int replay_request;            /* 0044f534's: 4 or 5 */
static TrackViewerResults results;

TrackViewerResults track_viewer_drive_results(void) { return results; }

void track_viewer_drive_menus(const TrackViewerMenus *m)
{
    menus = *m;
    menus_on = 1;
}

void track_viewer_drive_link(const TrackViewerLink *l)
{
    drive_link = *l;
    drive_link_on = l->humans >= 1 && l->humans <= RACE_PLAYERS && l->slot >= 0 && l->slot < l->humans;
}

/* 004275f8 for a replay: 0052b316, 0044f528 and 0044f534(4); the cars' models 00457870 drew against the file's */
static void replay_begin(void)
{
    enum { MODELS_AT = 0x2d8 };
    RaceState *race = &drive_sim.race;
    drive_replay.file = replay_file;
    race->replay = 1;
    race_replay_end_clear(&drive_replay.dir);
    race_replay_request(&drive_replay.dir, replay_request);
    for (int i = 0; i < RACE_CARS; i++) {
        const uint8_t *m = replay_file + MODELS_AT + 4 * i;
        const int32_t model = (int32_t)(m[0] | m[1] << 8 | m[2] << 16 | (uint32_t)m[3] << 24);
        if (race->models[i] != model)
            fprintf(stderr, "track_viewer: the replay's slot %d model %d, drawn %d\n", i, model, race->models[i]);
    }
}

/* A car model's specs (its PBS) */
static int load_specs(const char *data_root, int model, CarSpecs *specs)
{
    char stem[16];
    car_stem(model, stem);
    char path[512];
    snprintf(path, sizeof path, "%s/SIMDATA/CARSPECS/%s.PBS", data_root, stem);
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    const int ok = file != NULL && car_specs_read(specs, file, size);
    platform_free_file(file);
    if (!ok) fprintf(stderr, "track_viewer: no specs %s\n", path);
    return ok;
}

static void link_stand_in(int16_t player);

static int32_t link_wait_ticks;  /* 004fbac8: 00410d80's, a link race's queue wait */

/* 004273d0 in a link game: this machine's entry (its car and aids, as a solo race's first) out, every slot's back */
static const uint8_t *link_entries(int model, uint32_t *seed, uint32_t *flags)
{
    RaceEntry own;
    memset(&own, 0, sizeof own);
    own.car = model, own.automatic = 1;
    if (menus_on) own.automatic = menus.automatic, own.anti_lock = menus.anti_lock, own.traction = menus.traction;
    return drive_link.exchange(&own, menus_on ? menus.game_state : NULL, &link_wait_ticks, seed, flags);
}

static int start_drive(Track *t, const char *data_root, const char *name, int model, int racers, int race_type)
{
    static CarSpecs human_specs[RACE_PLAYERS];  /* a link race's other humans' */
    /* 004fb7f4: 004275f8's time(0), the race's random seed (00428fe0); --drive alone keeps a fixed one */
    uint32_t seed = menus_on ? (uint32_t)time(NULL) : DRIVE_SEED;
    uint32_t flags = menus_on ? menus.track_flags : 0;  /* 004fb7d8: 0042702c's copy of 0052b306 */
    const uint8_t *entries = drive_link_on ? link_entries(model, &seed, &flags) : NULL;  /* the host's setup's, 004fb7fa */
    if (entries != NULL) memcpy(&model, entries + offsetof(RaceEntry, car), sizeof model);  /* slot 0's specs: the host's car's */
    CarSpecs specs;
    if (!load_specs(data_root, model, &specs)) return 0;
    int own = model;  /* the dash (0041f9a0) is this machine's car's: in a link race its slot's in the setup */
    if (entries != NULL) memcpy(&own, entries + sizeof(RaceEntry) * (size_t)drive_link.slot + offsetof(RaceEntry, car), sizeof own);
    char stem[16];
    car_stem(own, stem);
    RaceField field;
    memset(&field, 0, sizeof field);
    field.racers = racers;
    field.race_type = racers > 1 ? SINGLE_RACE : 0;
    if (race_type >= 0) {
        field.race_type = race_type;
        race_field_counts(&field, race_sim_track_index(name), flags, drive_link_on);  /* 0052b312: no police in a link game */
        racers = field.racers;
    }
    field.seed = seed;
    field.track_flags = flags;
    field.load = drive_load;
    drive_cars.data_root = data_root;
    drive_cars.tint.tint = (RacePaletteTint){ 0x10000, 0x100, 0, 0x100, 0x100 };
    drive_cars.tint.tint_ready = 1;
    for (int i = 0, next = 0; i < racers; i++) {
        if (i > 0 && next == model) next++;
        const int m = i == 0 ? model : next++;  /* the others: the 8 race models in order */
        if (i == 0) snprintf(field.entries[i].name, sizeof field.entries[i].name, "%s",
                             menus_on && menus.name != NULL ? menus.name : "Player");
        else snprintf(field.entries[i].name, sizeof field.entries[i].name, "%s", car_model_names[m]);
        field.entries[i].car = m;
        field.entries[i].automatic = 1;
    }
    if (menus_on) {
        field.entries[0].automatic = menus.automatic, field.entries[0].anti_lock = menus.anti_lock;
        field.entries[0].traction = menus.traction;
        if (field.race_type == HEAD_TO_HEAD && racers > 1) field.entries[1].car = menus.opponent;
    }
    /* The frontend's stand-in: a single race with the other entry's car 8
     * gets 00457870's field of eight different cars (004c5628); any other
     * car there would race seven copies of it. */
    if (field.race_type == SINGLE_RACE && racers > 1 && !drive_link_on) field.entries[1].car = FULL_FIELD;
    for (int i = 0; entries != NULL && i < drive_link.humans && i < racers; i++) {  /* the machines' entries */
        memcpy(&field.entries[i], entries + sizeof(RaceEntry) * (size_t)i, sizeof field.entries[i]);
        if (i > 0 && !load_specs(data_root, field.entries[i].car, &human_specs[i])) return 0;
        field.specs[i] = i > 0 ? &human_specs[i] : NULL;
    }
    if (drive_link_on) field.humans = drive_link.humans;
    if (replay_of != NULL) {  /* the replay's own field (0044d3b0's setup), with its seed and its humans' specs */
        field = replay_of->field;
        field.track_flags = replay_of->track_flags;
        field.load = drive_load;
        for (int i = 1; i < field.humans && i < RACE_PLAYERS; i++) {
            if (!load_specs(data_root, field.entries[i].car, &human_specs[i])) return 0;
            field.specs[i] = &human_specs[i];
        }
    }
    const int laps = replay_of != NULL ? replay_of->laps : menus_on && menus.laps > 0 ? menus.laps : 2;
    race_sim_start_field(&drive_sim, &t->tri, name, &specs, &field, laps, 0);
    if (menus_on && replay_of == NULL)  /* the cheats' words, shared with the menus in the original */
        drive_sim.race.unknown_4c5cd4 = menus.explode, drive_sim.race.start_input = menus.start_boost;
    /* config.dat's words the race reads in place: 00524e85 the track, 00524e9f the replay's kind (0 a race; set, the
     * results and records are left alone, 0045af80) with 00524ea1 the laps above it */
    drive_sim.race.frontend_track = menus_on && replay_of == NULL ? menus.track : race_sim_track_index(name);
    drive_sim.race.frontend_laps_word = (int32_t)((uint32_t)(uint16_t)laps << 16) | (replay_of != NULL ? REPLAY_KIND : 0);
    if (drive_link_on) {  /* 004275f8 in a link game: 0052b312 1, this machine's car followed and viewed, its side */
        RaceState *race = &drive_sim.race;
        race->playback = 1, race->player = race->viewed = drive_link.slot;
        race->unknown_4fbb0c = drive_link.host, race->unknown_4fb7d4 = 1;
    }
    drive_joy_install(&drive_sim.race);
    if (menus_on && menus.game_state != NULL)
        memcpy(drive_sim.race.game_state, menus.game_state, sizeof drive_sim.race.game_state);
    results = (TrackViewerResults){ 0 };
    drive_input_start(&drive_input, &drive_sim.race);
    drive_input.hud = &drive_hud.hud;
    drive_input.own_specs = &drive_sim.specs[drive_sim.race.player];  /* port: test harness only (drive_autopilot) */
    if (drive_link_on) {
        drive_input.link = &drive_link, drive_input.stand_in_block = drive_link.stand_ins;
        for (int i = 0; i < drive_link.humans; i++) drive_input.queues[i].active = 1;
        drive_link.racing(drive_input.queues, drive_sim.race.players, link_stand_in);
    }
    drive_keys_start(&drive_keys);
    drive_pause_start(&drive_pause);
    memcpy(drive_views.layouts, race_view_layouts, sizeof drive_views.layouts);
    memcpy(drive_views.rows, race_view_rows, sizeof drive_views.rows);
    drive_views.mode = HIGH_SCREEN;
    drive_sim.race.select_view = select_view;
    drive_sim.race.view_user = &drive_views;
    drive_sim.race.window_size = 0;
    race_chase_follow(&drive_sim.race, &drive_sim.race.chase, drive_sim.race.cars[drive_link_on ? drive_link.slot : 0], DRIVE_VIEW);
    if (drive_link_on) {  /* a link game records none (0044edd8 with 0052b312 1): 004fbb0c stays the link's */
        drive_replay_start(&drive_replay, &drive_sim, &drive_views);
        drive_sim.race.unknown_4fbb0c = drive_link.host;
    } else if (!drive_replay_start(&drive_replay, &drive_sim, &drive_views)) {
        fprintf(stderr, "track_viewer: no replay recording\n");
    }
    drive_replay.seed = field.seed;
    if (replay_of != NULL) replay_begin();
    /* 0044cc74's words before any frame; race_run's 0044d2c4 (drive_frame_setup) sets the race's start on them */
    drive_frame = (FrameDraw){ 0 };
    drive_frame.pending_segments = -1;
    drive_frame.mirror_count = DRAW_SETUP_MIRROR_COUNT;
    drive_cars.ended = 0;
    static const char *const CARPAL[] = { ".FSH", ".fsh", NULL };
    size_t carpal_size = 0;
    drive_cars.carpal = read_any(data_root, "CARFAMS", "CARPAL", CARPAL, &carpal_size);
    CarLoad *cars[RACE_CARS] = { 0 };
    for (int i = 0; i < RACE_CARS; i++) cars[i] = drive_cars.loaded[i] ? &drive_cars.load[i] : NULL;
    if (!drive_hud_read(&drive_hud, data_root)) fprintf(stderr, "track_viewer: no HUD\n");
    drive_hud.director = drive_replay.recording != NULL ? &drive_replay.dir : NULL;  /* the dash controls' words */
    drive_replay.hud = &drive_hud.hud;
    drive_replay.key_flags = drive_input.keys;  /* 0044f55c reads 004c693c */
    drive_replay.dash = &drive_hud.art;
    drive_replay.panel = &drive_hud.panel, drive_replay.replay_art = &drive_hud.replay;  /* 00471460 */
    /* 0041f9a0 from 004400f4, before 00455a5c builds the palette (0041f818 maps the dash onto it) */
    drive_hud_cockpit(&drive_hud, &drive_sim.race, data_root, stem);
    /* 00456a7c from 00455a5c (after the cars' load), its bank and 00456948 at 00447098's end */
    drive_smoke_setup(&drive_smoke, data_root, &drive_sim.race.trail);
    const DrivePaletteLate late = { &drive_smoke, drive_smoke_palette, drive_smoke_blend };
    if (!drive_palette_build(&drive_sim.race, &t->fam, cars, drive_cars.carpal, &drive_hud, &late, &t->tint, t->art.palette))
        fprintf(stderr, "track_viewer: the race palette was not built; the track's own is used\n");
    if (drive_smoke.ready) ported_view_set_puffs(&drive_smoke.art, &drive_smoke.blend, draw_rand, NULL, &drive_sim.race.trail);
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
    drive_hud.director = NULL;
    drive_replay_end(&drive_replay);
    drive_hud_end(&drive_hud, &drive_sim.race);
    drive_hud_free(&drive_hud);
    ported_view_set_puffs(NULL, NULL, NULL, NULL, NULL);
    drive_smoke_free(&drive_smoke);
}

/* The race's end (once): 004275f8 runs 0045af80 after race_run however it
 * stopped, a game given up from the pause menu too; the results printed. */
static void drive_end(void)
{
    if (drive_cars.ended) return;
    race_sim_end(&drive_sim);
    drive_cars.ended = 1;
    const RaceState *race = &drive_sim.race;
    results = (TrackViewerResults){ 1, (int16_t)race->end_request, race->results_player_rank, race->results_rival_rank,
                                    race->results_rival, race->results_rival_name, { 0 }, race->results_player_place,
                                    race->results_shown_place, race->racers, race->player, race->end_state };
    memcpy(results.dropped, race->unknown_4f82a0, sizeof results.dropped);
    if (menus_on && menus.game_state != NULL)
        memcpy(menus.game_state, race->game_state, sizeof race->game_state);
    printf("race over (%d):", drive_sim.race.end_request);
    for (int i = 0, place = 0; i < drive_sim.count; i++) {  /* the racers only, not the traffic or police */
        const int slot = drive_sim.race.results_order[i];
        if (slot >= drive_sim.race.racers) continue;
        const Car *c = slot >= 0 ? drive_sim.race.cars[slot] : NULL;
        printf(" %d.%s", ++place, c == NULL ? "?" : slot == 0 ? "Player" : car_model_names[c->model & 7]);
    }
    printf("\n");
}

/* One step; at the race's end the results. */
static void drive_tick(uint32_t input)
{
    race_sim_tick(&drive_sim, input);
    drive_cars.ticked = 1;
    if (race_sim_over(&drive_sim)) drive_end();
}

/* The window's race: 00455a5c (race_run) on --drive. Its setup and
 * teardown calls are start_drive's / free_drive's work, done there (no-ops
 * here); the start's three ticks run on the sim; the services poll the
 * window and run the timer at 30 a second (60 race ticks, two a frame);
 * a frame is the keys and race_sim_tick; a draw is render_drive and the
 * present. */
typedef struct {
    const Track *t;
    uint8_t *pixels;
    RaceRunWords *w;
    uint32_t base_ms;
    int quit;
    int32_t headless_ticks;     /* TNFS_RUN with --screenshot: a frame due each pass, no window, this many frames */
    int32_t late, unknown_4fb78c, store_clock;  /* 004fb790, 004fb78c, 004fb7a0: the network's, unread here */
    int32_t timer_written;      /* the timer run_service last wrote (-1 none yet) */
} DriveRun;

enum { DRIVE_TIMER_HZ = 30, DRIVE_IDLE_MS = 1 };

static void run_none(void *ctx) { (void)ctx; }
static void run_none_i(void *ctx, int32_t a) { (void)ctx, (void)a; }

static void run_service(void *ctx)
{
    DriveRun *r = ctx;
    if (r->headless_ticks > 0) {
        const int last = r->w->done >= r->headless_ticks;
        if (last) r->w->running = 0;
        r->w->timer = last ? r->w->done : r->w->done + 1;
        return;
    }
    drive_audio_pump();
    if (!platform_poll_events()) r->quit = 1;
    for (PlatformKey key; (key = platform_next_key()) != PLATFORM_KEY_NONE;)  /* Esc is the menu's (004557f4) */
        if (key == PLATFORM_KEY_ESCAPE && drive_pause.menu == NULL) r->quit = 1;
    if (r->quit) r->w->running = 0;
    const uint32_t now = platform_ticks_ms();
    /* the timer written elsewhere (the start zeroes it): count on from its value */
    if (r->w->timer != r->timer_written) r->base_ms = now - (uint32_t)((int64_t)r->w->timer * 1000 / DRIVE_TIMER_HZ);
    r->w->timer = (int32_t)((uint64_t)(now - r->base_ms) * DRIVE_TIMER_HZ / 1000u);
    r->timer_written = r->w->timer;
    if (r->w->timer <= r->w->done) platform_sleep_ms(DRIVE_IDLE_MS);
}

static void run_screen_mode(void *ctx, int32_t w, int32_t h, int32_t bits) { (void)ctx, (void)w, (void)h, (void)bits; }
static int32_t run_net_wait(void *ctx, int32_t kind) { (void)ctx; return drive_link_on ? drive_link.wait(kind) : 1; }
static void run_clip(void *ctx, int32_t l, int32_t r, int32_t t, int32_t b) { (void)ctx, (void)l, (void)r, (void)t, (void)b; }
/* 00474c8c: --drive records nothing yet (TODO: race_replay_record's glue) and reads no 0051c770, so none is made. */
static uint8_t *run_no_recording(void *ctx, int32_t size) { (void)ctx, (void)size; return NULL; }

/* 0044edd8 on the race's words; the ones without a home in the port (0052b8d4..0052b8f0, 005120c4, 005120c8,
 * 00513718, 00513730, 00513888..98, 004c5388, 005137e4) are this call's own. */
static void run_replay_start(void *ctx)
{
    (void)ctx;
    RaceState *race = &drive_sim.race;
    RaceReplayStart w = { 0 };
    w.view_taken = race->view_taken, w.w4fbb0c = race->unknown_4fbb0c, w.playback = race->playback;
    w.mark_countdown = race->replay_mark_countdown, w.unknown_512f78 = race->unknown_512f78;
    w.unknown_512f7c = race->unknown_512f7c, w.unknown_513728 = race->unknown_513728;
    w.unknown_51372c = race->unknown_51372c, w.unknown_51371c = race->unknown_51371c;
    w.note_second = race->note_second, w.notes = race->notes, w.camera_roll_add = race->camera_roll_add;
    race_replay_start(&w, run_no_recording, NULL);
    race->view_taken = w.view_taken, race->replay_mark_countdown = w.mark_countdown;
    race->unknown_512f78 = w.unknown_512f78, race->unknown_512f7c = w.unknown_512f7c;
    race->unknown_513728 = w.unknown_513728, race->unknown_51372c = w.unknown_51372c;
    race->unknown_51371c = w.unknown_51371c, race->note_second = w.note_second;
    race->camera_roll_add = w.camera_roll_add;
}

/* 00457594: the distance steps and the buffer have no reader in the port yet (its car drawing keeps its own);
 * the skid marks are ported_view's. */
static void run_car_draw_setup(void *ctx)
{
    (void)ctx;
    static CarDrawSetup words;
    car_draw_setup(&words, ported_view_skids(), run_no_recording, NULL);
}

/* 00464604: the music's volume ramped to */
static void run_music_volume(void *user, int16_t volume) { (void)user, drive_audio_music_volume(volume); }

static void run_last_grey(void *ctx) { (void)ctx, race_palette_last_grey(drive_hud_palette_write, &drive_hud); }  /* 00446918 */

/* 00456244 on the music setting (00524e63: the CONFIG default 1, a track of the twelve drawn) at the music volume
 * (00524e65: 100); 00464428 plays the track */
enum { DRIVE_MUSIC_SETTING = 1, DRIVE_MUSIC_VOLUME = 100, DRIVE_MUSIC_TRACKS = 12 };
static const char *const drive_music_names[DRIVE_MUSIC_TRACKS] = {  /* 004c55ac */
    "rock1.as4", "rock2.as4", "rock3.as4", "rock4.as4", "rock5.as4", "rock6.as4",
    "techno1.as4", "techno2.as4", "techno3.as4", "techno4.as4", "techno5.as4", "techno6.as4"
};
static int32_t run_music_rand(void *ctx, int32_t n) { (void)ctx; return race_rand_below(&drive_sim.race, n); }
static void run_music_play(void *ctx, int32_t track)
{
    (void)ctx;
    if (track >= 0 && track < DRIVE_MUSIC_TRACKS) drive_audio_music_play(drive_music_names[track]);
}
static void run_music_fill(void *ctx) { (void)ctx, drive_audio_music_fill(); }  /* 004640d0 */
static void run_music_start(void *ctx)
{
    const DriveRun *r = ctx;
    drive_audio_music_set(r->w->music_on, menus_on ? (int16_t)menus.music_volume : DRIVE_MUSIC_VOLUME, r->w->music_started);
    const RaceMusicOps ops = { ctx, run_music_rand, run_music_play };
    race_music_start(&ops, r->w->music_on);
}

static void run_sound_setup(void *ctx) { (void)ctx, race_sound_setup(&drive_sim.race, run_music_volume, NULL); }  /* 00443b64 */

static void run_draw_setup(void *ctx) { (void)ctx, drive_frame_setup(&drive_sim.race); }  /* 0044d2c4 */

static void run_queue_reset(void *ctx, InputQueue *q)
{
    DriveRun *r = ctx;
    input_queue_reset(q, &r->late, &r->unknown_4fb78c);
}

/* A queue's clock as a frame count: the original polls on every second 004bb8d8 (0043b970), its clock (2 a poll)
 * keeping pace with 004c5590; the drive polls each frame, its clock twice the frames run */
static int32_t clock_frames(int32_t clock) { return clock / 2; }

static void run_queue_prime(void *ctx, InputQueue *q)
{
    DriveRun *r = ctx;
    input_queue_prime(q, drive_sim.race.unknown_4fb7d4, r->w->wait_ticks, &r->store_clock, &r->w->replay_due);
    if (drive_link_on) r->w->replay_due = clock_frames(r->w->replay_due);
}
/* 00410250: a local game has no links and sends nothing, a link race through the link; 004fbb0c taken as 1 in a
 * local game, as drive_input does */
static void run_send_none(void *ctx, int32_t to, const int16_t *words, int32_t size)
{
    (void)ctx;
    if (drive_link_on) drive_link.send(to, words, size);
}
static int32_t run_linked(void *ctx, int32_t player) { (void)ctx, (void)player; return 1; }
/* 0041019c for the link's handlers: the player stood in, sent round */
static void link_stand_in(int16_t player)
{
    const RaceState *race = &drive_sim.race;
    const InputStandInWords w = { (int16_t)race->race_type, race->replay, race->playback, race->player, drive_link.host };
    const InputSendOps send = { NULL, run_send_none };
    if (player >= 0 && player < RACE_PLAYERS) input_stand_in(player, drive_input.stand_in_block, drive_sim.race.players, &w, &send);
}

static void run_stand_in_start(void *ctx)
{
    const InputStandInStartWords w = { drive_link_on ? drive_link.host : 1, drive_sim.race.playback, drive_sim.race.racers,
                                       drive_sim.race.humans };
    const InputStandInStartOps ops = { ctx, run_send_none, run_linked };
    input_stand_in_start(drive_input.stand_in_block, drive_sim.race.players, &w, &ops);
}
/* 00443be0: --drive loads no sound banks (audio output waits, option B), so only the channels stop */
static RaceSoundBanks drive_banks;

static void run_sound_end(void *ctx) { (void)ctx, race_sound_end(&drive_sim.race, &drive_banks, drive_audio_bank_ops()); }
static void run_sound_banks(void *ctx) { (void)ctx, race_sound_load_banks(&drive_sim.race, drive_audio_load, &drive_banks); }  /* 00443924 */
static void run_start_voices(void *ctx) { (void)ctx, race_sound_start_voices(&drive_sim.race); }  /* 00443820 */
static void run_engines_start(void *ctx) { (void)ctx, race_sound_start_engine(&drive_sim.race); }  /* 00444394 */
/* 0043cbe0 / 0043cbf4 / 0043cbcc: the controls' forces (no force hooks in --drive) */
static void run_forces_on(void *ctx) { (void)ctx, race_forces_enable(&drive_sim.race); }
static void run_forces_clear(void *ctx) { (void)ctx, race_forces_clear(&drive_sim.race); }
static void run_forces_off(void *ctx) { (void)ctx, race_forces_disable(&drive_sim.race); }
static void run_laps(void *ctx, int32_t nodes, int32_t laps) { (void)ctx, (void)nodes, (void)laps; }  /* race_sim_start_field's */
static void run_tick(void *ctx) { (void)ctx, race_sim_tick_only(&drive_sim); }

static uint32_t screenshot_input(uint32_t i, uint32_t ticks);

/* A frame number from the environment, -1 when unset */
static int32_t env_frame(const char *name)
{
    const char *v = getenv(name);
    return v != NULL ? (int32_t)strtol(v, NULL, 10) : -1;
}

/* Headless, F9 down on the frames TNFS_REPLAY_AT and TNFS_REPLAY_BACK name,
 * once each (race_run's frame count stands while a replay seeks) */
static int env_press(int32_t frame)
{
    static int fired[2];
    static const char *const NAMES[2] = { "TNFS_REPLAY_AT", "TNFS_REPLAY_BACK" };
    for (int k = 0; k < 2; k++)
        if (!fired[k] && env_frame(NAMES[k]) == frame) return fired[k] = 1;
    return 0;
}

static void run_frame(void *ctx, int32_t frame)
{
    DriveRun *r = ctx;  /* race_sim_tick runs 0045597c with the race's own tick */
    if (r->headless_ticks > 0) {
        drive_replay_key(&drive_replay, env_press(frame));
        drive_tick(screenshot_input((uint32_t)frame, (uint32_t)r->headless_ticks));
        return;
    }
    drive_input_read_keys(&drive_input);
    drive_audio_music_pump();  /* 0045597c: 00464240 with 00524e63 set, before the schedule */
    drive_pause_frame(&drive_pause, &drive_input, &drive_sim.race, &drive_hud, (int16_t)drive_sim.race.tick);
    if (drive_pause.paused_ms != 0) {  /* the timer stood still */
        r->base_ms += drive_pause.paused_ms;
        drive_pause.paused_ms = 0;
    }
    if (drive_pause.quit) r->quit = 1, r->w->running = 0;
    drive_keys_frame(&drive_keys, &drive_input, &drive_sim.race, &drive_views);
    ported_view_set_sky_off(drive_keys.state.w4fb786);
    drive_replay_key(&drive_replay, drive_input.keys[KEY_F9] != 0);
    drive_tick(drive_input_tick(&drive_input, &drive_sim.race));
}

static void present_drive(const Track *t, uint8_t *pixels)
{
    PlatformColor palette[256];
    for (int i = 0; i < 256; i++)  /* the police screen writes the palette (00476460) */
        palette[i] = (PlatformColor){ t->art.palette[3 * i], t->art.palette[3 * i + 1], t->art.palette[3 * i + 2] };
    platform_present(pixels, palette);
}

static void run_draw(void *ctx)
{
    DriveRun *r = ctx;
    render_drive(r->t, r->pixels);
    if (r->headless_ticks == 0) present_drive(r->t, r->pixels);
}

/* 0041ec0c (004b95b4 0, a link race's start): the screen filled with 00473140(0xff000000) on the race's palette,
 * the sync shape (00513970) at its own place, shown */
static void run_wait_screen(void *ctx)
{
    DriveRun *r = ctx;
    const RacePalette *p = drive_hud.palette;
    const PaletteMatch m = { p != NULL ? p->inverse : NULL, p != NULL ? p->screen : NULL, p != NULL ? p->enabled : NULL };
    memset(r->pixels, palette_match_rgb(&m, 0xff000000u), SCREEN_W * SCREEN_H);
    const QuadTarget screen = { r->pixels, SCREEN_W, 0, 0, SCREEN_W, SCREEN_H };
    if (drive_hud.art.sync != NULL) blit_shape_placed(&screen, drive_hud.art.sync);
    if (r->headless_ticks == 0) present_drive(r->t, r->pixels);
}

static const RaceRunOps *drive_run_ops(void);

static int drive_window(const Track *t, uint8_t *pixels)
{
    const PlatformConfig config = { SCREEN_W, SCREEN_H, "TNFS drive" };
    if (!menus_on && !platform_init(&config)) return 0;
    if (drive_audio_open(&drive_sim.race, drive_cars.data_root) && menus_on) {  /* 004275f8's words for the followed slot */
        RaceEntry *followed = &drive_sim.race.entries[race_slot(drive_sim.race.player)];
        followed->sound_volume = menus.sound_volume, followed->music_volume = menus.music_volume;
        followed->sound_mode = menus.sound_mode;
    }
    drive_menu = (DriveMenu){ &drive_sim.race, &drive_replay, &drive_hud, &drive_pause.keys, NULL, NULL, NULL, 0, 0,
                              drive_link_on ? &drive_link : NULL };
    if (drive_menu_open(&drive_menu, drive_cars.data_root))
        drive_pause.menu = drive_menu_run, drive_pause.menu_user = &drive_menu;
    RaceRunWords w = { 0 };
    w.queues = drive_input.queues;
    w.wait_ticks = drive_link_on ? link_wait_ticks : 0;
    w.music_on = menus_on ? (int16_t)menus.music : DRIVE_MUSIC_SETTING;
    w.unknown_4c53a0 = 0;
    drive_replay.pace = &w.unknown_4c53a0, drive_replay.timer = &w.timer, drive_replay.due = &w.due;
    DriveRun r = { t, pixels, &w, platform_ticks_ms(), 0, 0, 0, 0, 0, -1 };
    drive_input.store_clock = &r.store_clock, drive_input.late = &r.late, drive_input.timer = &w.timer;
    drive_link_run = &w;
    RaceRunOps ops = *drive_run_ops();
    ops.ctx = &r;
    drive_menu.words = &w, drive_menu.music_start = run_music_start, drive_menu.music_user = &r;
    race_run(&ops, &drive_sim.race, &w);
    drive_end();
    drive_link_run = NULL;
    results.end_state = drive_sim.race.end_state;  /* also a replay left before its race was over */
    drive_menu.words = NULL, drive_menu.music_start = NULL, drive_menu.music_user = NULL;
    drive_replay.pace = drive_replay.timer = drive_replay.due = NULL;
    while (!menus_on && !r.quit && platform_poll_events()) {  /* the race over: its last picture until Escape */
        for (PlatformKey key; (key = platform_next_key()) != PLATFORM_KEY_NONE;)
            if (key == PLATFORM_KEY_ESCAPE) r.quit = 1;
        render_drive(t, pixels);
        present_drive(t, pixels);
        drive_audio_pump();
        platform_wait_vblank();
    }
    drive_pause.menu = NULL;
    if (drive_menu.ready) drive_menu_close(&drive_menu);
    drive_audio_close();
    if (!menus_on) platform_shutdown();
    return 1;
}

/* TNFS_RUN=1 with --screenshot: race_run as the window runs it, without a
 * window or a clock, until the race tick reaches `ticks`. */
static int drive_run_screenshot(const Track *t, uint8_t *pixels, uint32_t ticks, const char *screenshot)
{
    RaceRunWords w = { 0 };
    w.queues = drive_input.queues;
    w.music_on = DRIVE_MUSIC_SETTING;  /* the track drawn as the window draws it (no sound here) */
    drive_replay.pace = &w.unknown_4c53a0, drive_replay.timer = &w.timer, drive_replay.due = &w.due;
    DriveRun r = { t, pixels, &w, 0, 0, (int32_t)ticks, 0, 0, 0, -1 };
    RaceRunOps ops = *drive_run_ops();
    ops.ctx = &r;
    race_run(&ops, &drive_sim.race, &w);
    drive_replay.pace = drive_replay.timer = drive_replay.due = NULL;
    render_drive(t, pixels);
    return write_ppm(screenshot, pixels, t->art.palette);
}

static const RaceRunOps *drive_run_ops(void)
{
    static const RaceRunOps ops = {
        NULL, run_service, run_screen_mode, run_none_i, run_forces_on, run_forces_clear, run_sound_setup, run_sound_banks, run_start_voices,
        run_engines_start, run_none_i,
        run_none, run_car_draw_setup, run_replay_start, run_none, run_none, run_none, run_none_i, run_net_wait, run_draw_setup, run_none, run_none,
        run_none, run_none_i, run_none, run_none, run_none, run_none, run_none, run_none, run_none, run_none, run_none,
        run_none, run_last_grey, run_none, run_none, run_music_start, run_music_fill, run_none, run_clip, run_queue_reset, run_queue_prime,
        run_none,
        run_none, run_wait_screen, run_laps, run_tick, run_none, run_stand_in_start, run_frame, run_draw, run_forces_off, run_none, run_none,
        run_none, run_none, run_none, run_none, run_none, run_none, run_none, run_none, run_none, run_sound_end };
    return &ops;
}

/* The screenshots' input on step i of `ticks`: full throttle, a tap of
 * 0x1000 while the race tick is 320..327, and over the last 20 steps the
 * lock TNFS_STEER=left / right asks for. */
static uint32_t screenshot_input(uint32_t i, uint32_t ticks)
{
    const char *steer_env = getenv("TNFS_STEER");
    const uint32_t lock = steer_env == NULL ? 0x20 : strcmp(steer_env, "left") == 0 ? 0 : 0x3f;
    const int tap = drive_sim.race.tick >= 320 && drive_sim.race.tick < 328;
    return (i + 20 >= ticks ? lock : 0x20) | (tap ? 0x1000u : 0);
}

/* Headless: `ticks` steps in drive at full throttle, then a screenshot
 * (TNFS_VIEW=N: from view kind N; TNFS_STEER=left / right: full lock at the
 * end; TNFS_NOTICE=1 / 2: that police stop notice up for the last 10 ticks,
 * TNFS_NOTICE=1off / 2off: and down again for the last 5; TNFS_FRAMES=N:
 * whole frames over the last N ticks, so what builds up while drawing, the
 * skid marks, does). */
static int drive_screenshot(const Track *t, uint8_t *pixels, uint32_t ticks, const char *screenshot)
{
    const char *view = getenv("TNFS_VIEW");
    if (view != NULL) race_chase_follow(&drive_sim.race, &drive_sim.race.chase, drive_sim.race.cars[0], atoi(view) % CHASE_KINDS);
    const char *frames_env = getenv("TNFS_FRAMES");
    const uint32_t frames = frames_env == NULL ? 0 : (uint32_t)atoi(frames_env);
    for (uint32_t i = 0; i < ticks; i++) {
        drive_replay_key(&drive_replay, env_press((int32_t)i));
        drive_tick(screenshot_input(i, ticks));
        const char *notice = getenv("TNFS_NOTICE");
        if (notice != NULL && (notice[0] == '1' || notice[0] == '2') && i + 10 >= ticks) {
            drive_sim.race.stop_notice[notice[0] - '1'] = !(notice[1] == 'o' && i + 5 >= ticks);
            render_drive(t, pixels);  /* whole frames, so the screen comes and goes as on screen */
            continue;
        }
        if (i + frames >= ticks) {
            render_drive(t, pixels);
            continue;
        }
        drive_hud_strip(&drive_hud, &drive_sim.race);  /* the HUD keeps drawing every frame */
        ported_view_stream(&t->tri, &t->art, chase_camera(&drive_sim.race.chase, t->tri.ring_nodes).node);  /* and 0043f540 */
    }
    const Car *car = drive_sim.race.cars[0];
    const RaceChase *chase = &drive_sim.race.chase;
    printf("tick %d (phase %d): node %d, speed %.1f m/s, gear %d; view kind %d, eye node %d (%.1f, %.1f, %.1f)\n",
           drive_sim.race.tick, drive_sim.race.phase, car->node, car->speed / 65536.0, car->gear, chase->kind, chase->node,
           chase->position[0] / 65536.0, chase->position[1] / 65536.0, chase->position[2] / 65536.0);
    render_drive(t, pixels);
    return write_ppm(screenshot, pixels, t->art.palette);
}

int track_viewer_drive(const char *data_root, const char *name, int model, int racers, int race_type, uint32_t ticks,
                       const char *screenshot)
{
    static Track t;
    /* 004278c0: 004fa0ec 1 with a variant, else the track's time of day (004fbae0[004fb7c2]): a replay's from its
     * setup (004fbad8 +0x31c, 004fbadc +0x320, 004fbae0 +0x324, 004fb7c2 +6), the menus'; else 0 and no variant */
    TrackFamInputs look = { 0, 0, 0, race_sim_track_index(name), NULL, NULL, NULL };
    if (replay_file != NULL) {
        const int32_t track = le32_at(replay_file + 6);
        look.variant_m = le32_at(replay_file + 0x31c), look.variant_r = le32_at(replay_file + 0x320), look.track = track;
        look.time_of_day = look.variant_m != 0 || look.variant_r != 0 ? 1
                           : track >= 0 && track < 9 ? le32_at(replay_file + 0x324 + 4 * track) : 1;  /* port guard */
    } else if (menus_on) {
        look.variant_m = menus.variant_m, look.variant_r = menus.variant_r;
        look.time_of_day = menus.variant_m != 0 || menus.variant_r != 0 ? 1 : menus.time_of_day;
    }
    int ok = model >= 0 && model < CAR_MODELS && racers >= 1 && racers <= RACE_PLAYERS && race_type <= 3 &&
             load_track(&t, data_root, name, &look);
    ok = ok && start_drive(&t, data_root, name, model, racers, race_type);
    uint8_t *pixels = ok ? malloc(SCREEN_W * SCREEN_H) : NULL;
    ok = ok && pixels != NULL;
    if (ok && screenshot != NULL && getenv("TNFS_RUN") != NULL)
        ok = drive_run_screenshot(&t, pixels, ticks, screenshot);
    else if (ok && screenshot != NULL)
        ok = (race_sim_begin(&drive_sim), drive_screenshot(&t, pixels, ticks, screenshot));
    else if (ok)
        ok = drive_window(&t, pixels);
    free(pixels);
    free_drive();
    free_track(&t);
    if (drive_link_on) drive_link.racing(NULL, NULL, NULL);
    menus_on = 0, drive_link_on = 0;
    drive_input.link = NULL, drive_input.stand_in_block = drive_input.own_stand_ins;
    return ok;
}

int track_viewer_drive_replay_data(const char *data_root, const uint8_t *file, size_t size, int request)
{
    static ReplayRace race;
    if (file == NULL || size != REPLAY_FILE_SIZE || !race_replay_image_race(file, &race)) {
        fprintf(stderr, "track_viewer: not a replay\n");
        menus_on = 0;
        return 0;
    }
    replay_of = &race, replay_file = file, replay_request = request;
    const int ok = track_viewer_drive(data_root, race.stem, race.field.entries[0].car, race.field.racers, -1, 0, NULL);
    replay_of = NULL, replay_file = NULL;
    return ok;
}

int track_viewer_drive_replay(const char *data_root, const char *path)
{
    enum { FROM_START = 4 };
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    const int ok = track_viewer_drive_replay_data(data_root, file, size, FROM_START);
    platform_free_file(file);
    return ok;
}

int track_viewer_main(const char *data_root, const char *name, uint32_t node, const char *screenshot)
{
    Track t;
    const TrackFamInputs look = { 0, 0, 0, race_sim_track_index(name), NULL, NULL, NULL };
    int ok = load_track(&t, data_root, name, &look);
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

/* --- a link race's messages (004bcb98[2], [3]) --- */

static void message_lock(void *ctx, uint32_t lock) { (void)ctx, (void)lock; }
static int16_t message_slot_of(void *ctx, uint32_t from) { (void)ctx; return drive_link.slot_of(from); }
static void message_send(void *ctx, int32_t to, const int16_t *words, int32_t size)
{
    (void)ctx;
    drive_link.send(to, words, size);
}

void track_viewer_drive_message(int16_t kind, uint32_t from, const uint8_t *data, int32_t size)
{
    enum { INPUTS = 2, SCHEDULE = 3, SCHEDULE_SIZE = 8 };
    if (!drive_link_on || drive_link_run == NULL) return;  /* none before the race runs, or after */
    RaceState *race = &drive_sim.race;
    if (kind == INPUTS && size >= (drive_link.host != 0 ? 4 : 2 + 2 * race->humans)) {  /* a word, or every human's */
        static int32_t unknown_4fb78c;
        InputMessageWords m = { 0, { 1, 0, drive_link.host, drive_link.slot, 0, 0 }, *drive_input.late, unknown_4fb78c,
                                drive_link_run->replay_due * 2 /* the clock again */, (int16_t)race->humans };
        static const InputMessageOps ops = { NULL, message_lock, message_lock, message_slot_of };
        input_on_message(&ops, drive_input.queues, &m, from, data);
        *drive_input.late = m.late, unknown_4fb78c = m.w4fb78c, drive_link_run->replay_due = clock_frames(m.w4c5598);
    } else if (kind == SCHEDULE && size >= SCHEDULE_SIZE) {
        uint8_t copy[SCHEDULE_SIZE];  /* 004109e4 writes the frame in place */
        memcpy(copy, data, sizeof copy);
        const RaceScheduleInWords w = { 1, drive_link.host, (int16_t)race->tick };
        const InputSendOps send = { NULL, message_send };
        race_schedule_on_message(&send, drive_pause.sched, &w, copy);
    }
}
