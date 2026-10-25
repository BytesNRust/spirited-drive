#include "game/gfx/raster_record.h"

#include <string.h>

static RasterRunner runner;

void raster_record_set_runner(RasterRunner r) { runner = r; }

enum { BAND_MIN = 8 };   /* a shorter list is replayed whole on the caller: a pool batch would cost more than it saves */

struct RasterRecorder *raster_active;

void raster_record_begin(struct RasterRecorder *r, const uint8_t *pixels)
{
    r->pixels = pixels;
    r->count = 0;
    r->top = INT32_MAX;
    r->bottom = INT32_MIN;
    raster_active = r;
}

void raster_record_end(struct RasterRecorder *r)
{
    raster_record_flush(r);
    raster_active = NULL;
}

RasterCmd *raster_record_add(struct RasterRecorder *r, const QuadTarget *t, RasterReplay replay)
{
    if (r->count == RASTER_RECORD_MAX) raster_record_flush(r);
    RasterCmd *c = &r->cmds[r->count++];
    memset(c, 0, sizeof *c);
    c->replay = replay;
    c->pixels = t->pixels, c->pitch = t->pitch;
    c->clip_left = t->clip_left, c->clip_top = t->clip_top, c->clip_right = t->clip_right, c->clip_bottom = t->clip_bottom;
    c->top = INT32_MIN, c->bottom = INT32_MAX;
    if (t->clip_top < r->top) r->top = t->clip_top;
    if (t->clip_bottom > r->bottom) r->bottom = t->clip_bottom;
    return c;
}

void raster_cmd_rows(RasterCmd *c, const int32_t *xy, int points)
{
    if (points < 1) return;
    int32_t lo = xy[1], hi = xy[1];
    for (int i = 1; i < points; i++) {
        if (xy[2 * i + 1] < lo) lo = xy[2 * i + 1];
        if (xy[2 * i + 1] > hi) hi = xy[2 * i + 1];
    }
    c->top = lo > INT32_MIN + 1 ? lo - 1 : INT32_MIN;
    c->bottom = hi < INT32_MAX - 2 ? hi + 2 : INT32_MAX;
}

QuadTarget raster_cmd_target(const RasterCmd *c)
{
    return (QuadTarget){ c->pixels, c->pitch, c->clip_left, c->clip_top, c->clip_right, c->clip_bottom };
}

int raster_narrow(const QuadTarget *whole, int32_t top, int32_t bottom, QuadTarget *out)
{
    *out = *whole;
    if (top > out->clip_top) out->clip_top = top;
    if (bottom < out->clip_bottom) out->clip_bottom = bottom;
    return out->clip_top < out->clip_bottom;
}

void raster_record_replay(const struct RasterRecorder *r, int32_t top, int32_t bottom)
{
    for (int i = 0; i < r->count; i++) {
        const RasterCmd *c = &r->cmds[i];
        const int32_t from = top > c->top ? top : c->top, to = bottom < c->bottom ? bottom : c->bottom;
        if (from >= to) continue;
        const QuadTarget whole = raster_cmd_target(c);
        c->replay(c, &whole, from, to);
    }
}

static void replay_band(void *ctx, int top, int bottom) { raster_record_replay(ctx, top, bottom); }

void raster_record_flush(struct RasterRecorder *r)
{
    if (r->count == 0) return;
    const int32_t top = r->top < 0 ? 0 : r->top;
    struct RasterRecorder *const active = raster_active;
    raster_active = NULL;   /* the replays draw: the band threads read this */
    if (r->count < BAND_MIN || runner == NULL) raster_record_replay(r, top, r->bottom);
    else runner(top, r->bottom, replay_band, r);
    raster_active = active;
    r->count = 0;
    r->top = INT32_MAX;
    r->bottom = INT32_MIN;
}

void raster_record_sync(const QuadTarget *t)
{
    struct RasterRecorder *r = raster_recorder_for(t);
    if (r != NULL) raster_record_flush(r);
}
