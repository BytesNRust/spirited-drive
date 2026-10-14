#include "game/race/track_cache.h"

#include "game/gfx/x86_int.h"

#include <stddef.h>

enum {
    CHUNK_OFFSETS = 0x2c,
    TRKD = 0x444b5254   /* "TRKD" */
};

static int32_t rd32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

/* 0043e244 */
void track_cache_load(const TrackCacheOps *ops, const TrackSegmentSource *src, int32_t circuit, uint8_t *cache,
                      int32_t segment)
{
    const uint32_t s = (uint32_t)segment & src->segment_mask;
    if ((int32_t)s < 0 && circuit == 0) return;
    const uint32_t offset = (uint32_t)x_mul((int32_t)(s & src->segment_mask), TRACK_SEGMENT_RECORD) % TRACK_CACHE_SIZE;
    /* 32-bit address sums: a negative index or offset lands before the table, as in the original */
    const uint8_t *offsets = src->header + CHUNK_OFFSETS + 4 * (ptrdiff_t)(int32_t)(s & src->segment_mask);
    while (rd32(src->terrain + (ptrdiff_t)rd32(offsets)) != TRKD)
        ops->wait(ops->ctx);
    if (offset + TRACK_SEGMENT_RECORD > TRACK_CACHE_SIZE) return;
    track_segment_record(src, s, cache + offset);
}
