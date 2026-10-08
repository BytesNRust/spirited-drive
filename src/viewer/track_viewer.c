/* `tnfs --track NAME`: development fly-through of a track (simdata/misc/NAME.TRI
 * with its simdata/ntrackfm/NAME_[TMR]01.FAM textures). Up/Down move along the
 * road, Left/Right turn the camera. With --screenshot it renders one frame
 * headless. Geometry and textures come from the ported loaders; the drawing
 * is the viewer's own (viewer/raster.h), not the original renderer yet. */
#include "viewer/track_viewer.h"

#include "formats/chunk.h"
#include "formats/tri.h"
#include "game/race/track_art.h"
#include "game/race/track_geometry.h"
#include "platform/platform.h"
#include "viewer/raster.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SCREEN_W = 640,
    SCREEN_H = 480,
    DRAW_SEGMENTS = 40,   /* segments ahead of the camera */
    STRIPS = 10,          /* 5 on each side of the centre */
    SKY_COLOUR = 0
};

static const float UNIT = 65536.0f;       /* node positions are 16.16 */
static const float EYE_HEIGHT = 1.5f;

typedef struct {
    uint8_t *tri_file;
    uint8_t *fam_file;
    Tri tri;
    Chunk fam;
    TrackArt art;
} Track;

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    Vec3 eye, forward, right, up;
} Camera;

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
    return 1;
}

static void free_track(Track *t)
{
    platform_free_file(t->tri_file);
    platform_free_file(t->fam_file);
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
    const Vec3 d = { p.x - c->eye.x, p.y - c->eye.y, p.z - c->eye.z };
    return (RasterVertex){ d.x * c->right.x + d.y * c->right.y + d.z * c->right.z,
                           d.x * c->up.x + d.y * c->up.y + d.z * c->up.z,
                           d.x * c->forward.x + d.y * c->forward.y + d.z * c->forward.z, u, v };
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

static void render(const Track *t, uint32_t node, float yaw, uint8_t *pixels)
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
    }
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
    for (int running = 1; running && platform_poll_events();) {
        for (PlatformKey key; (key = platform_next_key()) != PLATFORM_KEY_NONE;)
            if (key == PLATFORM_KEY_ESCAPE) running = 0;
        if (platform_key_down(0x48)) node = (node + 1) % t->tri.node_count;
        if (platform_key_down(0x50)) node = (node + t->tri.node_count - 1) % t->tri.node_count;
        if (platform_key_down(0x4B)) yaw -= 0.03f;
        if (platform_key_down(0x4D)) yaw += 0.03f;
        render(t, node, yaw, pixels);
        platform_present(pixels, palette);
        platform_wait_vblank();
    }
    platform_shutdown();
    return 1;
}

int track_viewer_main(const char *data_root, const char *name, uint32_t node, const char *screenshot)
{
    Track t;
    int ok = load_track(&t, data_root, name);
    uint8_t *pixels = ok ? malloc(SCREEN_W * SCREEN_H) : NULL;
    ok = ok && pixels != NULL;
    if (ok) {
        printf("%s: %u segments, %s\n", name, t.tri.segment_count, tri_is_circuit(&t.tri) ? "circuit" : "stage");
        node %= t.tri.node_count;
        if (screenshot != NULL) {
            render(&t, node, 0, pixels);
            ok = write_ppm(screenshot, pixels, t.art.palette);
        } else {
            ok = run_window(&t, pixels, node);
        }
    }
    free(pixels);
    free_track(&t);
    return ok;
}
