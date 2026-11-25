#include "idle.h"
#include "app.h"

int idle_draw(Idle *s, double now, int events, int busy, unsigned long stamp, int typing)
{
    if (!s->started) s->started = 1, s->last_event = now, s->last_draw = -1e9, s->stamp = stamp;
    if (events || busy) s->last_event = now;
    const int draw = now - s->last_event < IDLE_AFTER || stamp != s->stamp || (typing && now - s->last_draw >= IDLE_BEAT) || now - s->last_draw >= IDLE_SAFETY;
    if (draw) s->last_draw = now, s->stamp = stamp;
    return draw;
}

unsigned long app_state_stamp(const App *a)
{
    unsigned long h = 1469598103934665603UL;
    for (const char *p = a->status; *p != '\0'; ++p) h = (h ^ (unsigned char)*p) * 1099511628211UL;
    const unsigned long parts[] = { (unsigned long)a->tmask_serial, (unsigned long)a->mask.generation, (unsigned long)a->tweak_st_ok, (unsigned long)a->kind };
    for (size_t i = 0; i < sizeof parts / sizeof parts[0]; ++i) h = (h ^ parts[i]) * 1099511628211UL;
    return h;
}

int idle_resting(const Idle *s, double now)
{
    return s->started && now - s->last_event >= IDLE_AFTER;
}

int app_animating(const App *a)
{
    return a->kind == APP_KIND_CAR && (a->model_auto_rotate || a->night.police || (a->model_wheels == 1 && !a->model_wheels_fixed));
}
