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
