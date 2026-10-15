#include "game/audio/sound_driver.h"

#include <stddef.h>

static void channel_stopped(void *ctx, uint32_t slot)
{
    sound_channel_stopped(ctx, (int32_t)slot);
}

void sound_driver_wire(SoundDriver *d)
{
    for (int k = 0; k < SOUND_VOICES; k++) d->mixer.voice[k] = &d->voice[k];
    d->mixer.stopped = channel_stopped;
    d->mixer.ctx = &d->channels;
}

/* 00473c68 */
int32_t sound_stop(SoundDriver *d, int32_t handle)
{
    if (sound_handle_channel(&d->channels, handle) == NULL) return -1;
    sound_voice_stop(&d->mixer, (uint32_t)d->channels.handle[handle]); /* 00497a1c */
    return 0;
}

enum { NO_CHANNEL = -20 };

static int32_t channel_take(SoundDriver *d, uint32_t ch)
{
    d->channels.channel_cursor = ch + 1 == SOUND_CHANNELS ? 0 : ch + 1;
    return (int32_t)ch;
}

/* 00497640 */
int32_t sound_channel_pick(SoundDriver *d, uint32_t mask, int32_t priority)
{
    uint32_t ch = d->channels.channel_cursor;
    for (int n = 0; n < SOUND_CHANNELS; n++, ch = ch + 1 == SOUND_CHANNELS ? 0 : ch + 1)
        if ((mask & 1u << (ch & 31)) && !d->channels.channel[ch].busy) return channel_take(d, ch);
    ch = d->channels.channel_cursor;
    for (int n = 0; n < SOUND_CHANNELS; n++, ch = ch + 1 == SOUND_CHANNELS ? 0 : ch + 1) {
        if (!(mask & 1u << (ch & 31)) || (int32_t)d->channels.channel[ch].priority > priority) continue;
        channel_take(d, ch);
        sound_stop(d, d->channels.channel[ch].handle); /* 00497ab8: ch is 0..15 */
        return (int32_t)ch;
    }
    return NO_CHANNEL;
}
