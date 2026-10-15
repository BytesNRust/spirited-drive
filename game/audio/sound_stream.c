#include "game/audio/sound_stream.h"

#include <stddef.h>
#include <string.h>

enum { BLOCK_SIZE = 4, BLOCK_DATA = 8 };

static uint8_t *at(const SoundRing *r, uint32_t address) { return r->bytes + (address - r->base); }

static uint32_t get(const SoundRing *r, uint32_t address)
{
    uint32_t w;
    memcpy(&w, at(r, address), sizeof w);
    return w;
}

static void put(const SoundRing *r, uint32_t address, uint32_t w) { memcpy(at(r, address), &w, sizeof w); }

/* idiv: the original faults on 0 and on INT32_MIN / -1; the port gives 0
 * and INT32_MIN */
static int32_t block_frames(const SoundStream *s, uint32_t block)
{
    const int32_t bytes = (int32_t)(get(&s->ring, block + BLOCK_SIZE) - BLOCK_DATA);
    if (s->frame_bytes == 0) return 0;
    if (s->frame_bytes == -1) return (int32_t)(0u - (uint32_t)bytes);
    return bytes / s->frame_bytes;
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
