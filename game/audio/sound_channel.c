#include "game/audio/sound_channel.h"

enum { FULL_VOLUME = 0x7f };

/* 00497750 */
void sound_channel_mix_volume(SoundChannel *c)
{
    c->mixed_volume = (uint8_t)(c->sound_volume * c->volume / FULL_VOLUME);
}
