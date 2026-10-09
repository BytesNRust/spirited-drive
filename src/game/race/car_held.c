#include "game/race/car_held.h"

void race_stop_notices_clear(RaceState *race)
{
    if (race->stop_notice[0] != 0 || race->stop_notice[1] != 0) race->stop_notice_tick = race->tick;
    race->stop_notice[0] = 0;
    race->stop_notice[1] = 0;
}
