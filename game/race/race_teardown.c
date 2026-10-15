#include "game/race/race_teardown.h"

void race_scenery_state_free(const RaceFreeOps *ops, uint32_t *scenery_state)
{
    ops->free(ops->ctx, *scenery_state);
    *scenery_state = 0;
}

void race_rrsm_free(const RaceFreeOps *ops, uint32_t *rrsm)
{
    if (*rrsm == 0) return;
    ops->free(ops->ctx, *rrsm);
    *rrsm = 0;
}

void race_sky_ring_free(const RaceFreeOps *ops, uint32_t ring)
{
    ops->free(ops->ctx, ring);
}

void race_track_free(const RaceTrackFreeOps *ops, RaceTrackFiles *t)
{
    ops->close(ops->ctx, t->file);
    ops->free(ops->ctx, t->cache);
    t->unknown_5041ec = 0;
    t->unknown_4bba5c = 0;
    ops->free(ops->ctx, t->unknown_5041f0);
    ops->free(ops->ctx, t->header);
    ops->free(ops->ctx, t->terrain);
}

static void free_held(const RaceDrawFreeOps *ops, uint32_t *surface)
{
    if (*surface == 0) return;
    ops->free(ops->ctx, *surface);
    *surface = 0;
}

void race_draw_free(const RaceDrawFreeOps *ops, RaceDrawSurfaces *s, int32_t back_buffer)
{
    free_held(ops, &s->mirror);
    free_held(ops, &s->view);
    free_held(ops, &s->back);
    free_held(ops, &s->map);
    if (back_buffer != 0) free_held(ops, &s->frame);
    ops->track_info_free(ops->ctx);
    ops->track_free(ops->ctx);
    s->set_up = 0;
    s->drawn = 0;
}
