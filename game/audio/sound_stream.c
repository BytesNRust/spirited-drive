#include "game/audio/sound_stream.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

enum { BLOCK_SIZE = 4, BLOCK_DATA = 8 };

/* A link outside the ring is a broken queue (the original reads wherever it points) */
static uint8_t *at(const SoundRing *r, uint32_t address)
{
    assert(address - r->base <= r->size - sizeof(uint32_t));
    return r->bytes + (address - r->base);
}

static uint32_t get(const SoundRing *r, uint32_t address)
{
    uint32_t w;
    memcpy(&w, at(r, address), sizeof w);
    return w;
}

static void put(const SoundRing *r, uint32_t address, uint32_t w) { memcpy(at(r, address), &w, sizeof w); }

/* idiv: the original faults on 0 and on INT32_MIN / -1; the port gives 0
 * and INT32_MIN */
static int32_t quotient(int32_t a, int32_t b)
{
    if (b == 0) return 0;
    if (b == -1) return (int32_t)(0u - (uint32_t)a);
    return a / b;
}

/* imul, wrapping */
static int32_t times(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }

static int32_t block_frames(const SoundStream *s, uint32_t block)
{
    return quotient((int32_t)(get(&s->ring, block + BLOCK_SIZE) - BLOCK_DATA), s->frame_bytes);
}

/* 0048dda8 */
void sound_stream_next(SoundStream *s, const uint8_t **block, int32_t *frames)
{
    const uint32_t head = s->head;
    if (head == 0) {
        *block = NULL, *frames = 0;
        return;
    }
    if (head != s->tail) {
        const uint32_t next = get(&s->ring, head);
        *block = at(&s->ring, next + BLOCK_DATA);
        *frames = block_frames(s, next);
        s->head = next;
        s->played++;
        put(&s->ring, head, SOUND_BLOCK_FREE);
        return;
    }
    if (get(&s->ring, head) == SOUND_BLOCK_END) {
        put(&s->ring, head, SOUND_BLOCK_FREE); /* 0048bde8 */
        *block = NULL, *frames = 0;
        s->state = 1;
        return;
    }
    *block = at(&s->ring, head + BLOCK_DATA);
    *frames = block_frames(s, head);
}

/* 0048e018 */
void sound_stream_drop(SoundStream *s)
{
    if (s->tail == 0 || s->head == 0) return; /* the original writes address 0 */
    put(&s->ring, s->tail, SOUND_BLOCK_FREE);
    uint32_t block = s->head;
    while (block != s->tail) {
        const uint32_t next = get(&s->ring, block);
        put(&s->ring, block, SOUND_BLOCK_FREE);
        block = next;
    }
    put(&s->ring, block, SOUND_BLOCK_FREE);
    s->head = block;
}

/* 0048e054 */
int32_t sound_stream_queued(const SoundStream *s, uint32_t unit)
{
    if (s->state == 0) return -1;
    uint32_t block = s->head;
    if (block == 0) return 0;
    int32_t bytes = 0;
    for (;;) {
        bytes = (int32_t)((uint32_t)bytes + get(&s->ring, block + BLOCK_SIZE));
        const uint32_t link = get(&s->ring, block);
        if (link != 0) block = link;
        if (block == SOUND_BLOCK_FREE || block == SOUND_BLOCK_END || get(&s->ring, block) == 0) break;
    }
    const int32_t frames = quotient(bytes, s->frame_bytes);
    switch (unit) {
    case SOUND_STREAM_BYTES: return bytes;
    case SOUND_STREAM_FRAMES: return times(frames, s->frame_scale);
    case SOUND_STREAM_TIME: return times(quotient((int32_t)((uint32_t)frames << 12), (int32_t)s->rate >> 4), s->frame_scale);
    default: return -1;
    }
}
