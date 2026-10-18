#include "game/audio/sound_queue.h"

#include <stddef.h>

/* 00492438 */
int32_t sound_queue_setup(const SoundDriver *d, SoundQueue *q, int32_t channel_a, int32_t channel_b, uint32_t volume)
{
    if (!d->channels.open) return -1; /* 00492324 */
    if (channel_a < 0 || channel_a >= SOUND_CHANNELS || channel_b < 0 || channel_b >= SOUND_CHANNELS) return -1;
    q->playing = 0;
    for (int i = 0; i < SOUND_QUEUE_ENTRIES; i++) q->entry[i].sample = NULL; /* 004923f0 */
    q->channel_a = channel_a, q->channel_b = channel_b, q->channel = channel_a;
    q->write = 0, q->read = 0;
    q->volume = (int32_t)(volume & 0x7f);
    q->ready = 1;
    return 0;
}
