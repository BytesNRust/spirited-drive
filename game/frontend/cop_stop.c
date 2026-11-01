#include "game/frontend/cop_stop.h"

#include <stdio.h>

enum { WIDE = 0x280, HIGH = 0x1e0 };

/* 00428610 */
void cop_stop(const CopStopState *st, const CopStopOps *ops)
{
    char name[0x50];
    if (*st->movie_on == 0) {
        /* the race's screen the cops ended over is freed before its place is taken (the port's pool is finite) */
        if (*st->screen != 0) ops->picture_free(ops->ctx, *st->screen);
        *st->screen = ops->picture_new(ops->ctx, WIDE, HIGH, 0);
        ops->screen_show(ops->ctx, "ticket.qfs", 0, 1);
        ops->picture_free(ops->ctx, *st->screen);
        return;
    }
    snprintf(name, sizeof name, *st->german ? "gcop%d.tgv" : "cop%d.tgv", (int16_t)ops->random_pick(ops->ctx) + 1);
    ops->movie(ops->ctx, name);
}
