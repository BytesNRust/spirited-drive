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

enum { LEVELS = 6 };

void race_timer_level(RaceTimerLevel *l, int32_t level)
{
    static const int32_t w88[LEVELS] = { 4, 3, 2, 3, 2, 1 }, w8c[LEVELS] = { 3, 3, 3, 2, 2, 2 };
    if (level < 0 || level >= LEVELS) return;
    l->unknown_4fcd88 = w88[level];
    l->unknown_4fcd8c = w8c[level];
}

void race_timer_start(RaceTimerLevel *l, int32_t level, int32_t *ticks, int32_t *running)
{
    l->unknown_4fcd78 = 3;
    l->unknown_4fcd7c = 3;
    l->unknown_4fcd8c = 3;
    l->unknown_4fcd88 = 4;
    race_timer_level(l, level);
    *ticks = 0;
    *running = 1;
}

void race_timer_stop(int32_t *running) { *running = 0; }
