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

static int32_t ring_records = TRACK_CACHE_RECORDS;

void track_cache_set_records(int32_t records)
{
    ring_records = records > TRACK_CACHE_RECORDS && records <= TRACK_CACHE_RECORDS_MAX ? records : TRACK_CACHE_RECORDS;
}

int32_t track_cache_records(void) { return ring_records; }

int32_t track_cache_size(void) { return ring_records * TRACK_SEGMENT_RECORD; }

/* 0043e244 */
void track_cache_load(const TrackCacheOps *ops, const TrackSegmentSource *src, int32_t circuit, uint8_t *cache,
                      int32_t segment)
{
    const uint32_t s = (uint32_t)segment & src->segment_mask;
    if ((int32_t)s < 0 && circuit == 0) return;
    const uint32_t offset = (uint32_t)x_mul((int32_t)(s & src->segment_mask), TRACK_SEGMENT_RECORD) % (uint32_t)track_cache_size();
    /* 32-bit address sums: a negative index or offset lands before the table, as in the original */
    const uint8_t *offsets = src->header + CHUNK_OFFSETS + 4 * (ptrdiff_t)(int32_t)(s & src->segment_mask);
    while (rd32(src->terrain + (ptrdiff_t)rd32(offsets)) != TRKD)
        ops->wait(ops->ctx);
    if (offset + TRACK_SEGMENT_RECORD > (uint32_t)track_cache_size()) return;
    track_segment_record(src, s, cache + offset);
}

/* the original's fold: above half the ring it takes ring - d, not d - ring */
static int32_t fold(int32_t d, int32_t ring)
{
    if (d > ring / 2) return x_sub(ring, d);
    if (d < x_sub(0, ring) / 2) return x_add(d, ring);
    return d;
}

/* 0043e300 */
void track_cache_window(const TrackWindowOps *ops, TrackCacheWindow *w, int32_t segment)
{
    const int unloaded = w->head == TRACK_WINDOW_UNLOADED;
    int32_t d = fold(x_sub(w->head, segment), w->ring);
    int32_t from_back = x_sub(segment, x_sub(w->head, w->ahead));
    if (from_back < 1) from_back = x_sub(0, from_back);
    if (fold(from_back, w->ring) > w->behind) {
        w->head = x_sub(x_add(w->ahead, segment), w->behind) & w->mask;
        d = x_sub(w->ahead, w->behind);
    }
    while (d < w->ahead) {
        w->loading = x_add(w->head, 1) & w->mask;
        ops->load(ops->ctx, w->head);
        w->head = x_add(w->head, 1) & w->mask;
        d = fold(x_sub(w->head, segment), w->ring);
    }
    while (d > w->ahead) {
        w->loading = x_sub(w->head, 1) & w->mask;
        ops->load(ops->ctx, x_sub(x_sub(w->head, 1), w->behind));
        w->head = x_sub(w->head, 1) & w->mask;
        d = fold(x_sub(w->head, segment), w->ring);
    }
    /* port: the loops above fill the window only from a start near segment 0 (the head's masked sentinel is 242, and `fold` leaves a jump from -9999
     * far past `behind`, so the jump's own reset never runs): anywhere else they load a few segments at the back and none ahead, so the road ahead
     * is empty. The segments ahead of the camera are loaded here, those the loops loaded again with the same record. */
    if (unloaded)
        for (int32_t i = 0; i < w->ahead; i++) ops->load(ops->ctx, x_add(segment, i));
}
