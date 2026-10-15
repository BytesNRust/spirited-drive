#include "game/audio/music_reader.h"

#include <string.h>

enum { CHUNK_HEADER = 8, CHUNK_SIZE = 4 };

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t ring_end(const MusicReader *r) { return r->ring.base + r->ring.size; }

void music_reader_open(MusicReader *r, SoundRing ring, const uint8_t *file, size_t size)
{
    *r = (MusicReader){ .ring = ring, .file = file, .size = size, .oldest = ring.base, .write = ring.base };
}

void music_reader_seek(MusicReader *r, uint32_t offset) { r->at = offset; }

/* 0048bc28: the oldest blocks the stream has played made free again */
static void reclaim(MusicReader *r)
{
    while (r->blocks > 0) {
        if (r->oldest == r->wrapped_at) {
            r->oldest = r->ring.base;
            r->wrapped_at = 0;
            continue;
        }
        if (sound_ring_get(&r->ring, r->oldest) != SOUND_BLOCK_FREE) return;
        r->oldest += sound_ring_get(&r->ring, r->oldest + CHUNK_SIZE);
        r->blocks--;
    }
    r->oldest = r->write = r->ring.base;
    r->wrapped_at = 0;
}

/* Where a block of `size` bytes goes, 0 when the ring has no room for it */
static uint32_t place(MusicReader *r, uint32_t size)
{
    if (r->blocks > 0 && r->write <= r->oldest) return r->oldest - r->write >= size ? r->write : 0;
    if (ring_end(r) - r->write >= size) return r->write;
    if (r->blocks > 0 && r->oldest - r->ring.base < size) return 0;
    if (r->blocks > 0) r->wrapped_at = r->write;
    return r->ring.base;
}

uint32_t music_reader_next(MusicReader *r)
{
    if (r->at > r->size || r->size - r->at < CHUNK_HEADER) return 0;
    const uint32_t size = u32(r->file + r->at + CHUNK_SIZE);
    if (size < CHUNK_HEADER || size > r->size - r->at || size > r->ring.size) return 0;
    reclaim(r);
    const uint32_t block = place(r, size);
    if (block == 0) return 0;
    memcpy(r->ring.bytes + (block - r->ring.base), r->file + r->at, size);
    r->write = block + size;
    r->blocks++;
    r->at += size;
    return block;
}
