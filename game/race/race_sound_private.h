/* The race sound files' shared names (race_sound.c, race_sound_engine.c, race_sound_tyres.c, race_sound_others.c, race_sound_music.c): the levels' constants,
 * the engine curve and the voice calls, split from race_sound.c with no change in what they do. */
#ifndef TNFS_RACE_SOUND_PRIVATE_H
#define TNFS_RACE_SOUND_PRIVATE_H

#include "game/race/race_sound.h"
#include "game/gfx/x86_int.h"
#include "game/sys/fixed_math.h"

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
    FRESH_TOO = 0x50,
    PULSE_SOUND = 0x61,
    PULSE_PAN = 0x40,
    PULSE_PITCH = 0x40,
    PULSE_STEPS = 7,
    NODE_CHANNEL = 0xb,           /* 004444b8's node sound */
    NODE_SOUND = 0x3e,
    NODE_SOUND_TRACK7 = 0x3d,
    NODE_STEP = 5,
    NODE_TOP = 0x7a,
    GRAVEL_CHANNEL = 0xf,         /* track 5 off the road (+0x4aa) */
    GRAVEL_SOUND = 0x50,
    GRAVEL_LOW_PITCH = 0x30,
    RUMBLE_CHANNEL = 4,           /* track 5 on a +0x4c6 node */
    RUMBLE_SOUND = 0x21,
    ENGINE_SOUND = 1,
    ROAD_SOUND = 0x29,
    ENGINE2_SOUND = 2,            /* 00443820's voices[1] */
    START_PAN = 0x40, START_VOLUME = 0, START_PITCH = 0x40,  /* 00444394's voices */
    ROAD_VOICE = 5,               /* 0050a4f4 */
    ENGINE_STOP_4 = 4, ENGINE_STOP_3 = 3,  /* 0044435c's other two channels */
    ENGINE_VOICE = 1,             /* 0050a4d4 */
    EXTRA_VOICE = 14,             /* 0050a53c, -1 when unused */
    CURVE_TOP = 0x80
};

/* 004bc76c: the engine's two voices crossfaded by the eased throttle. */
static const int32_t ENGINE_CURVE[CURVE_TOP + 1] = {
    0, 3, 7, 10, 13, 16, 19, 22, 24, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 46, 48, 50, 51, 53, 54, 55, 57, 58,
    60, 61, 62, 63, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 84, 85, 86,
    87, 88, 88, 89, 90, 91, 91, 92, 93, 94, 94, 95, 96, 96, 97, 98, 98, 99, 100, 100, 101, 101, 102, 103, 103,
    104, 104, 105, 106, 106, 107, 107, 108, 108, 109, 109, 110, 110, 111, 111, 112, 112, 113, 113, 114, 114,
    115, 115, 116, 116, 117, 117, 118, 118, 119, 119, 119, 120, 120, 121, 121, 122, 122, 122, 123, 123, 124,
    124, 125, 125, 125, 126, 127, 127
};

static inline int32_t at_most(int32_t v, int32_t most)
{
    return v > most ? most : v;
}

static inline int32_t voice_start(RaceState *race, int32_t sound, int32_t pan, int32_t volume, int32_t pitch)
{
    if (race->sounds.voice_start == NULL)
        return -1;
    return race->sounds.voice_start(race->sounds.user, sound, pan, (uint8_t)volume, (uint8_t)pitch);
}

static inline int voice_stopped(RaceState *race, int32_t voice)
{
    return race->sounds.voice_stopped == NULL || race->sounds.voice_stopped(race->sounds.user, voice) != 0;
}

static inline void voice_volume(RaceState *race, int32_t voice, int32_t volume)
{
    if (race->sounds.voice_volume != NULL)
        race->sounds.voice_volume(race->sounds.user, voice, volume);
}

static inline void voice_pitch(RaceState *race, int32_t voice, int32_t pitch)
{
    if (race->sounds.voice_pitch != NULL)
        race->sounds.voice_pitch(race->sounds.user, voice, pitch);
}

static inline void voice_pan(RaceState *race, int32_t voice, int32_t pan)
{
    if (race->sounds.voice_pan != NULL)
        race->sounds.voice_pan(race->sounds.user, voice, pan);
}

static inline void voice_set(RaceState *race, int32_t voice, int32_t volume, int32_t pitch, int32_t pan)
{
    if (race->sounds.voice_volume != NULL)
        race->sounds.voice_volume(race->sounds.user, voice, volume);
    if (race->sounds.voice_pitch != NULL)
        race->sounds.voice_pitch(race->sounds.user, voice, pitch);
    if (race->sounds.voice_pan != NULL)
        race->sounds.voice_pan(race->sounds.user, voice, pan);
}

/* A level scaled by the followed slot's sound volume, 0 when silent. */
static inline int32_t scaled(const RaceState *race, int32_t level, int32_t volume)
{
    return race->sound_silent != 0 ? 0 : (int32_t)((uint32_t)level * (uint32_t)volume) / LEVEL_MAX;
}

static inline int32_t magnitude(int32_t v)
{
    return v > 0 ? v : x_sub(0, v);
}

/* the larger of two magnitudes plus a quarter of the smaller (ties quarter the second) */
static inline int32_t estimate(int32_t first, int32_t second)
{
    return second > first ? x_add(second, first >> 2) : x_add(second >> 2, first);
}

/* entries[004fbb08], the followed slot's (port guard: slot 0 past them) */
static inline const RaceEntry *player_entry(const RaceState *race)
{
    return &race->entries[(uint32_t)race->player < RACE_PLAYERS ? race->player : 0];
}

#endif
