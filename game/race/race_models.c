#include "game/race/race_models.h"

#include <stdint.h>

enum {
    HUMAN_SET = 200,            /* the humans' car file set */
    OTHER_SET = 100,            /* 004fb787 set: the other humans' and opponents' set */
    POLICE_MODEL = 400,
    POLICE_SLOT = 2,
    CAR_8 = 8,                  /* the car whose opponents are 004c5628's */
    TRAFFIC_MODELS = 12,        /* race_random() * 12 >> 16 */
    TYPE_3_SLOTS = 8,
    RANDOM_PERIOD = 0x10000     /* race_random keeps 16 bits of state */
};

/* 00426620: 00426fec's table, the head to head opponent by 004fbad0. */
static const int16_t HEAD_TO_HEAD[16] = { 7, 6, 0, 2, 3, 5, 4, 1, 8, 9, 10, 11, 12, 13, 14, 15 };
/* 004c5628: the opponents of car 8, by slot. */
static const uint8_t CAR_8_OPPONENTS[TYPE_3_SLOTS] = { 7, 6, 0, 3, 2, 5, 1, 4 };

/* 00426fec(004fbad0); -1 past its table (the original reads its stack). */
static int32_t head_to_head(const RaceState *race)
{
    const int16_t at = (int16_t)race->unknown_4fbad0;
    return at >= 0 && at < 16 ? HEAD_TO_HEAD[at] : -1;
}

static void set_model(RaceState *race, int32_t slot, int32_t model)
{
    if ((uint32_t)slot < RACE_CARS) race->models[slot] = model;  /* port guard */
}

/* Type 2: every opponent the same car, the other entry's or the head to
 * head pick; 004c5628's when that is car 8. */
static void single_race(RaceState *race, int32_t set, int32_t other)
{
    const int32_t pick = head_to_head(race);
    const int32_t other_car = race->entries[other].car;
    if ((race->humans == 1 && other_car == CAR_8) || (race->humans > 1 && pick == CAR_8)) {
        for (int32_t i = race->humans; i < race->racers && i < TYPE_3_SLOTS; i++)
            set_model(race, i, CAR_8_OPPONENTS[i] + set);
        return;
    }
    if (other_car > 7) return;
    if (race->humans > 1) {
        if (pick < 0) return;  /* port guard */
        for (int32_t i = race->humans; i < race->racers; i++) set_model(race, i, pick + set);
        return;
    }
    for (int32_t i = race->humans; i < race->racers; i++) set_model(race, i, other_car + set);
}

/* Type 3: three cars chosen by the player's, in turn up to slot 7. */
static void tournament(RaceState *race, int32_t set)
{
    int32_t three[3];
    switch (race->entries[race->player & (RACE_PLAYERS - 1)].car) {
    case 0: case 6: case 7:
        three[0] = 0, three[1] = 7, three[2] = 6;
        break;
    case 1: case 4:
        three[0] = 4, three[1] = 1, three[2] = 4;
        break;
    case 2: case 3: case 5:
        three[0] = 5, three[1] = 3, three[2] = 2;
        break;
    default:
        return;  /* port guard: the original reads unset stack words */
    }
    if (race->humans >= TYPE_3_SLOTS || race->humans < 0) return;
    for (int32_t i = race->humans; i < TYPE_3_SLOTS; i++) set_model(race, i, three[i % 3] + set);
}

/* Types 0 and 1: the police car, the other entry's car, then the traffic
 * models, each drawn once. */
static void open_road(RaceState *race, int32_t other)
{
    if (race->race_type == 1 && race->humans == 1 && race->cops != 0) {
        race->unknown_4fbab8 = race->race_type;
        set_model(race, POLICE_SLOT, POLICE_MODEL);
    }
    if (race->humans == 1) set_model(race, other, race->entries[other].car + HUMAN_SET);
    int used[TRAFFIC_MODELS] = { 0 };
    int drawn = 0;
    for (int32_t i = race->racers + race->unknown_4fbab8;
         i < race->traffic_setting + race->racers + race->unknown_4fbab8; i++) {
        if (drawn == TRAFFIC_MODELS) return;  /* port guard: the original loops for ever */
        uint32_t model;
        int32_t tries = 0;
        do {
            if (tries++ > RANDOM_PERIOD) return;  /* port guard: the generator cycles on used models for ever */
            model = race_random(race) * TRAFFIC_MODELS >> 16;
        } while (used[model]);
        used[model] = 1;
        drawn++;
        set_model(race, i, (int32_t)model);
    }
}

void race_models(RaceState *race)
{
    const int32_t set = race->unknown_4fb787 != 0 ? OTHER_SET : HUMAN_SET;
    race->unknown_4fbab8 = 0;
    for (int32_t i = 0; i < race->humans && i < RACE_PLAYERS; i++) {  /* port guard: 8 entries */
        int32_t model = race->entries[i].car + HUMAN_SET;
        if (i != race->player && race->race_type > 1) model -= HUMAN_SET - set;
        set_model(race, i, model);
    }
    const int32_t other = race->player == 0;
    if (race->race_type == 2) single_race(race, set, other);
    else if (race->race_type == 3) tournament(race, set);
    else if (race->race_type <= 1) open_road(race, other);
}
