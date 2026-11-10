/* The race's engine sound: the node and road noise, the second voice, the engine and the banks and the voices at the start (race_sound.h). */
#include "game/race/race_sound_private.h"
#include "game/race/rally_gravel.h"
#include "game/race/car_walls.h"

/* 004446ed: the rumble's volume by the road noise's level / 10 from 2 */
static const int32_t RUMBLE_LEVELS[7] = { 0x19, 0x1e, 0x1e, 0x28, 0x32, 0x41, 0x4b };

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

/* port: the level of the road noise on the rally's rolling gravel (the car is on the road, on the ground), and its crunch strikes; sets the pitch */
static int32_t gravel_roll_noise(RaceState *race, const Car *car, int32_t level)
{
    RaceEngineSound *e = &race->engine_sound;
    /* port (rally_gravel.h): the rally rolls on gravel: the road noise as the shoulder's own (the off-road branch below: its level plus
     * 0x14 and the pitch 0x5e), only lower. */
    level = rally_gravel_level(race->port_rally_gravel, (int32_t)((uint32_t)level + 0x14u));
    level = rally_gravel_road_level(race->port_rally_gravel, level, car->speed);   /* and heard at a crawl */
    e->pitch = 0x5e;
    /* and the exe's own gravel sample struck on the rolling car (rally_gravel_crunch): the road voice alone is sample 0x29, the same low murmur the asphalt has */
    if (race->port_rally_shoulder_sound != 0 || race->port_rally_gravel == RALLY_GRAVEL_SNOW) {
        /* port (rally_gravel_shoulder): the shoulder's own tyre sound carries the roll (race_sound_tyres), so no strike of the 0x50 sample here; the snow's crunch is its own voice (snow_roll.h) */
        return level;
    }
    const int32_t crunch = rally_gravel_crunch(race->port_rally_gravel, car->speed, race_rand(race) & 0xffffu);
    if (crunch > 0) {
        const int32_t pan = (int16_t)(((race_rand(race) & 0xffffu) * LEVEL_MAX) >> 16);
        race_sound_channel(race, GRAVEL_CHANNEL, GRAVEL_SOUND, PAN_CENTRE, crunch, pan);
        /* the sample is the bank's quietest (0.09 of full scale against the engine's 0.3): a second strike, lower and from the other side, doubles it and thickens it */
        race_sound_channel(race, GRAVEL_CHANNEL, GRAVEL_SOUND, GRAVEL_LOW_PITCH, crunch, LEVEL_MAX - pan);
    }
    return level;
}

/* The road noise's level (0..0x7f) from the car's speed, and its pitch;
 * gravel and rumble on track 5. */
static int32_t road_noise(RaceState *race, const Car *car)
{
    RaceEngineSound *e = &race->engine_sound;
    int32_t level = car->speed >> 16;
    level = (int32_t)((level > 0 ? (uint32_t)level : 0u - (uint32_t)level) * 2u);
    if (race->port_rally_gravel != 0 && car->node_flag_4aa == 0 && car->node_flag_4c6 == 0 && car->airborne_ticks == 0) {
        level = gravel_roll_noise(race, car, level);
    } else if (car->node_flag_4aa != 0) {
        level = (int32_t)((uint32_t)level + 0x14u);
        level = rally_gravel_shoulder_level(race->port_rally_gravel, level, car->speed);   /* port: heard at a crawl in the rally */
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

/* 00443670: each car's own bank, 004436a0: its opponent bank (2 per car) */
static const char *const player_banks[12] = { "Supra_sw.bnk", "Diablosw.bnk", "911_sw.bnk", "ZR1_sw.bnk", "tr512_sw.bnk", "Vipersw.bnk",
                                              "NSX_sw.bnk", "RX7_sw.bnk", "RX7_sw.bnk", "RX7_sw.bnk", "911_sw.bnk", "traffc.bnk" };
static const char *const opponent_banks[12] = { "OSupmb.bnk", "ODiamb.bnk", "O911mb.bnk", "OZR1mb.bnk", "O512mb.bnk", "OVipmb.bnk",
                                                "ONSXmb.bnk", "ORX7mb.bnk", "O911mb.bnk", "O911mb.bnk", "O911mb.bnk", "traffcO.bnk" };

/* 00443924 */
void race_sound_load_banks(RaceState *race, void (*load)(void *user, const char *name), void *user)
{
    if (race->entries[race_slot(race->player)].sound_on == 0) return;
    const uint32_t car = (uint32_t)race->entries[race_slot(race->player)].car;
    if (car < 12) load(user, player_banks[car]);
    const uint32_t other = (uint32_t)race->entries[race->player == 0 ? 1 : 0].car;
    if (other < 12) load(user, opponent_banks[other]);
    load(user, race->track == 7 ? "COLLSWMT.BNK" : "COLLSWWT.BNK");
    load(user, "NFS_FMmb.bnk");
    race_sound_start_voices(race);
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

void race_sound_stop_engine(RaceState *race)
{
    race_sound_channel_stop(race, (int16_t)race->engine_sound.channel);
    race_sound_channel_stop(race, ROAD_VOICE);
    race_sound_channel_stop(race, ENGINE_STOP_4);
    race_sound_channel_stop(race, ENGINE_STOP_3);
    race->unknown_4bc6f0 = 0;
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
