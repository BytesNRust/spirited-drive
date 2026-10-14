/* The segment cache: a ring of 64 records of 0x21c bytes (005041d8) that
 * holds the segments about the camera, decoded from their TRKD chunks.
 *
 *   track_cache_load  0043e244 */
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

#endif
