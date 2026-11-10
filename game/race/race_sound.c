/* The race's sound levels: the pan and loudness, the channels, the effects and the pick, the held sounds, the signal, the end and the play (race_sound.h). */
#include "game/race/race_sound_private.h"
#include "game/race/car_walls.h"

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
    if (race->port_stop_replaced != 0 && v->voice >= 0 && v->sound >= 0 && (v->sound < FRESH_FROM || v->sound > FRESH_TO) &&
        v->sound != FRESH_TOO && race->sounds.voice_stop != NULL)
        race->sounds.voice_stop(race->sounds.user, v->voice);   /* port: the record is all that cuts a looping voice */
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

/* 00443db8: the rows are tested in this order; b and c are compared as bytes. */
enum { PICK_NONE = 0, PICK_20 = 0x20, PICK_21 = 0x21, PICK_22 = 0x22, PICK_23 = 0x23, PICK_24 = 0x24, PICK_25 = 0x25 };

int32_t race_sound_pick_any(int32_t speed, int32_t b, int32_t c)
{
    const int32_t s = x_sar(speed, 16);
    const uint8_t kb = (uint8_t)b, kc = (uint8_t)c;
    if (kb == 0xe || kb == 0xc) return PICK_NONE;
    if (kb == 0xd) return PICK_20;
    if (kb == 4 && kc == 1) return PICK_25;
    if (kb == 2 && kc == 9) return PICK_23;
    if (kb == 2 && kc == 0xa) return PICK_21;
    if (kb == 2 && kc == 1) {
        if (s > 0x50) return PICK_24;
        if (s > 0x16) return PICK_22;
        if (s > 0x10) return PICK_23;
        return PICK_21;
    }
    if (s > 0x6e) return PICK_24;
    if (s > 0x3c) return PICK_22;
    return PICK_21;
}

/* 00443c90: every row is tested and the last that matches wins (the engine's
 * channel may be 2, 3, 4 or 12 too). */
typedef struct { int32_t channel, b, c, sound; } SoundPick;

enum { PICK_ENGINE = -1 };

static const SoundPick SOUND_PICKS[] = {
    { PICK_ENGINE, 4, 1, 0x2d }, { PICK_ENGINE, 2, 1, 0x27 }, { PICK_ENGINE, 4, 8, 0x2e },
    { 2, 4, 1, 0x38 }, { 2, 4, 8, 0x38 }, { 2, 2, 1, 0x34 },
    { 3, 4, 1, 0x3a }, { 3, 4, 8, 0x3a }, { 3, 2, 1, 0x36 },
    { 4, 4, 1, 0x2a }, { 4, 4, 8, 0x26 },
    { 12, 4, 1, 0x2b }, { 12, 4, 8, 0x28 },
};

int32_t race_sound_pick(const RaceState *race, int32_t channel, int32_t b, int32_t c)
{
    const int32_t engine = (int16_t)race->engine_sound.channel;
    int32_t sound = PICK_NONE;
    for (size_t i = 0; i < sizeof SOUND_PICKS / sizeof SOUND_PICKS[0]; i++) {
        const SoundPick *p = &SOUND_PICKS[i];
        const int32_t want = p->channel == PICK_ENGINE ? engine : p->channel;
        if (channel == want && (uint8_t)b == p->b && (uint8_t)c == p->c) sound = p->sound;
    }
    return sound;
}

enum { HELD_PITCH_FREE = 4 };

int32_t race_sound_held_pitch(RaceState *race, int32_t channel, int32_t sound, int32_t speed)
{
    const int32_t pitch = x_sar(speed, 16);
    if (channel == HELD_PITCH_FREE) return pitch;
    if ((uint32_t)channel >= RACE_VOICES) return pitch;  /* port guard: the original indexes unchecked */
    if (sound != race->voices[channel].sound) {
        race->sound_pitch[channel] = pitch;
        return pitch;
    }
    return race->sound_pitch[channel];
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

void race_sound_channels_stop(RaceState *race)
{
    for (int32_t k = 0; k < RACE_VOICES; k++) race_sound_channel_stop(race, k);
}

void race_sound_end(RaceState *race, RaceSoundBanks *banks, const RaceSoundBankOps *ops)
{
    race_sound_channels_stop(race);
    for (int32_t i = banks->count - 1; i >= 0; i--) {
        if (i >= RACE_SOUND_BANKS) continue;  /* port guard: the original reads past the six */
        ops->unregister(ops->ctx, banks->registered[i]);
        ops->free(ops->ctx, banks->file[i]);
    }
    banks->count = 0;
}

enum { PLAY_PHASE_QUIET = 3, PLAY_MODE_MONO = 2, PLAY_ANY_CHANNEL = 4, PLAY_ANY_PITCH = 0x40, PLAY_LOUD_SHIFT = 7 };

void race_sound_play(RaceState *race, int32_t channel, int32_t b, int32_t c, int32_t speed, int32_t distance, int32_t angle)
{
    if (race->phase == PLAY_PHASE_QUIET || player_entry(race)->sound_volume == 0) return;
    if (player_entry(race)->sound_mode == PLAY_MODE_MONO) angle = 0;
    const int32_t pan = race_sound_pan(angle);
    const int32_t loud = race_sound_loudness(distance);
    b &= 0xff, c &= 0xff;
    if (channel < 0) {
        race_sound_channel(race, PLAY_ANY_CHANNEL, race_sound_pick_any(speed, b, c), PLAY_ANY_PITCH, loud, pan);
        return;
    }
    if (speed <= 0) {
        if ((uint32_t)channel < RACE_VOICES && race->voices[channel].voice >= 0)  /* port guard: unchecked in the original */
            race_sound_channel_stop(race, channel);
        return;
    }
    const int32_t sound = race_sound_pick(race, channel, b, c);
    const int32_t pitch = race_sound_held_pitch(race, channel, sound, speed);
    const int32_t volume = channel == PLAY_ANY_CHANNEL ? x_sar(x_mul(speed, loud), PLAY_LOUD_SHIFT) : loud;
    race_sound_channel(race, channel, sound, pitch, volume, pan);
}
