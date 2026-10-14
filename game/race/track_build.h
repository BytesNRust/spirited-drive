/* The vertex list of the segments ahead, built as the original keeps it:
 * per segment the LOD's selected terrain points of each row, and, row by
 * row, the scenery records on that row's node placed as object points
 * through the object walk (track_object_walk.h).
 *
 *   track_build_segment  0043eca4
 *   track_build          0043f48c: the segments ahead of the camera's node */
#ifndef TNFS_TRACK_BUILD_H
#define TNFS_TRACK_BUILD_H

#include "game/race/track_object_walk.h"

#include <stdint.h>

enum {
    TRACK_BUILD_SEGMENT_SIZE = 0x21C,   /* a segment record in the cache: +0xc the rows */
    TRACK_BUILD_CACHE_SIZE = 0x8700,
    TRACK_BUILD_ROW_SIZE = 0x84,        /* 11 points of 12 bytes */
    TRACK_BUILD_NODE_SIZE = 0x24        /* +7 property, +8 position, +0x18 heading */
};

typedef struct {
    uint8_t lod;    /* 00504228 */
    int16_t first;  /* 0050422a: the segment's first point */
    int16_t count;  /* 0050422c: its point count */
} TrackSegmentEntry;

typedef struct {
    const uint8_t *cache;          /* 005041d8: the segment records, a ring of 0x8700 bytes */
    const uint8_t *nodes;          /* 00505038 */
    const uint8_t *knocked;        /* 004b9098: two bytes per record, the first not 0 once knocked over */
    uint32_t segment_mask;         /* 00504210 */
    int32_t (*world)[3];           /* 0050503c */
    int32_t world_capacity;
    int32_t world_next;            /* 0050977c */
    int32_t vertex_count;          /* 00509780 */
    TrackSegmentEntry *segments;   /* 00504228, segment_mask + 1 of them */
    int32_t (*object_world)[3];    /* 004fd134 */
    int32_t object_capacity;
    TrackObjectWalk walk;
    int32_t first_segment;         /* 00504214 */
    int32_t first_node;            /* 00504218 */
} TrackBuild;

/* 0043eca4 (watcall first, segment, rows, objects): the LOD of the segment
 * (by the property of its first node and its distance from `first`); for
 * each of `rows` rows, the LOD's points copied from the cache into world,
 * then with `objects` every record on that row's node placed: a model's
 * position (node + offset << 8), or a quad of four corners (half the width
 * either side along the record's heading, the height up), or lying flat
 * when knocked over; two-sided ones two corners more. The segment's entry
 * (lod, first point, count) set and vertex_count raised by the count. */
void track_build_segment(TrackBuild *b, int32_t first, int32_t segment, int32_t rows, int objects);

/* 0043f48c (watcall node, segment, count): from the camera's node, its
 * segment (node / 4) kept with it and the lists emptied, the object run
 * started there (0044a9b8, on the masked segment), then count - 1 segments
 * from `segment` built with their objects, the next without them and the
 * one after it a single row. */
void track_build(TrackBuild *b, int32_t node, int32_t segment, int32_t count);

#endif
