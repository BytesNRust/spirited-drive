#include "game/race/car_variant.h"

/* the index in `list` of the k-th offered variant (k from 1), or -1 */
static int offered_at(const CarVariant *list, int count, int model, int k)
{
    for (int i = 0; i < count; i++)
        if (list[i].offered(model) && --k == 0) return i;
    return -1;
}

int car_variant_choices(const CarVariant *list, int count, int model)
{
    int offered = 0;
    for (int i = 0; i < count; i++) offered += list[i].offered(model) != 0;
    return offered == 0 ? 0 : offered + 1;
}

int car_variant_current(const CarVariant *list, int count, int model)
{
    int k = 0;
    for (int i = 0; i < count; i++) {
        if (!list[i].offered(model)) continue;
        k++;
        if (list[i].chosen(model)) return k;
    }
    return 0;
}

int car_variant_cycle(const CarVariant *list, int count, int model, int step)
{
    const int choices = car_variant_choices(list, count, model);
    if (choices == 0) return 0;
    const int next = (car_variant_current(list, count, model) + step + choices) % choices;
    for (int i = 0; i < count; i++)
        if (list[i].offered(model)) list[i].apply(model, offered_at(list, count, model, next) == i);
    return next;
}

int car_variant_draw(const CarVariant *list, int count, int model, const CarVariantCanvas *canvas)
{
    const int current = car_variant_current(list, count, model);
    const int shown = offered_at(list, count, model, current == 0 ? 1 : current);
    if (shown < 0) return 0;
    list[shown].draw(canvas, model, current != 0);
    return 1;
}
