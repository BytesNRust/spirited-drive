#include "game/frontend/menu_sound.h"

static int16_t short_at(const ConfigDat *c, uint32_t at)
{
    const uint8_t *b = c->image + (at - 0x00524df0);
    return (int16_t)(uint16_t)(b[0] | b[1] << 8);
}

/* 0046492c */
void menu_sound_play(MenuSound *m, const ConfigDat *c, int sound, const MenuSoundOps *ops)
{
    enum { CENTRE = 0x40, SOUND_4A = 0x4a };
    if (!m->sound_on) return;
    const int32_t voice = ops->voice_start(ops->ctx, (int16_t)sound, CENTRE, short_at(c, 0x00524e69), 0);
    if ((int16_t)sound == SOUND_4A) m->voice_4a = voice;
    else m->voice = voice;
}
