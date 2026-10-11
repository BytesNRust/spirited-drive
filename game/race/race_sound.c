/* The race's sound levels (race_sound.h). */
#include "game/race/race_sound.h"

enum {
    PAN_CENTRE = 0x40,            /* angle >> 16: 0x40 a quarter turn */
    PAN_BACK_FROM = 0xc0,         /* 0xc0..0x100: the last quarter, back to centre */
    PAN_BACK_TO = 0x100,
    PAN_BACK = 0x13f,
    PAN_MAX = 0x7f,
    HEARD_WITHIN = 0x200000,
    LOUDNESS_MAX = 0x7f,
    LOUDNESS_SHIFT = 8,
    LOUDNESS_DIVIDE = 0x2000,     /* 0x200000 >> 8 */
    LEVEL_MAX = 0x7f,
    FRESH_FROM = 0x20,            /* sounds started anew each time */
    FRESH_TO = 0x25,
    FRESH_TOO = 0x50
};

int32_t race_sound_pan(int32_t angle)
{
    int32_t a = angle >> 16;
    int32_t pan;
    if (a <= PAN_CENTRE && a >= 0)
        pan = PAN_CENTRE - a;
    else if (a >= PAN_BACK_FROM && a <= PAN_BACK_TO)
        pan = PAN_BACK - a;
    else
        pan = a - PAN_CENTRE;
    if (pan < 0)
        pan = 0;
    if (pan > PAN_MAX)
        pan = PAN_MAX;
    return pan;
}

int32_t race_sound_loudness(int32_t distance)
{
    if (distance >= HEARD_WITHIN)
        return 0;
    int32_t left = (int32_t)((uint32_t)HEARD_WITHIN - (uint32_t)distance) >> LOUDNESS_SHIFT;
    return left * LOUDNESS_MAX / LOUDNESS_DIVIDE;
}

static int32_t at_most(int32_t v, int32_t most)
{
    return v > most ? most : v;
}

static int32_t voice_start(RaceState *race, int32_t sound, int32_t pan, int32_t volume, int32_t pitch)
{
    if (race->sounds.voice_start == NULL)
        return -1;
    return race->sounds.voice_start(race->sounds.user, sound, pan, (uint8_t)volume, (uint8_t)pitch);
}

static int voice_stopped(RaceState *race, int32_t voice)
{
    return race->sounds.voice_stopped == NULL || race->sounds.voice_stopped(race->sounds.user, voice) != 0;
}

static void voice_set(RaceState *race, int32_t voice, int32_t volume, int32_t pitch, int32_t pan)
{
    if (race->sounds.voice_volume != NULL)
        race->sounds.voice_volume(race->sounds.user, voice, volume);
    if (race->sounds.voice_pitch != NULL)
        race->sounds.voice_pitch(race->sounds.user, voice, pitch);
    if (race->sounds.voice_pan != NULL)
        race->sounds.voice_pan(race->sounds.user, voice, pan);
}

void race_sound_channel(RaceState *race, int32_t channel, int32_t sound, int32_t pitch, int32_t volume, int32_t pan)
{
    if ((uint32_t)race->player >= RACE_PLAYERS || (uint32_t)channel >= RACE_VOICES)
        return;
    int32_t level = race->entries[race->player].sound_volume;
    if (level == 0)
        return;
    if (race->sound_silent != 0)
        volume = 0;
    pitch = at_most(pitch, LEVEL_MAX);
    volume = at_most(volume, LEVEL_MAX);
    pan = at_most(pan, LEVEL_MAX);
    int32_t loud = race->sound_silent != 0 ? 0 : (int32_t)((uint32_t)level * (uint32_t)volume) / LEVEL_MAX;
    RaceVoice *v = &race->voices[channel];
    if ((sound < FRESH_FROM || sound > FRESH_TO) && sound != FRESH_TOO) {
        if (sound == v->sound) {
            voice_set(race, v->voice, loud, pitch, pan);
            return;
        }
        if (v->sound <= FRESH_TO && !voice_stopped(race, v->voice))
            return;
    }
    int32_t voice = voice_start(race, sound, pan, loud, pitch);
    if (voice >= 0) {
        v->voice = voice;
        v->sound = sound;
    }
}
