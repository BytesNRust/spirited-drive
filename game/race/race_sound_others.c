/* The other cars' sounds: the other human, the doppler, the sirens and the loudest cars (race_sound.h). */
#include "game/race/race_sound_private.h"
#include "game/race/car_walls.h"

enum { DOPPLER_NEAR = 0x96000, DOPPLER_SPEED = 0x1bcccc, DOPPLER_SCALE = 0x1bcccc00 };

enum { OTHER_HUMAN_TICKS = 999, OTHER_HUMAN_KIND = 1 };

void race_sound_other_human(RaceState *race)
{
    const int32_t viewed = race->viewed;
    for (int16_t i = 0; i < race->humans; i = (int16_t)(i + 1)) {
        if (i == viewed) continue;
        const Car *car = race->cars[race_slot(i)];  /* port guard */
        if (car == NULL || car->horn == 0) continue;
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

enum {
    POLICE_SLOT = 2, ACTIVE = 4, SIREN_FLAGS = 0xc, PHYSICS_CRASH_MODEL = 4, PHYSICS_HELD_MODEL = 3,
    SIREN_NODES = 0x40, SIREN_STEPS = 6, SIREN_RANGE = 0x400000, SIREN_PITCH = 0x40, SIREN_PITCH_LOW = 5,
    SIREN_CHANNEL = 9, SIREN_SOUND = 0x62, HORN_CHANNEL = 11, HORN_SOUND = 0x64, HORN_COUNT = 3,
    OTHER_RANGE = 0x200000, OTHER_LEVEL_SHIFT = 6,
    LOUD_CHANNEL = 2, SECOND_CHANNEL = 3, RACER_SOUND_LOUD = 0x43, RACER_SOUND_SECOND = 0x45,
    TRAFFIC_SOUND_LOUD = 0x30, TRAFFIC_SOUND_SECOND = 0x32, RACER_PITCH = 64, TRAFFIC_PITCH = 95,
    SKID_SOUND = 4, SKID_BYTE = 1, TICK_FOURTH = 4
};

/* 004c5fc8: a car's engine pitch by its slot (+0x516) */
static const int32_t ENGINE_PITCH_BY_SLOT[11] = { 0, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45 };

static int32_t engine_pitch(int32_t slot)
{
    return ENGINE_PITCH_BY_SLOT[(uint32_t)slot < 11 ? slot : 0];  /* port guard: the original reads on unchecked */
}

/* x, y, z of `car` from the listener as magnitudes */
static void apart3(const RaceState *race, const Car *car, int32_t d[3])
{
    d[0] = magnitude(x_sub(car->x, race->chase.position[0]));
    d[1] = magnitude(x_sub(car->y, race->chase.position[1]));
    d[2] = magnitude(x_sub(car->z, race->chase.position[2]));
}

/* 00472666..0047272c: the police car near the heard car: 004b92d4 from how
 * many nodes apart (unless either crashed), halved when it is not ahead;
 * with a siren flag (+0x176 & 0xc) within 0x400000 it is heard. */
static int siren_near(RaceState *race, const Car *heard, const Car *cop, const Car **siren, int32_t *siren_distance)
{
    int near = 0;
    if (heard->physics != PHYSICS_CRASH_MODEL && cop->physics != PHYSICS_CRASH_MODEL) {
        int32_t pulse = x_sub(SIREN_NODES, magnitude(x_sub(cop->node, heard->node))) / 8;
        race->unknown_4b92d4 = pulse > SIREN_STEPS ? SIREN_STEPS : pulse < 0 ? 0 : pulse;
        near = 1;
    }
    if (race->unknown_4b92d4 != 0 && cop->node <= heard->node) race->unknown_4b92d4 /= 2;
    if ((cop->flags_176[0] & SIREN_FLAGS) == 0) return near;
    int32_t d[3];
    apart3(race, cop, d);
    const int32_t distance = estimate(estimate(d[0], d[2]), d[1]);
    if (distance < SIREN_RANGE) *siren = cop, *siren_distance = distance;
    return near;
}

typedef struct {
    const Car *car;
    int32_t level;
} Heard;

/* 0047272c..00472896: every other active car (not the police car while the
 * chase is on) within 0x200000 flat, its engine level by distance and speed;
 * the loudest two kept. */
static void loudest(const RaceState *race, const Car *heard, Heard *loud, Heard *second)
{
    for (int32_t i = 0; i < race->car_count; i++) {
        if (race->unknown_4fbab8 != 0 && i == POLICE_SLOT) continue;
        const Car *car = race->cars[race_slot(i)];
        if (car == NULL || car == heard || (car->flags_51a & ACTIVE) == 0) continue;  /* port guard: NULL */
        int32_t d[3];
        apart3(race, car, d);
        const int32_t flat = estimate(d[0], d[2]);
        if (flat >= OTHER_RANGE) continue;
        const int32_t distance = estimate(flat, d[1]);
        if (player_entry(race)->sound_volume == 0) continue;
        const int32_t speed = race_sound_speed_level(car, race->chase.car);
        const int32_t volume = distance < OTHER_RANGE
            ? x_mul(x_sar(x_sub(OTHER_RANGE, distance), 8), LEVEL_MAX) / 0x2000 : 0;
        const int32_t level = x_mul(volume, speed) / (1 << OTHER_LEVEL_SHIFT);
        if (level > loud->level) {
            *second = *loud;
            *loud = (Heard){ car, level };
        } else if (level > second->level) {
            *second = (Heard){ car, level };
        }
    }
}

/* 004728f3..00472a7c: the siren on channel 9 while its Doppler factor
 * holds and the police car is within a node of where it was last heard;
 * the heard car's node sound (+0x4aa) brings the second siren (11) every
 * fourth call; off, both stopped. */
static void siren(RaceState *race, const Car *heard, const Car *cop, int32_t distance)
{
    race->unknown_4c5fa0 = 0;
    if (cop == NULL || race->unknown_4fbab8 == 0) {
        if (race->siren_on == 0) return;
        race_sound_channel_stop(race, SIREN_CHANNEL);
        race_sound_channel_stop(race, HORN_CHANNEL);
        race->siren_on = 0;
        return;
    }
    const int32_t volume = distance < SIREN_RANGE ? x_mul(x_sub(SIREN_RANGE, distance), LEVEL_MAX) / SIREN_RANGE : 0;
    int32_t angle;
    race_sound_angle(race, cop, &angle);
    int32_t pitch = SIREN_PITCH;
    const int32_t factor = race_sound_doppler((Car *)cop, &race->chase);
    if (factor != 0 && magnitude(x_sub(race->siren_node, cop->node)) < 2) {
        pitch = x_mul(factor, SIREN_PITCH) / 256;
        const Car *police = race->cars[POLICE_SLOT];
        if (police != NULL && police->physics == PHYSICS_HELD_MODEL) {  /* port guard: NULL */
            race->siren_pitch = pitch;
        } else {
            race->siren_pitch = x_sub(race->siren_pitch, 1);
            if (race->siren_pitch < SIREN_PITCH_LOW) race->siren_pitch = 1;
        }
        race_sound_channel_at(race, SIREN_CHANNEL, SIREN_SOUND, race->siren_pitch, volume, angle);
        race->siren_on = 1;
    }
    if (heard->node_flag_4aa == 0) {
        race->unknown_4c5fa0 = 0;
        race_sound_channel_stop(race, HORN_CHANNEL);
    } else if (race->siren_count == 0) {
        race_sound_channel_at(race, HORN_CHANNEL, HORN_SOUND, pitch, volume, angle);
        race->unknown_4c5fa0 = 1;
        race->siren_count = HORN_COUNT;
    } else if (race->unknown_4c5fa0 == 0) {
        race->siren_count = x_sub(race->siren_count, 1);
    }
}

/* 00472ac6..00472c6c / 00472c71..00472e27: one heard car on its channel:
 * the skid effect (00444000) while it skids (+0x409) and the heard human
 * has no finish; else its engine at its slot's pitch plus its Doppler
 * factor's share; none: the channel stopped while it has a voice. */
static void other_car(RaceState *race, int32_t channel, const Heard *h, int racer_sound, int traffic_sound)
{
    if (h->car == NULL) {
        if (race->voices[channel].voice >= 0) race_sound_channel_stop(race, channel);
        return;
    }
    Car *car = (Car *)h->car;
    int32_t angle;
    race_sound_angle(race, car, &angle);
    const int16_t heard = (int16_t)race->viewed;  /* the pick's slot as a short */
    if (car->skid_rear != 0 && race->players[(uint16_t)heard <= RACE_PLAYERS ? heard : 0].finish == 0) {  /* port guard */
        if (car->index >= race->humans)
            race_sound_effect_scaled(race, channel, x_sar(car->skid_rear, 8), car->skid_rear, car->skid_front, h->level, angle);
        else
            race_sound_effect_scaled(race, channel, SKID_SOUND, SKID_BYTE, car->skid_front, h->level, angle);
        return;
    }
    const int32_t factor = race_sound_doppler(car, &race->chase);
    if (factor == 0) return;
    if (car->index >= 0 && car->index < race->racers)
        race_sound_channel_at(race, channel, racer_sound, x_add(engine_pitch(car->index), x_shl(factor, 6) / 256), h->level, angle);
    else
        race_sound_channel_at(race, channel, traffic_sound, x_add(engine_pitch(car->index), x_mul(factor, TRAFFIC_PITCH) / 256),
                              h->level, angle);
}

void race_sound_others(RaceState *race, int32_t *police_lights)
{
    const Car *heard = race->cars[race_slot(race->viewed)];  /* 0052b316 / 004fbb08 / 00513e6c pick the same car */
    if (heard == NULL) return;  /* port guard */
    const Car *cop = NULL;
    int32_t cop_distance = 0;
    int near = 0;
    const Car *police = race->cars[POLICE_SLOT];
    if (race->unknown_4fbab8 != 0 && police != NULL && (police->flags_51a & ACTIVE) != 0 &&
        magnitude(x_sub(police->node, heard->node)) < SIREN_NODES)
        near = siren_near(race, heard, police, &cop, &cop_distance);
    Heard loud = { NULL, 0 }, second = { NULL, 0 };
    loudest(race, heard, &loud, &second);
    if (race->unknown_4fbab8 != 0) {
        if (!near) {
            race->unknown_4b92d4 = 0;
            *police_lights = 0;
        } else if (race->tick_flags & TICK_FOURTH) {
            *police_lights = x_sub(1, *police_lights);
        }
    }
    if (player_entry(race)->sound_volume == 0) return;
    siren(race, heard, cop, cop_distance);
    if (race->unknown_4fbab8 != 0 && police != NULL) race->siren_node = police->node;  /* port guard: NULL */
    if (loud.car == race->sound_second || second.car == race->sound_loud) {
        const Heard t = loud;
        loud = second, second = t;
    }
    other_car(race, LOUD_CHANNEL, &loud, RACER_SOUND_LOUD, TRAFFIC_SOUND_LOUD);
    if (loud.car != NULL) race->sound_loud = loud.car;
    other_car(race, SECOND_CHANNEL, &second, RACER_SOUND_SECOND, TRAFFIC_SOUND_SECOND);
    if (second.car != NULL) race->sound_second = second.car;
}
