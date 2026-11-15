/* See car_list.h. */
#include "game/frontend/car_list.h"

enum { WARRIOR = 11 };

void car_list_build(CarList *out, int restomod, int warrior)
{
    out->count = 0;
    for (int16_t car = 0; car < CAR_ORIGINAL_MODELS; car++) out->car[out->count++] = car;
    if (restomod) out->count += car_registry_cars(out->car + out->count, CAR_ADDED_MAX);
    if (warrior) out->car[out->count++] = WARRIOR;
}

int16_t car_list_step(const CarList *l, int16_t car, int step)
{
    if (l->count == 0) return 0;
    for (int i = 0; i < l->count; i++)
        if (l->car[i] == car) return l->car[(i + (step < 0 ? l->count - 1 : 1)) % l->count];
    return l->car[0];
}
