/* The track renderer's walk over the scenery records (00509788) as the
 * original keeps it: the records placed by the build (0043eca4) form a run
 * of the ring starting at `first`, `count` long; the draws walk that run
 * back from its last record, farthest first, in step with the road rows.
 *
 *   track_objects_from_last  0044a804: the walker on the run's last record
 *   track_objects_back_to    0044a87c: and back from there to a node
 *   track_objects_start      0044a9b8: an empty run from the camera's segment */
#ifndef TNFS_TRACK_OBJECT_WALK_H
#define TNFS_TRACK_OBJECT_WALK_H

#include <stdint.h>

enum {
    TRACK_WALK_MAX_OBJECTS = 0xB4,  /* 0044a804: more and none is drawn */
    TRACK_WALK_RECORD_SIZE = 16     /* +0 node (dword), +4 descriptor (byte) */
};

typedef struct {
    const uint8_t *descriptors;  /* 00504204: 16 bytes each, +1 the point count (byte) */
    const uint8_t *records;   /* 00509788 */
    int32_t record_count;     /* 005041d0: records before the -1 node */
    int32_t first;            /* 005041c0: the run's first record */
    int32_t count;            /* 005041cc: the run's length, objects placed */
    int32_t vertex_count;     /* 005041b4: object points placed */
    int32_t vertex_total;     /* 005041b8: the same, kept apart by the build */
    int32_t cursor;           /* 005041c4: the walker's record */
    int32_t node;             /* 005041c8: its node, -1 when none */
    int32_t vertex_cursor;    /* 005041bc: the walker's object points */
    int32_t circuit;          /* 00504200: not 0 on a circuit */
    uint32_t node_mask;       /* 00504208: the ring's nodes - 1 */
    int32_t max_objects;      /* port: the count past which none is drawn; 0 is the original's 0xb4 (the ultra
                               * distance places more: track_render_set_reach) */
} TrackObjectWalk;

/* 0044a804 (): a count past 0xb4 (or max_objects) becomes 0; with objects, the walker on
 * the run's last record (first + count - 1, less record_count once if past
 * it), its node read, the point cursor at vertex_count; else node -1. */
void track_objects_from_last(TrackObjectWalk *w);

/* 0044a87c (watcall node): with objects (count > 0, no 0xb4 check), the
 * walker on the run's last record as above, then back one record at a
 * time, the point cursor less each record's descriptor point count, while
 * the record's node lies ahead of node & node_mask (0 < d <= the last d,
 * 9999 at first; after a d below -0x100 the new d) or, on a circuit, more
 * than 0x100 behind it; past record 0 to the last record, whose node is
 * -1 on a stage. */
void track_objects_back_to(TrackObjectWalk *w, int32_t node);

/* 0044a9b8 (watcall segment): unless record 0's node is negative (no
 * records), the run starts by bisection at a record about segment x 4,
 * back over the records sharing that record's node onto the one before
 * them, then forward while its node is below segment x 4 (not past the
 * last record); 0 otherwise. The walker on it, its node read; the counts 0. */
void track_objects_start(TrackObjectWalk *w, int32_t segment);

#endif
