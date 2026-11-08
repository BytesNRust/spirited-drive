/* port (wipers): see wiper.h. */
#include "game/race/wiper.h"

#include <math.h>
#include <stddef.h>

enum { REACH_PERCENT = 105, HUB_PERCENT = 8 };   /* the blade reaches a little past its length (its tip's width); nothing is swept at the hub */

static const float PI = 3.14159265358979f;

WiperTiming wiper_timing(int kind)
{
    switch (kind) {   /* ticks at the race's 30 a second */
    case WIPER_LIGHT: return (WiperTiming){ 54, 30 };
    case WIPER_RAIN:  return (WiperTiming){ 36, 34 };
    case WIPER_STORM: return (WiperTiming){ 21, 20 };
    case WIPER_SNOW:  return (WiperTiming){ 45, 40 };
    default:          return (WiperTiming){ 0, 0 };
    }
}

static float fraction_at(int phase, int sweep)
{
    if (phase < 0 || phase >= sweep || sweep <= 0) return 0.0f;
    const float h = (float)sweep / 2.0f, p = (float)phase;
    return p <= h ? p / h : ((float)sweep - p) / h;
}

float wiper_fraction(uint32_t tick, WiperTiming t)
{
    if (t.period <= 0 || t.sweep <= 0) return 0.0f;
    return fraction_at((int)(tick % (uint32_t)t.period), t.sweep);
}

float wiper_angle(const WiperBlade *b, float fraction)
{
    return b->park + (b->top - b->park) * fraction;
}

static void axes(const WiperBlade *b, float *ux, float *uy, float *vx, float *vy)
{
    if (b->ux == 0.0f && b->uy == 0.0f && b->vx == 0.0f && b->vy == 0.0f) *ux = 1.0f, *uy = 0.0f, *vx = 0.0f, *vy = 1.0f;
    else *ux = b->ux, *uy = b->uy, *vx = b->vx, *vy = b->vy;
}

void wiper_offset(const WiperBlade *b, float r, float angle, float *dx, float *dy)
{
    float ux, uy, vx, vy;
    axes(b, &ux, &uy, &vx, &vy);
    const float p = r * cosf(angle), q = r * sinf(angle);
    *dx = p * ux + q * vx;
    *dy = -(p * uy + q * vy);   /* the screen's y runs down */
}

static float wrap(float a)
{
    while (a > PI) a -= 2.0f * PI;
    while (a <= -PI) a += 2.0f * PI;
    return a;
}

int wiper_point_fraction(const WiperBlade *b, float x, float y, float *fraction)
{
    if (b->xmax > 0.0f && x >= b->xmax) return 0;
    float ux, uy, vx, vy;
    axes(b, &ux, &uy, &vx, &vy);
    const float dx = x - b->x, dy = b->y - y, det = ux * vy - vx * uy;   /* y up */
    if (fabsf(det) < 1e-6f) return 0;
    const float p = (dx * vy - vx * dy) / det, q = (ux * dy - uy * dx) / det;   /* the point in the glass's plane */
    const float r = sqrtf(p * p + q * q);
    if (r > b->len * REACH_PERCENT / 100.0f || r < b->len * HUB_PERCENT / 100.0f) return 0;
    const float span = wrap(b->top - b->park);
    if (fabsf(span) < 1e-4f) return 0;
    const float f = wrap(atan2f(q, p) - b->park) / span;
    if (f < 0.0f || f > 1.0f) return 0;
    *fraction = f;
    return 1;
}

int64_t wiper_last_pass(const WiperBlade *b, WiperTiming t, uint32_t tick, float x, float y)
{
    float f;
    if (t.period <= 0 || t.sweep <= 0 || !wiper_point_fraction(b, x, y, &f)) return -1;
    const int64_t up = (int64_t)floorf(f * (float)t.sweep / 2.0f), down = (int64_t)t.sweep - up;
    const int64_t start = (int64_t)tick - (int64_t)(tick % (uint32_t)t.period);
    int64_t best = -1;
    if (start + down <= (int64_t)tick) best = start + down;   /* back over it in this cycle */
    else if (start + up <= (int64_t)tick) best = start + up;  /* up over it in this cycle */
    else if (start >= t.period) best = start - t.period + down;   /* the last cycle's way back */
    return best;
}

int64_t wiper_pass_latest(const WiperPass *w, uint32_t tick, float x, float y)
{
    int64_t best = -1;
    for (int i = 0; w != NULL && i < w->count && i < WIPER_BLADES; i++) {
        const int64_t p = wiper_last_pass(&w->blade[i], w->timing, tick, x, y);
        if (p > best) best = p;
    }
    return best;
}
