/* The race's sound levels (race_sound.h). */
#include "game/race/race_sound.h"

#include "game/gfx/x86_int.h"

#include "game/race/car_walls.h"

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
    RUMBLE_CHANNEL = 4,           /* track 5 on a +0x4c6 node */
    RUMBLE_SOUND = 0x21,
    ENGINE_SOUND = 1,
    ROAD_SOUND = 0x29,
    ENGINE2_SOUND = 2,            /* 00443820's voices[1] */
    START_PAN = 0x40, START_VOLUME = 0, START_PITCH = 0x40,  /* 00444394's voices */
    ROAD_VOICE = 5,               /* 0050a4f4 */
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

/* 004446ed: the rumble's volume by the road noise's level / 10 from 2 */
static const int32_t RUMBLE_LEVELS[7] = { 0x19, 0x1e, 0x1e, 0x28, 0x32, 0x41, 0x4b };

static const int32_t PULSE_COUNTS[PULSE_STEPS] = { 1, 20, 15, 10, 5, 3, 1 };  /* 00443710 */

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

static void voice_volume(RaceState *race, int32_t voice, int32_t volume)
{
    if (race->sounds.voice_volume != NULL)
        race->sounds.voice_volume(race->sounds.user, voice, volume);
}

static void voice_pitch(RaceState *race, int32_t voice, int32_t pitch)
{
    if (race->sounds.voice_pitch != NULL)
        race->sounds.voice_pitch(race->sounds.user, voice, pitch);
}

