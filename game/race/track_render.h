/* The race's track renderer: terrain of the segments ahead of the camera,
 * drawn far to near with per-segment levels of detail, on the original's
 * state (0043e4e0's: the segment cache, the nodes, the scenery records).
 *
 *   track_render_build      0043f540: the segment cache's window (0043e300,
 *                           loading through 0043e244), the vertex list
 *                           again when the camera's segment changes (0043f48c)
 *   track_render_transform  0044aa94: to camera space, a copy clamped and
 *                           projected
 *   track_render_draw       0044a804, then 0044b510: far segments (0044ad5c)
 *                           whole, then the 4 nearest with subdivision
 *
 * Scenery is placed while the vertex list is built (0043eca4) and drawn
 * after each row of road (0044a4b0), farthest first. Fences go up at the
 * end of each row band (0044a2b4). The cars (0045a848) and, in the near
 * pass, the skid marks (00459eb8) go out row by row through the sink. */
#ifndef TNFS_TRACK_RENDER_H
#define TNFS_TRACK_RENDER_H

#include "formats/tri.h"
#include "game/race/mirror_scene.h"
#include "game/race/track_art.h"
#include "game/race/track_build.h"
#include "game/race/track_cache.h"
#include "game/race/track_draw.h"
#include "game/race/track_fence.h"
#include "game/race/track_object_draw.h"
#include "game/race/track_objects.h"

#include <stdint.h>

enum {
    TRACK_DRAW_SEGMENTS = 0x1C,   /* 00504220, set by 0043e4e0 */
    TRACK_NEAR_SEGMENTS = 4,      /* 0044b510 subdivides at most 4 */
    TRACK_MAX_VERTICES = 4096,    /* 0050503c holds 0x8700 / 12; twice the 2048 the exe's 0x1c segments need, for
                                     the port's "ultra" reach */
    TRACK_MAX_OBJECT_VERTICES = 2048,
    TRACK_SEGMENT_ENTRIES = TRI_SEGMENT_CAPACITY + 64,   /* 00504228: a stage indexes it by segment */
    TRACK_NODE_SPARE = 4 * 64                           /* nodes read past a stage's last */
};

typedef struct {
    int32_t position[3];   /* world 16.16 (camera struct +0x10) */
    int32_t matrix[9];     /* camera matrix (camera struct +0x94, 004496ac) */
    int32_t node;          /* road node the camera is on (camera struct +0x38) */
} TrackCamera;

typedef struct {
    const Tri *tri;
    const TrackArt *art;
    int draw_segments;                       /* 00504220 */
    int vertex_count;                        /* 00509780 */
    int32_t world[TRACK_MAX_VERTICES][3];    /* 0050503c */
    int32_t camera[TRACK_MAX_VERTICES][3];   /* 00506860 */
    int32_t clamped[TRACK_MAX_VERTICES][3];  /* 00510764: camera clamped and pushed, what is projected */
    int32_t screen[TRACK_MAX_VERTICES][2];   /* 00508084 */
    int object_count;                        /* 005041cc */
    int object_vertex_count;                 /* 005041b4 */
    int32_t object_world[TRACK_MAX_OBJECT_VERTICES][3];   /* 004fd134 */
    int32_t object_camera[TRACK_MAX_OBJECT_VERTICES][3];  /* 004ffb64 */
    int32_t object_screen[TRACK_MAX_OBJECT_VERTICES][2];  /* 00502594 */
    /* the original's state the build and the draws work on */
    uint8_t cache[TRACK_CACHE_SIZE_MAX];                                  /* 005041d8: 0x8700 in the exe; port_follow_ring more (track_cache.h) */
    uint8_t nodes[(TRI_NODE_CAPACITY + TRACK_NODE_SPARE) * TRACK_BUILD_NODE_SIZE];  /* 00505038 */
    uint8_t descriptors[256 * TRI_DESCRIPTOR_SIZE];                       /* 00504204 */
    uint8_t records[(TRI_SCENERY_MAX + 1) * TRI_SCENERY_SIZE];            /* 00509788 */
    uint8_t knocked[2 * TRI_SCENERY_MAX];                                 /* 004b9098 */
    TrackSegmentEntry entries[TRACK_SEGMENT_ENTRIES];                     /* 00504228 */
    TrackBuild build;                        /* its segments: entries; the walk: 005041b4..d0, 00504200..08 */
    TrackCacheWindow window;                 /* 005041dc..e8, 0050420c, 00504210 */
    int32_t reach;                           /* port: the segments drawn when the track is bound; 0 the exe's 0x1c */
    uint32_t side_face;                      /* 00511fac */
    uint8_t quarter;                         /* 0051204c */
    MirrorBounds bounds;                     /* 00511f88 */
    /* port (the light effects' light, track_node_light.h): the intensity of each point of `world`, the light the track is built
     * with (NULL none) and the four corners' of the terrain polygon being drawn (+ 1 when it is) */
    uint8_t light_vertex[TRACK_MAX_VERTICES];
    const TrackNodeLight *node_light;
    uint8_t light_now[6];   /* the four corners, 1 when the polygon is lit, 1 when its corners are all in a tunnel's rows */
    uint8_t tunnel_vertex[TRACK_MAX_VERTICES];   /* 1 for the points of `world` in a tunnel's row */
    uint32_t vertex_node[TRACK_MAX_VERTICES];    /* port (headlight_beams): the node and the row point each point of `world` is of */
    uint8_t vertex_point[TRACK_MAX_VERTICES];
} TrackRender;

