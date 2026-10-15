#include "game/audio/sound_channel.h"

enum { FULL_VOLUME = 0x7f };

/* 00497750 */
void sound_channel_mix_volume(SoundChannel *c)
{
    c->mixed_volume = (uint8_t)(c->sound_volume * c->volume / FULL_VOLUME);
}

static int handle_take(SoundChannels *s, uint32_t i, int32_t channel)
{
    if (s->handle[i] != SOUND_HANDLE_FREE) return 0;
    s->handle[i] = channel;
    s->channel[channel].handle = (uint8_t)i;
    s->handle_cursor = i + 1 == SOUND_HANDLES ? 0 : i + 1;
    return 1;
}

/* 004978a0 */
int32_t sound_handle_alloc(SoundChannels *s, int32_t channel)
{
    if (channel < 0 || channel >= SOUND_CHANNELS) return -1;
    const uint32_t from = s->handle_cursor;
    for (uint32_t i = from; i < SOUND_HANDLES; i++)
        if (handle_take(s, i, channel)) return (int32_t)i;
    for (uint32_t i = 0; i < from; i++)
        if (handle_take(s, i, channel)) return (int32_t)i;
    return -1;
}

/* 00497a34 */
int32_t sound_handle_check(const SoundChannels *s, int32_t handle)
{
    if (!s->open || handle < 0 || handle >= SOUND_HANDLES) return -1;
    const int32_t channel = s->handle[handle];
    if (channel < 0 || channel >= SOUND_CHANNELS) return -1;
    return s->channel[channel].busy ? 0 : -1;
}
