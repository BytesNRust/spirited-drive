#include "game/race/track_object_walk.h"

static int32_t record_node(const TrackObjectWalk *w, int32_t index)
{
    if (index < 0 || index >= w->record_count) return -1;  /* port guard: the build keeps count <= record_count */
    const uint8_t *p = w->records + (uint32_t)index * TRACK_WALK_RECORD_SIZE;
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

/* 0044a804 */
void track_objects_from_last(TrackObjectWalk *w)
{
    if (w->count > TRACK_WALK_MAX_OBJECTS) w->count = 0;
    if (w->count <= 0) {
        w->node = -1;
        return;
    }
    w->cursor = w->first + w->count - 1;
    if (w->cursor >= w->record_count) w->cursor -= w->record_count;
    w->vertex_cursor = w->vertex_count;
    w->node = record_node(w, w->cursor);
}