/* 004496ac: camera matrix from pitch (+0x40), yaw (+0x44) and roll (+0x48),
 * 24-bit angles; mirrored for the "m" tracks (004fbad8). */
void track_camera_matrix(int32_t pitch, int32_t yaw, int32_t roll, int mirrored, int32_t vertical_scale, int32_t out[9]);
/* 004496ac's last product (camera struct +0xb8, the mirror's): yaw, roll and
 * pitch as for the front, then a half turn (0x800000) about y; no scale, no
 * mirroring. */
void track_camera_rear_matrix(int32_t pitch, int32_t yaw, int32_t roll, int32_t out[9]);

/* 0044a1c4: a world point to the screen ((world - position) >> shift through
 * the matrix), pushed onto the near plane along the road direction first. */
void track_point_screen(const TrackCamera *cam, const DrawContext *ctx, const int32_t world[3], int32_t camera[3],
                        int32_t screen[2]);

/* 0044a450: 1 when the x of each of four screen points (x, y pairs) is
 * within 0x800 of 0, else 0. */
int track_quad_on_screen(const int32_t s[8]);

/* 00449f0c / 0048f750: twice the signed area of the screen triangle whose
 * points (x, y pairs in s) tri names, its first and last swapped on
 * mirrored tracks (004fbad8); 32-bit wrap as in the original. */
int32_t track_winding(const int32_t *s, const int tri[3], int mirrored);

/* 00449f90 (watcall points, strip): 0 when the quad's four x are all
 * right of the camera's box or all left of it, or its points 0 and 3 are
 * both below it; else 1 when one of the triangles 004c5320 / 2c / 38
 * faces the camera (track_winding < 0 for strips 0..4, > 0 for the rest). */
int track_visible_near(const MirrorBounds *b, const int32_t s[8], int strip, int mirrored);

/* 0044a0d0 (watcall points, strip): 0 when the four points share an
 * outcode against the clip (00487b0e); else 1 when the triangle 004c5344 or
 * 004c5350 (the values of 004c5320's first two) faces the camera as for
 * track_visible_near. */
int track_visible_far(const ClipRect *clip, const int32_t s[8], int strip, int mirrored);

/* 004497d8 (watcall segment): the road direction across the camera's
 * segment, camera[next] - camera[0] (next: the segment's point count, the
 * short at 0050422c), 32-bit wrap; z of magnitude below 0x1999 (INT32_MIN
 * too) becomes 0x1999. */
void track_near_direction(const int32_t (*camera)[3], int32_t next, int32_t dir[3]);

void track_render_build(TrackRender *r, const Tri *tri, const TrackArt *art, int32_t camera_node);

/* port: the segments drawn: 8..DRAW_SEGMENTS_ULTRA (race_detail.h) as the frame asks for them, the exe's 0x1c for 0x1c
 * or any other value; the cache window keeps its two segments ahead and five behind. Taken at once, and at every later
 * bind. */
void track_render_set_reach(TrackRender *r, int32_t segments);

/* port (port_follow_ring): the cache's ring at `records` (track_cache_set_records); when it changes the window starts again, its records being where the old size put them */
void track_render_set_ring(TrackRender *r, int32_t records);

/* port: the the console version's baked light on the terrain's polygons (NULL none): the track is built again with it at the next frame */
void track_render_set_light(TrackRender *r, const TrackNodeLight *light);

/* port (headlight_beams): the light and tunnel flag of every point the track was built with, asked again of the light as it is now (the pools move each tick, the build only
 * when the camera changes segment) */
void track_render_relight(TrackRender *r);
void track_render_transform(TrackRender *r, const TrackCamera *cam, DrawContext *ctx);
/* 0044aa94's near plane, 004c52c0 >> the shift; with the port's port_near_scale (16.16) brought in by it first */
int32_t track_render_near(const DrawContext *ctx);
/* frame: the animation counter (00513e70). */
void track_render_draw(TrackRender *r, const TrackCamera *cam, const DrawContext *ctx, int mirrored, int32_t frame);

/* port (the follow camera's road behind, follow_behind.h): the track's draws as the sink gets them, for a caller with its
 * own vertex lists. The glue is what the ops' ctx points at, so it outlives the draws. */
typedef struct {
    const TrackRender *r;
    const TrackCamera *cam;
    const DrawContext *ctx;
    const uint8_t *light_now;        /* port (baked light): the corners of the polygon being drawn, as TrackRender's light_now; NULL the render's own */
    const TrackObjectWalk *walk;     /* port (baked light): the walker of the objects drawn, the node their light is of; NULL the render's own */
} TrackGlue;
void track_render_fence_ops(const TrackGlue *g, TrackFenceOps *out);
void track_render_object_ops(const TrackGlue *g, TrackObjectDrawOps *out);
/* the road point as 0044aa94 projects it: x and y clamped, then in front of `near` pushed onto it along `dir` (z of at
 * least 0x1999 for the push to be a distance) */
void track_render_push_near(int32_t v[3], int32_t near, const int32_t dir[3]);

#endif
