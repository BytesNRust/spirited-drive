/* See car_list.h. */
#include "game/frontend/car_list.h"

#include "formats/stock_content.h"

enum { WARRIOR = 11 };

void car_list_build(CarList *out, const Edition *e, int warrior)
{
    out->count = 0;
    if (e == NULL) {
        for (int16_t car = 0; car < CAR_ORIGINAL_MODELS; car++) out->car[out->count++] = car;
        if (warrior) out->car[out->count++] = WARRIOR;
        return;
    }
    for (int i = 0; i < e->cars && out->count < CAR_LIST_MAX; i++) {
        const int stock = stock_car_of_id(e->car[i]);
        const int car = stock >= 0 ? stock : car_registry_find(e->car[i]);
        if (car >= 0) out->car[out->count++] = (int16_t)car;
    }
}

int car_list_has(const CarList *l, int16_t car)
{
    for (int i = 0; i < l->count; i++)
        if (l->car[i] == car) return 1;
    return 0;
}

int16_t car_list_step(const CarList *l, int16_t car, int step)
{
    if (l->count == 0) return 0;
    for (int i = 0; i < l->count; i++)
        if (l->car[i] == car) return l->car[(i + (step < 0 ? l->count - 1 : 1)) % l->count];
    return l->car[0];
}

int16_t car_list_step_gallery(const CarList *l, int16_t car, int step)
{
    int16_t to = car;
    for (int i = 0; i < l->count; i++) {
        to = car_list_step(l, to, step);
        if (to == car || (to >= 0 && to < CAR_ORIGINAL_MODELS) || to == WARRIOR || car_registry_is_added(to)) return to;
    }
    return car;
}
