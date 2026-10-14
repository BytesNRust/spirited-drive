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

enum { TRACK_SEGMENT_RECORD = 0x21c };

/* What 0043dfd0 reads, as 0043e4e0 leaves it. */
typedef struct {
    const uint8_t *header;    /* 005041fc: +0x2c the segments' chunk offsets, a dword each */
    const uint8_t *terrain;   /* 005041f4: the chunks */
    const uint8_t *nodes;     /* 00505038: 0x24 bytes a node, +8 its position */
    uint32_t segment_mask;    /* 00504210 */
    uint32_t node_mask;       /* 00504208 */
} TrackSegmentSource;

/* 0043dfd0 (watcall segment, record): the record of a segment as the cache
 * holds it: the chunk's bytes +0xc..+0x17, then 4 rows of 11 points (12
 * bytes each), the row's node (segment x 4 + row, masked) position plus the
 * centre's delta, then two chains of 5 from the centre outward, each point
 * the one before plus its delta (deltas i16 x 0x200 at chunk +0x18, 0x42 a
 * row; 32-bit sums). */
void track_segment_record(const TrackSegmentSource *src, uint32_t segment, uint8_t record[TRACK_SEGMENT_RECORD]);

#endif
