#include "game/viewer/draw_rands.h"

void draw_rands_frame(DrawRands *d, int ticked)
{
    if (ticked) d->count = 0;
    d->logging = ticked;
    d->at = 0;
}

int32_t draw_rands_take(DrawRands *d, int32_t n, int32_t (*take)(void *user, int32_t n), void *user)
{
    if (n <= 0) return 0;
    if (!d->logging) {
        if (d->at >= d->count) return 0;
        const int i = d->at++;
        return d->n[i] == n ? d->value[i] : d->value[i] % n;
    }
    const int32_t r = take(user, n);
    if (d->count < DRAW_RANDS) {
        d->value[d->count] = r;
        d->n[d->count] = n;
        d->count++;
    }
    return r;
}
