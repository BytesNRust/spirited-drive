/* Cars against the scenery and each other, once a tick (0040d8e8). */
#include "game/race/car_collide.h"

#include "game/race/ai_lanes.h"
#include "game/race/ai_manager.h"
#include "game/race/car_held.h"
#include "game/race/car_scenery.h"

enum {
    SOUND_FLOOR = 0x6666,   /* the crash sound's level once its ticks run out */
    MANAGER_TICKS = 4,
    PHYSICS_GONE = 6,
    NEAR_END = 4,           /* nodes before the track's end: the cars past it are looked at too */
    AHEAD = 2,              /* nodes ahead past which the cars further on are out of reach */
    CHECKS = 6,             /* flags_51a: this car's own checks */
    TOUCHABLE = 5           /* flags_51a: another car it can touch */
};

static int32_t magnitude(int32_t v)
{
    return v > 0 ? v : -v;
}

/* Whether a player's car lies outside the lanes at its node: right of
 * (node +4 & 0xf) lane widths, or left of (node +4 >> 4). */
static int off_the_lanes(const Car *car, const RaceState *race)
{
    const TriNode at = race_node(race, car->node);
    const int32_t width = ai_lane_width(race, at.flags[0] & 0xf, at.right_verge) << 8;
    if ((int32_t)(at.flags[0] & 0xf) * width < car->road_lateral) return 1;
    return (int32_t)(at.flags[0] >> 4) * -width > car->road_lateral;
}

static int is_player(const Car *car, const RaceState *race)
{
    return car->index >= 0 && car->index < race->humans;
}

/* The cars after place `place` in the road's order, up to the car count
 * (4 more, wrapping to the first, near the track's end), that this car can
 * touch: each within both cars' radii in x and z is hit (0040d714). Past 2
 * nodes ahead the search stops unless the car is near the track's end. */
static void touch_cars_ahead(Car *car, RaceState *race, int32_t place, int32_t end)
{
    const int32_t limit = end - car->node < NEAR_END ? race->car_count + NEAR_END : race->car_count;
    if ((car->flags_51a & CHECKS) != CHECKS) return;
    for (int32_t j = place + 1; j < limit; j++) {
        const int32_t slot = j < race->car_count ? race->by_node[j] : race->by_node[j % race->car_count];
        Car *other = race->cars[slot];
        if (car->node + AHEAD < other->node && end - car->node > NEAR_END) break;
        if ((other->flags_51a & TOUCHABLE) != TOUCHABLE) continue;
        const int32_t reach = car->body_radius + other->body_radius;
        const int32_t dx = magnitude(car->x - other->x);
        const int32_t dz = magnitude(car->z - other->z);
        if (dx < reach && dz < reach) car_cars_hit(car, other, race);
    }
}

void race_cars_contacts(RaceState *race)
{
    if (race->crash_sound_ticks != 0) race->crash_sound_ticks--;
    else race->crash_sound_level = SOUND_FLOOR;
    if (--race->manager_countdown == 0) {
        ai_manager_tick(race);
        race->manager_countdown = MANAGER_TICKS;
    }
    for (int32_t place = 0; place < race->car_count; place++) {
        Car *car = race->cars[race->by_node[place]];
        const int32_t end = race->segments * 4;
        if (is_player(car, race)) car_pulled_over(car, race);
        if (is_player(car, race) && off_the_lanes(car, race)) car_knocks_scenery(car, race);
        if (car->physics != PHYSICS_GONE) touch_cars_ahead(car, race, place, end);
    }
}