static void voice_pan(RaceState *race, int32_t voice, int32_t pan)
{
    if (race->sounds.voice_pan != NULL)
        race->sounds.voice_pan(race->sounds.user, voice, pan);
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

void race_sound_pulse(RaceState *race)
{
    if ((uint32_t)race->unknown_4b92d4 >= PULSE_STEPS || (uint32_t)race->player >= RACE_PLAYERS)
        return;
    RaceEngineSound *p = &race->engine_sound;
    const int32_t count = PULSE_COUNTS[race->unknown_4b92d4];
    if (p->pulse_armed == 0) {
        p->pulse_armed = 1;
        p->pulse_count = count;
    } else if (count < p->pulse_count) {
        p->pulse_count = count;
    }
    if (p->pulse_count != 0) {
        p->pulse_count = (int32_t)((uint32_t)p->pulse_count - 1u);
        return;
    }
    const int32_t volume = race->sound_silent == 0 ? (uint8_t)race->entries[race->player].sound_volume : 0;
    voice_start(race, PULSE_SOUND, PULSE_PAN, volume, PULSE_PITCH);
    p->pulse_armed = 0;
}

/* A level scaled by the followed slot's sound volume, 0 when silent. */
static int32_t scaled(const RaceState *race, int32_t level, int32_t volume)
{
    return race->sound_silent != 0 ? 0 : (int32_t)((uint32_t)level * (uint32_t)volume) / LEVEL_MAX;
}

/* The node sound on channel 0xb: up by 5 to 0x7e while the car is on a
 * +0x4ca node (pan 0) or a +0x4ce one (pan 0x7f), else down by 5. */
static void node_sound(RaceState *race, const Car *car)
{
    RaceEngineSound *e = &race->engine_sound;
    if (car->node_flag_4ca != 0 || car->node_flag_4ce != 0) {
        e->node_pan = car->node_flag_4ca != 0 ? 0 : LEVEL_MAX;
        if (e->node_volume < NODE_TOP)
            e->node_volume += NODE_STEP;
        if (car->node_flag_4ca != 0 && race->track == 7) {
            race_sound_channel(race, NODE_CHANNEL, NODE_SOUND_TRACK7, 0, e->node_volume, e->node_pan);
            return;
        }
    } else if (e->node_volume >= NODE_STEP) {
        e->node_volume -= NODE_STEP;
    } else {
        return;
    }
    race_sound_channel(race, NODE_CHANNEL, NODE_SOUND, PAN_CENTRE, e->node_volume, e->node_pan);
}

/* The road noise's level (0..0x7f) from the car's speed, and its pitch;
 * gravel and rumble on track 5. */
static int32_t road_noise(RaceState *race, const Car *car)
{
    RaceEngineSound *e = &race->engine_sound;
    int32_t level = car->speed >> 16;
    level = (int32_t)((level > 0 ? (uint32_t)level : 0u - (uint32_t)level) * 2u);
    if (car->node_flag_4aa != 0) {
        level = (int32_t)((uint32_t)level + 0x14u);
        e->pitch = 0x5e;
        if (race->track == 5) {
            const int32_t volume = (int16_t)(((race_rand(race) & 0xffffu) * LEVEL_MAX) >> 16);
            const int32_t pan = (int16_t)(((race_rand(race) & 0xffffu) * LEVEL_MAX) >> 16);
            if ((int16_t)(((race_rand(race) & 0xffffu) * 0x1eu) >> 16) == 0)
                race_sound_channel(race, GRAVEL_CHANNEL, GRAVEL_SOUND, PAN_CENTRE, volume, pan);
        }
    } else if (car->node_flag_4c6 != 0) {
        if (race->track == 5) {
            const uint32_t step = (uint32_t)(level / 10 - 2);
            race_sound_channel(race, RUMBLE_CHANNEL, RUMBLE_SOUND, PAN_CENTRE, step <= 6 ? RUMBLE_LEVELS[step] : level,
                               PAN_CENTRE);
        } else {
            const uint32_t swing = (uint32_t)(car->speed >> 17);
            e->pitch = (int32_t)(e->rising != 0 ? (uint32_t)e->pitch + swing : (uint32_t)e->pitch - swing);
            if (e->pitch <= 0x40)
                e->rising = 1;
            else if (e->pitch > 0x5e)
                e->rising = 0;
        }
    } else if (car->surface != 0) {
        level += level / 4;
        e->pitch = 0x18;
    } else {
        e->pitch = 0x40;
    }
    return at_most(level, LEVEL_MAX);
}

/* The engine's second voice (channel 004bcb70) when 004bc6f0 is set,
 * started again on sound 1 once it stopped. */
static void second_voice(RaceState *race, int32_t level, int32_t volume, int32_t pan)
{
    const int32_t channel = (int16_t)race->engine_sound.channel;
    if (race->unknown_4bc6f0 == 0 || (uint32_t)channel >= RACE_VOICES)
        return;
    RaceVoice *v = &race->voices[channel];
    voice_volume(race, v->voice, scaled(race, level, volume));
    voice_pitch(race, v->voice, race->engine_sound.revs);
    voice_pan(race, v->voice, pan);
    if (!voice_stopped(race, v->voice))
        return;
    const int32_t voice = voice_start(race, ENGINE_SOUND, pan, scaled(race, level, volume), race->engine_sound.revs);
    if (voice >= 0) {
        v->voice = voice;
        v->sound = ENGINE_SOUND;
    }
}

void race_sound_engine(RaceState *race, const Car *car, int32_t angle, int32_t distance, const CarSpecs *specs)
{
    if ((uint32_t)race->player >= RACE_PLAYERS)
        return;
    const RaceEntry *entry = &race->entries[race->player];
    const int32_t volume = entry->sound_volume;
    if (volume == 0)
        return;
    if (race->unknown_4b92d4 != 0)
        race_sound_pulse(race);
    RaceEngineSound *e = &race->engine_sound;
    node_sound(race, car);
    int32_t level = road_noise(race, car);
    if (distance > 2 && car->node_flag_4c6 == 0 && car->node_flag_4aa == 0)
        level = level * 6 / 10;
    voice_volume(race, race->voices[ROAD_VOICE].voice, scaled(race, level, volume));
    voice_pitch(race, race->voices[ROAD_VOICE].voice, e->pitch);
    const Car *viewed = (uint32_t)race->viewed < RACE_CARS ? race->cars[race->viewed] : NULL;
    const int32_t divisor = (int32_t)((uint32_t)specs->unknown_0ac + 2000u);
    const int32_t revs = viewed != NULL ? (int32_t)((uint32_t)viewed->rpm * LEVEL_MAX) : 0;
    if (viewed == NULL || divisor == 0 || (divisor == -1 && revs == INT32_MIN))
        return;
    e->revs = at_most(revs / divisor, LEVEL_MAX);
    const int32_t gap = (int32_t)((uint32_t)car->throttle - (uint32_t)e->throttle);
    e->throttle = (int32_t)((uint32_t)e->throttle + (uint32_t)(e->throttle <= car->throttle ? gap / 2 : gap / 8));
    const int32_t at = e->throttle >> 1;
    if (at < 0 || at > CURVE_TOP)
        return;
    const int32_t loud = at_most((int32_t)((uint32_t)(e->revs + 0x14) * (uint32_t)(race_sound_loudness(distance) + 1)) / 128,
                                 LEVEL_MAX);
    const int32_t low = (int32_t)((uint32_t)ENGINE_CURVE[at] * (uint32_t)loud) >> 7;
    const int32_t high = (int32_t)((uint32_t)ENGINE_CURVE[CURVE_TOP - at] * (uint32_t)loud) >> 7;
    const int32_t pan = race_sound_pan(entry->sound_mode == 2 ? 0 : angle);
    second_voice(race, low, volume, pan);
    voice_volume(race, race->voices[ENGINE_VOICE].voice, scaled(race, high, volume));
    voice_pitch(race, race->voices[ENGINE_VOICE].voice, e->revs);
    voice_pan(race, race->voices[ENGINE_VOICE].voice, pan);
    if (race->voices[EXTRA_VOICE].voice == -1)
        return;
    voice_volume(race, race->voices[EXTRA_VOICE].voice, scaled(race, loud, volume));
    voice_pan(race, race->voices[EXTRA_VOICE].voice, pan);
}

void race_sound_start_voices(RaceState *race)
{
    if (race->unknown_52b31e != 0 && race->unknown_52b30e == 0) return;
    const int32_t channel = (int16_t)race->engine_sound.channel;
    if ((uint32_t)channel >= RACE_VOICES) return;  /* port guard: the original reads 0050a4cc + 8 x channel unchecked */
    if (race->voices[channel].voice >= 0) return;
    race->voices[channel].voice = voice_start(race, ENGINE_SOUND, START_PAN, START_VOLUME, START_PITCH);
    race->voices[channel].sound = ENGINE_SOUND;
    race->voices[ENGINE_VOICE].voice = voice_start(race, ENGINE2_SOUND, START_PAN, START_VOLUME, START_PITCH);
    race->voices[ENGINE_VOICE].sound = ENGINE2_SOUND;
    race->voices[ROAD_VOICE].voice = voice_start(race, ROAD_SOUND, START_PAN, START_VOLUME, START_PITCH);
    race->voices[ROAD_VOICE].sound = ROAD_SOUND;
}

void race_sound_follow(RaceState *race, const CarSpecs *specs)
{
    if ((uint32_t)race->viewed >= RACE_CARS || race->cars[race->viewed] == NULL)
        return;
    const Car *car = race->cars[race->viewed];
    int32_t angle = 0, distance = 1;
    if (race->chase.kind > 1) {
        const int32_t at[3] = { car->x, car->y, car->z };
        race_sound_position(race, at, &angle, &distance);
    }
    race_sound_engine(race, car, angle, distance, specs);
}

void race_sound_start_engine(RaceState *race)
{
    const int32_t channel = (int16_t)race->engine_sound.channel;
    /* port guard: the original writes 0050a4cc + 8 x channel unchecked */
    if ((uint32_t)channel < RACE_VOICES && race->voices[channel].sound != ENGINE_SOUND) {
        race->voices[channel].voice = voice_start(race, ENGINE_SOUND, START_PAN, START_VOLUME, START_PITCH);
        race->voices[channel].sound = ENGINE_SOUND;
    }
    race->voices[ROAD_VOICE].voice = voice_start(race, ROAD_SOUND, START_PAN, START_VOLUME, START_PITCH);
    race->unknown_4bc6f0 = 1;
    race->voices[ROAD_VOICE].sound = ROAD_SOUND;
    if (race->effects != 0 && race->sounds.force_release != NULL) race->sounds.force_release(race->sounds.user);
}

enum { OTHER_HUMAN_TICKS = 999, OTHER_HUMAN_KIND = 1 };

void race_sound_other_human(RaceState *race)
{
    const int32_t viewed = race->viewed;
    for (int16_t i = 0; i < race->humans; i = (int16_t)(i + 1)) {
        if (i == viewed) continue;
        const Car *car = race->cars[race_slot(i)];  /* port guard */
        if (car == NULL || car->look_back == 0) continue;
        if (race->players[(uint16_t)i <= RACE_PLAYERS ? i : 0].finish != 0) continue;  /* port guard */
        race->car_sound_slot = i;
        race->car_sound_ticks = OTHER_HUMAN_TICKS;
        race->car_sound_kind = OTHER_HUMAN_KIND;
        race->viewed = viewed;
        return;
    }
    race->car_sound_ticks = 0;
    race->viewed = viewed;
}

enum { DOPPLER_NEAR = 0x96000, DOPPLER_SPEED = 0x1bcccc, DOPPLER_SCALE = 0x1bcccc00 };

static int32_t magnitude(int32_t v)
{
    return v > 0 ? v : x_sub(0, v);
}

int32_t race_sound_doppler(Car *car, const RaceChase *chase)
{
    const int32_t dz = magnitude(x_sub(car->z, chase->position[2]));
    const int32_t dx = magnitude(x_sub(car->x, chase->position[0]));
    const int32_t distance = dx > dz ? x_add(dz >> 2, dx) : x_add(dz, dx >> 2);
    const int32_t moved = x_sub(distance, car->unknown_3a1);
    car->unknown_3a1 = distance;
    int32_t factor = 0;
    if (car->unknown_3ad == chase->kind && magnitude(moved) < DOPPLER_NEAR)
        factor = DOPPLER_SCALE / x_add(moved, DOPPLER_SPEED);
    car->unknown_3ad = chase->kind;
    return factor;
}

void race_sound_channel_at(RaceState *race, int32_t channel, int32_t sound, int32_t pitch, int32_t volume, int32_t angle)
{
    const int32_t pan = race_sound_pan(angle);
    if (volume > LEVEL_MAX) volume = LEVEL_MAX;
    race_sound_channel(race, channel, sound, pitch, ENGINE_CURVE[volume < 0 ? 0 : volume], pan);  /* port guard */
}

void race_sound_channel_stop(RaceState *race, int32_t channel)
{
    if ((uint32_t)channel >= RACE_VOICES) return;  /* port guard: the original indexes 0050a4cc unchecked */
    RaceVoice *v = &race->voices[channel];
    if (v->voice >= 0 && race->sounds.voice_stop != NULL) race->sounds.voice_stop(race->sounds.user, v->voice);
    v->voice = -1;
    v->sound = -1;
}

enum {
    CAR_SOUND_CHANNEL = 4, CAR_SOUND_FIRST = 0x3f, CAR_SOUND_LAST = 0x41, CAR_SOUND_ROWS = 10,
    CAR_SOUND_RANGE = 0x200000, CAR_SOUND_DOWN = 8, CAR_SOUND_LOUD = 0x7f, CAR_SOUND_SPAN = 0x2000, PITCH_UNIT = 0x100
};

/* 00471f40 (by slot) and 00471f68 (by kind) */
static const int32_t CAR_SOUND_BY_SLOT[CAR_SOUND_ROWS] = { 0x41, 0x41, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f };
static const int32_t CAR_SOUND_PITCH[CAR_SOUND_ROWS] = { 64, 64, 100, 90, 80, 70, 60, 50, 45, 40 };

static int32_t car_sound_row(int32_t i)
{
    return (uint32_t)i < CAR_SOUND_ROWS ? i : 0;  /* port guard: the original reads its stack past the copied table */
}

void race_sound_held_car(RaceState *race)
{
    if (race->car_sound_ticks == 0) {
        const RaceVoice *v = &race->voices[CAR_SOUND_CHANNEL];
        if (v->voice >= 0 && v->sound >= CAR_SOUND_FIRST && v->sound <= CAR_SOUND_LAST)
            race_sound_channel_stop(race, CAR_SOUND_CHANNEL);
        return;
    }
    Car *car = race->cars[race_slot(race->car_sound_slot)];
    if (car == NULL) return;  /* port guard */
    const int32_t at[3] = { car->x, car->y, car->z };
    int32_t angle, distance;
    race_sound_position(race, at, &angle, &distance);
    const int32_t factor = race_sound_doppler(car, &race->chase);
    race->car_sound_ticks = x_sub(race->car_sound_ticks, 1);
    if (factor == 0) return;
    const int32_t sound = CAR_SOUND_BY_SLOT[car_sound_row(race->car_sound_slot)];
    const int32_t pitch = x_mul(factor, CAR_SOUND_PITCH[car_sound_row(race->car_sound_kind)]) / PITCH_UNIT;
    const int32_t volume = distance < CAR_SOUND_RANGE
        ? x_mul(x_sar(x_sub(CAR_SOUND_RANGE, distance), CAR_SOUND_DOWN), CAR_SOUND_LOUD) / CAR_SOUND_SPAN : 0;
    race_sound_channel_at(race, CAR_SOUND_CHANNEL, sound, pitch, volume, angle);
}

void race_sound_effect(RaceState *race, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    if (race->sounds.sound != NULL) race->sounds.sound(race->sounds.user, a, b & 0xff, c & 0xff, d, e, f);
}

void race_sound_effect_scaled(RaceState *race, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    if (race->sounds.sound != NULL) race->sounds.sound(race->sounds.user, a, b & 0xff, c & 0xff, x_sar(d, 4), e, f);
}

enum { SIGNAL_CHANNEL = 14, SIGNAL_SOUND = 3, SIGNAL_PITCH = 0x40, SIGNAL_CENTRE = 0x40, SIGNAL_NEAR = 2 };

void race_sound_signal(RaceState *race, int32_t distance, int32_t angle)
{
    const int32_t volume = race_sound_loudness(distance);
    const int32_t pan = distance < SIGNAL_NEAR ? SIGNAL_CENTRE : race_sound_pan(angle);
    race_sound_channel(race, SIGNAL_CHANNEL, SIGNAL_SOUND, SIGNAL_PITCH, volume, pan);
}

void race_sound_signal_off(RaceState *race)
{
    race_sound_channel_stop(race, SIGNAL_CHANNEL);
}

enum {
    TYRE_FRONT_CHANNEL = 4, TYRE_REAR_CHANNEL = 12, TYRE_SOUND = 4, TYRE_ON_ROAD = 1, TYRE_OFF_ROAD = 8,
    TYRE_FADE_FIRST = 0x26, TYRE_FADE_LAST = 0x2e, TYRE_LATERAL_SCALE = 6, TYRE_TURN = 0x1000000, TYRE_REAR_ANGLE = 0x800000,
    IN_CAR_KINDS = 1
};

static int tyre_heard(const RaceState *race, int32_t channel, int32_t skid)
{
    const int32_t sound = race->voices[channel].sound;
    return skid > 0 || (sound <= TYRE_FADE_LAST && sound >= TYRE_FADE_FIRST);
}

void race_sound_tyres(RaceState *race)
{
    Car *car = race->cars[race_slot(race->viewed)];  /* 0052b316 / 004fbb08 / 00513e6c pick the same car */
    if (car == NULL) return;  /* port guard */
    int32_t distance = -1, angle, rear_distance, rear_angle;
    if (race->chase.kind <= IN_CAR_KINDS) {
        angle = x_mul(x_sub(0, car->lateral_force), TYRE_LATERAL_SCALE);
        if (angle < 0) angle = x_add(angle, TYRE_TURN);
        rear_angle = TYRE_REAR_ANGLE;
        distance = rear_distance = 0;
    } else {
        const int32_t at[3] = { car->x, car->y, car->z };
        race_sound_position(race, at, &angle, &distance);
        rear_distance = distance, rear_angle = angle;
    }
    int32_t sound, byte;
    if (race->unknown_4c5f98 >= 0) {
        sound = race->unknown_4c5f98, byte = race->unknown_4c5f9c;
    } else {
        sound = TYRE_SOUND;
        byte = race->surfaces[(uint32_t)car->surface < RACE_SURFACES ? car->surface : 0].off_road == 0  /* port guard */
            ? TYRE_ON_ROAD : TYRE_OFF_ROAD;
    }
    if (race->unknown_4bc6f0 != 0) {
        if (tyre_heard(race, TYRE_FRONT_CHANNEL, car->skid_front))
            race_sound_effect(race, TYRE_FRONT_CHANNEL, sound, byte, car->skid_front, x_add(distance, 1), angle);
        if (tyre_heard(race, TYRE_REAR_CHANNEL, car->skid_rear))
            race_sound_effect(race, TYRE_REAR_CHANNEL, sound, byte, car->skid_rear, x_add(rear_distance, 1), rear_angle);
    } else {
        race->car_sound_ticks = 0;
    }
    if (car->look_back == 0 || race->viewed < 0 || race->viewed >= race->humans) {
        race_sound_signal_off(race);
        return;
    }
    if (distance < 0) {
        const int32_t at[3] = { car->x, car->y, car->z };
        race_sound_position(race, at, &angle, &distance);
    }
    race_sound_signal(race, distance, angle);
}
