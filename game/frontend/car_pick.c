/* See car_pick.h. */
#include "game/frontend/car_pick.h"

#include "game/race/car_registry.h"

int16_t car_pick_to_image(CarPick *p, int16_t pick)
{
    if (pick < CAR_ADDED_FIRST) {
        p->added = 0, p->stock = pick;
        return pick;
    }
    p->added = pick;
    return p->stock;
}

int16_t car_pick_from_image(const CarPick *p, int16_t image)
{
    return p->added >= CAR_ADDED_FIRST && image == p->stock ? p->added : image;
}
