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

enum { EACS_HEADER = 0x20 };

/* 004924f8 */
int32_t sound_queue_add(const SoundDriver *d, SoundQueue *q, SoundSample *s, const uint8_t *file, uint32_t percent)
{
    if (!d->channels.open || !q->ready || s == NULL) return -1;
    s->data = file + EACS_HEADER;
    const int32_t at = q->write++;
    if (q->write == SOUND_QUEUE_WRITTEN) q->write = 0;
    q->entry[at].sample = s;
    q->entry[at].until = s->rate * percent / 100;
    return 0;
}

/* 004925a8 */
void sound_queue_play(SoundQueue *q)
{
    q->playing = 1;
}

/* 004923f8 */
static void queue_clear(SoundQueue *q)
{
    q->playing = 0;
    for (int i = 0; i < SOUND_QUEUE_ENTRIES; i++) q->entry[i].sample = NULL;
    q->write = 0, q->read = 0;
    q->channel = q->channel_a;
}

/* 00492620 */
void sound_queue_stop(SoundDriver *d, SoundQueue *q)
{
    if (!d->channels.open || !q->ready) return;
    q->playing = 0;
    sound_stop(d, q->last);
    sound_stop(d, q->handle);
    queue_clear(q);
    q->ready = 0;
}

enum { QUEUE_PAN = 0x40 };

/* 00492330 */
void sound_queue_tick(SoundDriver *d, SoundQueue *q)
{
    if (!q->playing) return;
    const int32_t at = q->read;
    const SoundQueueEntry *e = &q->entry[at];
    if (sound_frames_left(d, q->handle) > (int32_t)e->until) return;
    if (at == q->write) {
        if (sound_handle_stopped(&d->channels, q->handle)) q->playing = 0;
        return;
    }
    if (++q->read == SOUND_QUEUE_ENTRIES) q->read = 0;
    q->channel = q->channel == q->channel_a ? q->channel_b : q->channel_a;
    q->last = q->handle;
    q->handle = sound_play_on(d, e->sample, q->channel, QUEUE_PAN, (uint8_t)q->volume);
}
