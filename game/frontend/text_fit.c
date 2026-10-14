#include "game/frontend/text_fit.h"

#include <string.h>

/* 00463d34 */
void text_fit_shadowed(const char *text, int32_t x, int32_t y, int32_t width, const TextFitOps *ops)
{
    enum { BUF = 0x32, FIRST = 7, SHADOW = 2 };
    char buf[BUF] = { 0 };
    for (int n = FIRST; n < BUF; n++) {
        strncpy(buf, text, (size_t)n);
        if (ops->width(ops->ctx, buf) > width) break;
    }
    ops->ramp(ops->ctx, 1);
    ops->draw(ops->ctx, buf, (int32_t)((uint32_t)x + SHADOW), (int32_t)((uint32_t)y + SHADOW));
    ops->ramp(ops->ctx, 0);
    ops->draw(ops->ctx, buf, x, y);
}
