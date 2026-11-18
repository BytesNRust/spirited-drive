/* The frame of a road node, in integers only (the same results on every machine, as the engine's own maths).
 *
 * A TRI keeps the terrain rows and the scenery offsets in WORLD axes. To lay a cross-section along a curving road,
 * or to follow a road that moved, the offsets are turned between the world and the node's own axes:
 *   local = (right, up, along) of the road at the node; world = the TRI's (x, y, z), z being heading 0.
 * The frame is built from the node's three 14-bit angles (0x4000 = a turn) with the engine's fixed-point sine. */
#ifndef TNFS_TRACK_FRAME_H
#define TNFS_TRACK_FRAME_H

#include <stdint.h>

#include "formats/tri.h"

typedef struct {
    int32_t axis[3][3];   /* 16.16 world components of the local right, up and along axes (rows) */
} TrackFrame;

/* heading, slope and bank as in a TriNode (orientation, slope, slant_a): 14-bit angles, any value (wrapped). */
void track_frame_make(int32_t heading, int32_t slope, int32_t bank, TrackFrame *frame);
void track_frame_of_node(const TriNode *node, TrackFrame *frame);

/* A vector between the node's axes and the world's, each component rounded to the nearest integer.
 * The two are not exact inverses (the axes are orthonormal to about 1e-4, the sine being interpolated):
 * a round trip is off by 1.4e-4 of the biggest component, and 1 at rest.
 * |component| <= 2^30 (the i16 deltas times 0x200 are far inside). */
void track_frame_to_world(const TrackFrame *frame, const int32_t local[3], int32_t world[3]);
void track_frame_to_local(const TrackFrame *frame, const int32_t world[3], int32_t local[3]);

#endif
