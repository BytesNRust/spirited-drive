/* The marks over the other players' cars (car_tag.h). */
#include "game/race/car_tag.h"

#include "game/gfx/x86_int.h"

enum { NEAR_TAG = 1000, MIDDLE_TAG = 2000, FAR_TAG = 3000 };

static void triangle(const CarTagOps *ops, const CarTagView *v, int32_t x, int32_t y, int32_t w, int32_t h,
                     int32_t slot)
{
    const int32_t top = x_sub(y, h), left = x_sub(x, w), right = x_add(x, w);
    const int32_t points[6] = { left, top, x_sub(right, 1), top, x, y };
    ops->polygon(ops->ctx, v->colours[slot], 3, points);
    ops->line(ops->ctx, left, top, right, top, v->outline);
    ops->line(ops->ctx, right, top, x, y, v->outline);
    ops->line(ops->ctx, left, top, x, y, v->outline);
}

void car_tag(const CarTagOps *ops, const CarTagView *v, int32_t x, int32_t y, int32_t slot, int32_t z)
{
    if (v->hidden != 0 || slot < 0) return;
    int32_t h = 9, w = 4;
    if (v->high == 0 || z > FAR_TAG) h = 3, w = 1;
    else if (z > MIDDLE_TAG) h = 5, w = 2;
    else if (z > NEAR_TAG) h = 8, w = 3;
    triangle(ops, v, x, y, w, h, slot);
    if (v->humans > slot) triangle(ops, v, x, x_sub(y, h), w, h, slot);
}
