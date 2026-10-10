/* Physics model 6 (00470518): a traffic car parked off the road, waiting
 * for its turn to come back. */
#include "game/race/ai_drive.h"

#include "game/race/ai_lanes.h"

enum {
    RACER = 0x4,                  /* flags_174 */
    ZONE_WAKE_NODE = 0x78,        /* before the racers pass it the car may take a zone */
    PARK_X = 0x80000,
    PARK_Z = 0x80000,
    PARK_Y_BASE = 200,            /* y = (index x 10 + 200) << 16: each car high above the track */
    PARK_Y_STEP = 10,
    RETURN_NODE_STEP = 0x10,      /* the racers' furthest node must reach index x 16 + 15 */
    RETURN_NODE_BASE = 0xf,
    TRAFFIC_OFF = 0x4,            /* track_flags */
    PARAM_COPS_AT = 13,           /* ai params: taken off the traffic setting */
    PHYSICS_COMPUTER = 3          /* 0046d79c */
};

void ai_parked_tick(Car *car, RaceState *race)
{
    int32_t furthest = 0;
    for (int32_t i = 0; i < race->racers && i < RACE_PLAYERS; i++)  /* port guard: 8 cars */
        if (furthest < race->cars[i]->node) furthest = race->cars[i]->node;
    if (!(car->flags_174 & RACER)) {
        car->zone = 0;
        if (furthest < ZONE_WAKE_NODE && (race_random(race) & 1)) car->zone = 1;
    }
    car->node = 1;
    car->progress = 1;
    car->x = PARK_X;
    car->z = PARK_Z;
    Car *self = race_car_by_handle(race, car->unknown_088);
    if (self == NULL) return;  /* port guard: the original's +0x88 always points at a car */
    car->y = (self->index * PARK_Y_STEP + PARK_Y_BASE) << 16;
    if (--car->unknown_148 != 0) return;
    if (furthest < self->index * RETURN_NODE_STEP + RETURN_NODE_BASE ||
        race->traffic_setting - race->ai.params[PARAM_COPS_AT] >= race->ai.traffic_placed ||
        (car->flags_174 & RACER) || (race->track_flags & TRAFFIC_OFF)) {
        car->unknown_148 = 1;
        return;
    }
    race->ai.traffic_placed--;
    self->physics = PHYSICS_COMPUTER;
    ai_find_spawn(car, race);
}
