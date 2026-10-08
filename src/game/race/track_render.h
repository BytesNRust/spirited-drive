/* The race's track renderer: terrain of the segments ahead of the camera,
 * drawn far to near with per-segment levels of detail.
 *
 *   track_render_build      0043f48c / 0043eca4: the vertex list, a run of
 *                           selected terrain points per segment
 *   track_render_transform  0044aa94: to camera space, clamped, projected
 *   track_render_draw       0044b510: far segments (0044ad5c) whole, then the
 *                           4 nearest with subdivision (00425718)
 *
 * Fences, scenery, cars and shadows, which the original interleaves row by
 * row, are not drawn yet (TODO(0044a2b4), TODO(0044a4b0), TODO(00459eb8)). */
#ifndef TNFS_TRACK_RENDER_H
#define TNFS_TRACK_RENDER_H

#include "formats/tri.h"
#include "game/race/track_art.h"
#include "game/race/track_draw.h"

#include <stdint.h>

enum {
    TRACK_DRAW_SEGMENTS = 0x1C,   /* 00504220, set by 0043e4e0 */
    TRACK_NEAR_SEGMENTS = 4,      /* 0044b510 subdivides at most 4 */
    TRACK_MAX_VERTICES = 2048,    /* 0050503c holds 0x8700 / 12... ample */
    TRACK_WINDOW = TRACK_DRAW_SEGMENTS + 2
};

typedef struct {
    int valid;      /* 0 past the end of a stage */
    uint8_t lod;    /* 00504228 */
    int first;      /* 0050422a */
    int count;      /* 0050422c */
    uint8_t texture[10];
} TrackSegmentRecord;

typedef struct {
    int32_t position[3];   /* world 16.16 (camera struct +0x10) */
    int32_t matrix[9];     /* camera matrix (camera struct +0x94, 004496ac) */
    int32_t node;          /* road node the camera is on (camera struct +0x38) */
} TrackCamera;

typedef struct {
    const Tri *tri;
    const TrackArt *art;
    int32_t first_segment;
    TrackSegmentRecord segments[TRACK_WINDOW];
    int vertex_count;                        /* 00509780 */
    int32_t world[TRACK_MAX_VERTICES][3];    /* 0050503c */
    int32_t camera[TRACK_MAX_VERTICES][3];   /* 00506860 */
    int32_t screen[TRACK_MAX_VERTICES][2];   /* 00508084 */
} TrackRender;

/* 004496ac: camera matrix from pitch (+0x40), yaw (+0x44) and roll (+0x48),
 * 24-bit angles; mirrored for the "m" tracks (004fbad8). */
void track_camera_matrix(int32_t pitch, int32_t yaw, int32_t roll, int mirrored, int32_t out[9]);

void track_render_build(TrackRender *r, const Tri *tri, const TrackArt *art, int32_t camera_node);
void track_render_transform(TrackRender *r, const TrackCamera *cam, DrawContext *ctx);
void track_render_draw(const TrackRender *r, const TrackCamera *cam, const DrawContext *ctx, int mirrored);

#endif
