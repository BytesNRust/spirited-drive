/* A wide line darkened through the shade remap (00491fa0). */
#include "game/gfx/thick_line.h"

#include "game/gfx/flat.h"
#include "game/gfx/x86_int.h"
#include "game/sys/fixed_math.h"

enum { QUARTER_TURN = 0x100 };

void thick_line_shade(const QuadTarget *target, const uint8_t *remap, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                      int32_t width)
{
    int32_t dy = x_sub(y1, y0);
    dy = x_add(dy, dy >> 2);
    const int32_t angle = fx_atan2_10(x_sub(x1, x0), dy);
    const int32_t across = fx_mul_round(width, x_sub(0, fx_sin((uint32_t)angle + QUARTER_TURN)));
    int32_t down = fx_mul_round(width, fx_sin((uint32_t)angle));
    down = x_add(x_sub(down, down >> 2), down >> 4);
    const int32_t half_across = across >> 1, half_down = down >> 1;
    const int32_t rest_across = x_sub(across, half_across), rest_down = x_sub(down, half_down);
    const int32_t points[8] = { x_add(x0, half_across), x_add(y0, half_down), x_add(x1, half_across), x_add(y1, half_down),
                                x_sub(x1, rest_across), x_sub(y1, rest_down), x_sub(x0, rest_across), x_sub(y0, rest_down) };
    flat_polygon_shade(target, remap, 4, points, 0);
}
