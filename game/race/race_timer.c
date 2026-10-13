/* The race's timer callback (race_timer.h). */
#include "game/race/race_timer.h"

#include "game/gfx/x86_int.h"

enum { RACING = 1, OVER = 3, POLL_EVERY = 2 };

void race_timer_tick(const RaceTimerOps *ops, RaceTimerWords *w)
{
    if (w->w4fcdac != 0 || w->waiting != 0) return;
    ops->lock(ops->ctx);
    if (w->phase == RACING && w->ticks % POLL_EVERY == 0) ops->poll(ops->ctx);
    if (w->phase != OVER) w->ticks = x_add(w->ticks, 1);
    ops->unlock(ops->ctx);
}
