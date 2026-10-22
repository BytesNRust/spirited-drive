/* `tnfs --track NAME`: development fly-through of a track (simdata/misc/NAME.TRI
 * with its simdata/ntrackfm/NAME_[TMR]01.FAM textures). Up/Down move along the
 * road, Left/Right turn the camera, Space switches between the ported
 * renderer and the earlier stand-in (which also draws the scenery; with
 * --screenshot, TNFS_STAND_IN=1 picks it). */
#include "viewer/track_viewer.h"
#include "viewer/track_load.h"

#include "viewer/screen_layout.h"
#include "viewer/drive_replay.h"
#include "viewer/ported_view.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const float UNIT = 65536.0f;       /* node positions are 16.16 */
static const float EYE_HEIGHT = 1.5f;

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
    ported_view_set_models(t->models, t->model_ok, 0);
    if (t->car_ok) ported_view_set_car(&t->car[0], &t->car[1], CAR_AHEAD);
    ported_view_render(&t->tri, &t->art, &t->info, node, (int32_t)(yaw / (2.0f * 3.14159265f) * 0x1000000), pixels);
}

static int write_ppm(const char *path, const uint8_t *pixels, const uint8_t *rgb)
{
    return track_viewer_write_ppm(path, pixels, SCREEN_W, SCREEN_H, rgb);
}

static int run_window(const Track *t, uint8_t *pixels, uint32_t node)
{
    const PlatformConfig config = { SCREEN_W, SCREEN_H, "TNFS track viewer", 0, 0, 0 };
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

int track_viewer_main(const char *data_root, const char *name, uint32_t node, const char *screenshot)
{
    Track t;
    const TrackFamInputs look = { 0, 0, 0, race_sim_track_index(name), NULL, NULL, NULL, 0 };
    int ok = track_viewer_load(&t, data_root, name, &look);
    if (ok && getenv("TNFS_CAR") != NULL) track_viewer_load_car(&t, data_root, getenv("TNFS_CAR"));
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
    track_viewer_free(&t);
    return ok;
}
