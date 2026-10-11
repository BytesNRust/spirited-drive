#include "game/race/ai_police.h"

#include "game/race/ai_lanes.h"
#include "game/race/car_held.h"
#include "game/race/car_scenery.h"

enum {
    CHASING = 0x24,             /* flags_175: bits 2 (chasing) and 5 (gave up) */
    CHASE = 4,
    GAVE_UP = 0x20,
    ONCOMING = 0x10,
    OFF_DUTY = 0x80,            /* flags_175: drives on past the player */
    MARKED = 4,                 /* the player's flags_175 */
    NEAR = 8,
    PULLED_OVER = 1,            /* flags_176[0] */
    LOST = 8,                   /* the police car's flags_176[0] once behind */
    WAITING = 2,
    PHYSICS_HELD = 1,
    PHYSICS_CRASHED = 4,
    PARAM_STOPS = 7,            /* stops of the first kind before the second */
    PARAM_HOLD_SECOND = 9,      /* ticks held after a stop */
    PARAM_HOLD_FIRST = 10,
    STILL = 0x10000,            /* slower than this counts as stopped */
    SLOW = 0x30000,
    LEAN = 0x2e0000,            /* the wish's push to the road's side */
    STOPS_TO_END = 1,
    END_ARRESTED = 4,
    TRI_SPEED_TABLE = 0x15b0c   /* 3 bytes per segment (004c5cd0) */
};

/* Byte 1 of the track's segment table at a node, as 16.16. */
static int32_t chase_speed(const RaceState *race, uint32_t node)
{
    const size_t at = TRI_SPEED_TABLE + 3 * (size_t)(node >> 2) + 1;
    return at < race->tri->size ? (int32_t)((uint32_t)race->tri->data[at] << 16) : 0;  /* port guard */
}

static PlayerRecord *record_of(RaceState *race, const Car *car)
{
    return car->index >= 0 && car->index < RACE_PLAYERS + 1 ? &race->players[car->index] : NULL;  /* port guard */
}

static void stop(Car *cop, RaceState *race, Car *player, int32_t slot)
{
    PlayerRecord *record = record_of(race, player);
    if (race->ai.unknown_364 < race->ai.params[PARAM_STOPS]) {
        if (race->phase == 1 && record != NULL) record->police_stops[1]++;
        race_stop_notice_second(race);
        player->unknown_148 = race->ai.params[PARAM_HOLD_SECOND];
    } else {
        if (race->phase == 1 && record != NULL) record->police_stops[0]++;
        race_stop_notice_first(race);
        player->unknown_148 = race->ai.params[PARAM_HOLD_FIRST];
    }
    player->physics = PHYSICS_HELD;
    const Car *viewed = race->cars[race_slot(race->player)];
    if (viewed != NULL && viewed->unknown_088 == player->unknown_088 && viewed->physics == PHYSICS_CRASHED)
        race->sounds.start_engine(race->sounds.user);
    cop->flags_175 &= (uint8_t)~CHASE;
    cop->flags_176[0] &= (uint8_t)~LOST;
    player->flags_176[0] &= (uint8_t)~PULLED_OVER;
    race->unknown_4c5cd8 = 0;
    race->ai.unknown_364 = 0;
    ai_recheck_cars(race);
    car_beside_road(player, race, slot >= 0);
    if (record != NULL && record->police_stops[0] > STOPS_TO_END && race->phase == 1) {
        race->end_request = END_ARRESTED;
        race->end_state = END_ARRESTED;
        record->finish = 0;
        record->unknown_1c8 = 0;
        /* 0045aab0: an empty function */
    }
}

/* The wish leans to the road's side unless scenery stands at the police
 * car's node or the next two. */
static void lean(Car *cop, RaceState *race, int32_t slot, int32_t wish[3])
{
    const uint32_t node = (uint32_t)cop->node;
    const int scenery = car_scenery_at(cop, race->tri, (int32_t)node) || car_scenery_at(cop, race->tri, (int32_t)(node + 1)) ||
                        car_scenery_at(cop, race->tri, (int32_t)(node + 2));
    const TriNode n = race_node(race, (int32_t)node);
    if (n.flags[0] >> 4 == 0) {
        const int32_t lanes = n.flags[0] & 0xf;
        if (slot * 2 < lanes && !scenery) {
            wish[0] = (int32_t)((uint32_t)wish[0] + LEAN);
            if (slot * 2 + 1 == lanes) wish[2] = (int32_t)((uint32_t)wish[2] + LEAN);
            else wish[2] = (int32_t)((uint32_t)wish[2] - LEAN);
            return;
        }
    }
    if (!scenery) {
        wish[0] = (int32_t)((uint32_t)wish[0] - LEAN);
        wish[2] = (int32_t)((uint32_t)wish[2] + LEAN);
    }
}

void ai_police_stop(Car *cop, RaceState *race, int32_t slot, int32_t wish[3])
{
    Car *player = race->cars[0];
    const int32_t stops = race->ai.params[PARAM_STOPS];
    const uint32_t player_node = (uint32_t)player->node;
    int32_t gap = (int32_t)((uint32_t)cop->node - player_node);
    if (gap < 0 && (cop->flags_175 & CHASING) && !(cop->flags_176[0] & LOST)) cop->flags_176[0] |= LOST;
    if (stops <= race->ai.unknown_364 && race->ai.unknown_364 - 4 < stops && (cop->flags_175 & CHASING) && race->phase == 1)
        race->ai.unknown_33c++;
    if ((cop->flags_175 & CHASE) && player->crashed_hard == 0) {
        if (cop->ai_speed < STILL && player->speed < STILL && (player->flags_176[0] & PULLED_OVER))
            stop(cop, race, player, slot);
        else if (gap > 2 || (gap >= 0 && race->leader != NULL /* port guard: the original faults */ &&
                                  race->leader->speed < SLOW && cop->ai_speed < SLOW))
            player->flags_176[0] |= PULLED_OVER;
    }
    if ((cop->flags_175 & OFF_DUTY) || (player->flags_176[0] & PULLED_OVER)) lean(cop, race, slot, wish);
    if (gap < 1) gap = -gap;
    if (cop->flags_175 & CHASING) player->flags_175 |= MARKED;
    if (gap >= 10 || (cop->flags_175 & OFF_DUTY)) return;
    player->flags_175 |= NEAR;
    if (gap < 5 && race->unknown_4c5cd8 == 0 && chase_speed(race, player_node) < player->ai_speed) {
        race->ai.unknown_364 = 0;
        race->unknown_4c5cd8 = 1;
    }
    if (race->unknown_4c5cd8 != 0 && cop->node < (int32_t)player_node && (cop->flags_176[0] & WAITING))
        cop->flags_176[0] &= (uint8_t)~WAITING;
    if (race->unknown_4c5cd8 != 0 && !(cop->flags_175 & CHASE)) {
        if (!(cop->flags_175 & ONCOMING) && cop->node < (int32_t)player_node) cop->flags_175 |= CHASE;
        else cop->flags_175 |= GAVE_UP;
    }
}
