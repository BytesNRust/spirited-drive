/* Track terrain in world coordinates: the decoded form of a TRI segment that
 * the race keeps in its 0x8700-byte segment cache (0043e244 -> 0043dfd0).
 *
 * Each row belongs to one road node. Its 11 points are the road centre and two
 * chains of 5 points running outward, each point a delta from the previous
 * one. Deltas are stored as i16 and scaled by 512 (<< 9) onto the node
 * position, so all coordinates share the node's fixed-point scale. */
#ifndef TNFS_TRACK_GEOMETRY_H
#define TNFS_TRACK_GEOMETRY_H

#include "formats/tri.h"

#include <stdint.h>

enum {
    TRACK_ROW_POINTS = 11,  /* 0 centre, 1-5 first chain, 6-10 second chain */
    TRACK_CHAIN_POINTS = 5,
    TRACK_SEGMENT_ROWS = TRI_NODES_PER_SEGMENT
};

typedef struct {
    int32_t x, y, z;
} TrackPoint;

/* 0x21c bytes in the original: the chunk's 12 header bytes, then 4 rows. */
typedef struct {
    uint8_t unknown_0c;            /* chunk +0xc */
    uint8_t fence;                 /* chunk +0xd, TRI_FENCE_* */
    uint8_t texture[10];           /* chunk +0xe, texture slots (track_art.h) */
    TrackPoint rows[TRACK_SEGMENT_ROWS][TRACK_ROW_POINTS];
} TrackSegment;

/* 0043dfd0. Node positions come from nodes segment * 4 .. + 3 (masked by the
 * ring size as the original does). */
TriResult track_decode_segment(const Tri *tri, uint32_t segment, TrackSegment *out);

#endif
