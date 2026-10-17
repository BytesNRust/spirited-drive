#include "game/sys/pointer_state.h"

int32_t pointer_state_in_rect(const PointerState *p, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    return (int16_t)x0 <= p->x && p->x <= (int16_t)x1 && (int16_t)y0 <= p->y && p->y <= (int16_t)y1;
}
