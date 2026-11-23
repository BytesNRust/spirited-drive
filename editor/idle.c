#include "idle.h"
#include "app.h"

int idle_draw(Idle *s, double now, int events, int busy)
{
    if (!s->started) s->started = 1, s->last_event = now, s->last_draw = -1e9;
    if (events || busy) s->last_event = now;
    const int draw = now - s->last_event < IDLE_AFTER || now - s->last_draw >= IDLE_BEAT;
    if (draw) s->last_draw = now;
    return draw;
}

int idle_resting(const Idle *s, double now)
{
    return s->started && now - s->last_event >= IDLE_AFTER;
}

int app_animating(const App *a)
{
    return a->kind == APP_KIND_CAR && (a->model_auto_rotate || a->night.police || (a->model_wheels == 1 && !a->model_wheels_fixed));
}
