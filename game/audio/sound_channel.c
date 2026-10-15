#include "game/audio/sound_channel.h"

#include <stddef.h>

enum {
    FULL_VOLUME = 0x7f,
    RAND_MUL = 0x41c64e6d,
    RAND_ADD = 0x3039,
    RAND_MAX15 = 0x7fff,
    SPREAD_MAX = 0x10000,
    SPREAD_HALF = 0x4000
};

/* 00487bf8 */
int32_t sound_rand(uint32_t *seed)
{
    *seed = *seed * RAND_MUL + RAND_ADD;
    return (int32_t)(*seed >> 16 & RAND_MAX15);
}

/* 00497b64 */
int32_t sound_spread(uint32_t *seed, int32_t spread)
{
    const int32_t amount = spread > SPREAD_MAX ? SPREAD_MAX : spread < 0 ? 0 : spread;
    return (sound_rand(seed) - SPREAD_HALF) * amount / SPREAD_HALF;
}

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

/* 00497af8 */
SoundChannel *sound_handle_channel(SoundChannels *s, int32_t handle)
{
    if (sound_handle_check(s, handle) < 0) return NULL;
    return &s->channel[s->handle[handle]];
}

/* 00497980 */
int32_t sound_handle_free(SoundChannels *s, int32_t handle)
{
    if (handle < 0 || handle >= SOUND_HANDLES) return -1;
    const int32_t channel = s->handle[handle];
    if (channel < 0 || channel >= SOUND_CHANNELS) return -1;
    s->channel[channel].busy = 0;
    s->handle[handle] = SOUND_HANDLE_FREE;
    return 0;
}

/* 00497870 */
int32_t sound_channel_stopped(SoundChannels *s, int32_t slot)
{
    const int32_t handle = slot < 0 || slot >= SOUND_CHANNELS ? -1 : s->channel[slot].handle;
    return sound_handle_free(s, handle);
}

/* 00473d20 */
int32_t sound_handle_stopped(const SoundChannels *s, int32_t handle)
{
    return sound_handle_check(s, handle) < 0;
}
