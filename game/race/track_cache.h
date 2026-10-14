/* The segment cache: a ring of 64 records of 0x21c bytes (005041d8) that
 * holds the segments about the camera, decoded from their TRKD chunks.
 *
 *   track_cache_load    0043e244
 *   track_cache_window  0043e300 */
#ifndef TNFS_TRACK_CACHE_H
#define TNFS_TRACK_CACHE_H

#include "game/race/track_geometry.h"

#include <stdint.h>

enum { TRACK_CACHE_SIZE = 0x8700 };

typedef struct {
    void *ctx;
    void (*wait)(void *ctx);   /* 00477008: services until the chunk is read */
} TrackCacheOps;

/* 0043e244 (watcall segment): segment & 00504210; on a stage (circuit 0) a
 * negative one loads nothing. Its chunk (as 0043dfd0 finds it) is waited
 * for until it starts with "TRKD", then decoded (0043dfd0) into the record
 * at (segment x 0x21c) mod 0x8700 of the cache. */
void track_cache_load(const TrackCacheOps *ops, const TrackSegmentSource *src, int32_t circuit, uint8_t *cache,
                      int32_t segment);

/* The window about the camera's segment (005041dc..e8, 0050420c, 00504210). */
typedef struct {
    int32_t head;     /* 005041dc: one past the newest segment loaded; -9999 after 0043e4e0 */
    int32_t loading;  /* 005041e0: the segment beside head as each load starts */
    int32_t behind;   /* 005041e4: 0x23 */
    int32_t ahead;    /* 005041e8: 0x1e */
    int32_t ring;     /* 0050420c: the TRI's ring, 0x8000 on a stage */
    int32_t mask;     /* 00504210: ring - 1, -1 on a stage */
} TrackCacheWindow;

typedef struct {
    void *ctx;
    void (*load)(void *ctx, int32_t segment);  /* 0043e244 */
} TrackWindowOps;

/* 0043e300 (watcall segment): moves the window to `segment`, loading the
 * segments that enter it at its front (forward) or ahead + behind back
 * (backward); a jump past `behind` starts it again behind the segment.
 * Distances fold into the ring as the original does, ring - d above half. */
void track_cache_window(const TrackWindowOps *ops, TrackCacheWindow *w, int32_t segment);

#endif
