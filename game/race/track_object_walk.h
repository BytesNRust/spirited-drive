/* The track renderer's walk over the scenery records (00509788) as the
 * original keeps it: the records placed by the build (0043eca4) form a run
 * of the ring starting at `first`, `count` long; the draws walk that run
 * back from its last record, farthest first, in step with the road rows.
 *
 *   track_objects_from_last  0044a804: the walker on the run's last record */
#ifndef TNFS_TRACK_OBJECT_WALK_H
#define TNFS_TRACK_OBJECT_WALK_H

#include <stdint.h>

enum {
    TRACK_WALK_MAX_OBJECTS = 0xB4,  /* 0044a804: more and none is drawn */
    TRACK_WALK_RECORD_SIZE = 16     /* +0 node (dword), +4 descriptor (byte) */
};

typedef struct {
    const uint8_t *records;   /* 00509788 */
    int32_t record_count;     /* 005041d0: records before the -1 node */
    int32_t first;            /* 005041c0: the run's first record */
    int32_t count;            /* 005041cc: the run's length, objects placed */
    int32_t vertex_count;     /* 005041b4: object points placed */
    int32_t cursor;           /* 005041c4: the walker's record */
    int32_t node;             /* 005041c8: its node, -1 when none */
    int32_t vertex_cursor;    /* 005041bc: the walker's object points */
} TrackObjectWalk;

/* 0044a804 (): a count past 0xb4 becomes 0; with objects, the walker on
 * the run's last record (first + count - 1, less record_count once if past
 * it), its node read, the point cursor at vertex_count; else node -1. */
void track_objects_from_last(TrackObjectWalk *w);

#endif
