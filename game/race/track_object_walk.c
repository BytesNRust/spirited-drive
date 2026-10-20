#include "game/race/track_object_walk.h"

#include "game/gfx/x86_int.h"

enum {
    WALK_FAR = 9999,      /* 0044a87c's first bound */
    WALK_BEHIND = 0x100   /* a record this far behind the node is a lap back */
};

static int32_t record_node(const TrackObjectWalk *w, int32_t index)
{
    if (index < 0 || index >= w->record_count) return -1;  /* port guard: the build keeps count <= record_count */
    const uint8_t *p = w->records + (uint32_t)index * TRACK_WALK_RECORD_SIZE;
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

static int32_t record_points(const TrackObjectWalk *w, int32_t index)
{
    if (index < 0 || index >= w->record_count) return 0;  /* port guard, as record_node */
    return w->descriptors[(uint32_t)w->records[(uint32_t)index * TRACK_WALK_RECORD_SIZE + 4] * TRACK_WALK_RECORD_SIZE + 1];
}

static void on_last(TrackObjectWalk *w)
{
    w->cursor = w->first + w->count - 1;
    if (w->cursor >= w->record_count) w->cursor -= w->record_count;
    w->vertex_cursor = w->vertex_count;
    w->node = record_node(w, w->cursor);
}

/* 0044a804 */
void track_objects_from_last(TrackObjectWalk *w)
{
    if (w->count > (w->max_objects > 0 ? w->max_objects : TRACK_WALK_MAX_OBJECTS)) w->count = 0;
    if (w->count <= 0) {
        w->node = -1;
        return;
    }
    on_last(w);
}

/* 0044a87c */
void track_objects_back_to(TrackObjectWalk *w, int32_t node)
{
    if (w->count <= 0) return;
    const int32_t target = (int32_t)((uint32_t)node & w->node_mask);
    on_last(w);
    int32_t d = x_sub(w->node, target), bound = WALK_FAR;
    /* port guard: the original runs on when no record ends the walk */
    for (int32_t steps = 0; steps <= 2 * w->record_count + 1; steps++) {
        if (!((d > 0 && d <= bound) || (w->circuit != 0 && x_sub(target, w->node) > WALK_BEHIND))) break;
        w->vertex_cursor = x_sub(w->vertex_cursor, record_points(w, w->cursor));
        if (--w->cursor < 0) {
            w->cursor = w->record_count - 1;
            w->node = w->circuit != 0 ? record_node(w, w->cursor) : -1;
        } else {
            w->node = record_node(w, w->cursor);
        }
        const int32_t last = d;
        d = x_sub(w->node, target);
        bound = d > 0 && last < -WALK_BEHIND ? d : last;
    }
}

/* 0044a9b8 */
void track_objects_start(TrackObjectWalk *w, int32_t segment)
{
    const int32_t node = (int32_t)((uint32_t)segment << 2);
    int32_t at = 0;
    if (record_node(w, 0) >= 0) {
        int32_t lo = 0, hi = w->record_count - 1;
        at = hi / 2;
        while (lo + 1 < hi) {
            at = (lo + hi) / 2;
            if (node > record_node(w, at)) lo = at;
            else hi = at;
        }
        /* back over the records on its node, onto the one before them */
        const int32_t on = record_node(w, at);
        while (at > 0 && record_node(w, at) == on) at--;
        while (node > record_node(w, at) && at < w->record_count - 1) at++;
    }
    w->first = at;
    w->cursor = at;
    w->vertex_total = 0;
    w->count = 0;
    w->vertex_count = 0;
    w->node = record_node(w, at);
}
