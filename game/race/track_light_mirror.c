#include "game/race/track_light_mirror.h"

#include <stdlib.h>
#include <string.h>

/* the run's node of the file's node `f` (the track's nodes: track_node_light_old_node's own mapping, which is its own inverse) */
static uint32_t node_of(uint32_t f, uint32_t nodes) { return (nodes - f % nodes) % nodes; }

/* a range from..to of the file's nodes, as the run has it (from <= to again; a range that began at node 0 reaches the last node) */
static void range_of(uint32_t *from, uint32_t *to, uint32_t nodes)
{
    const uint32_t a = node_of(*to, nodes), b = *from == 0 ? nodes - 1 : node_of(*from, nodes);
    *from = a, *to = b;
}

/* the points of a row as the run has them: the two chains swap (point p > 0 is (p + 4) % 10 + 1; the middle stays) */
static uint16_t mask_of(uint16_t mask)
{
    uint16_t out = (uint16_t)(mask & 1u);
    for (int p = 1; p < TRACK_NODE_LIGHT_POINTS; p++)
        if (mask >> p & 1u) out |= (uint16_t)(1u << ((p + 4) % 10 + 1));
    return out;
}

static int side_of(int side) { return side == TRACK_LIGHT_SIDE_LEFT ? TRACK_LIGHT_SIDE_RIGHT : side == TRACK_LIGHT_SIDE_RIGHT ? TRACK_LIGHT_SIDE_LEFT : side; }

static int mirror_rows(TrackLightFile *file, uint32_t nodes)
{
    const size_t row = TRACK_NODE_LIGHT_POINTS;
    if (file->light.data != NULL || file->row_set != NULL) {
        uint8_t *const data = file->light.data != NULL ? malloc((size_t)nodes * row) : NULL;
        uint8_t *const set = file->row_set != NULL ? malloc(nodes) : NULL;
        if ((file->light.data != NULL && data == NULL) || (file->row_set != NULL && set == NULL)) {
            free(data), free(set);
            return 0;
        }
        for (uint32_t f = 0; f < nodes; f++) {
            const uint32_t r = node_of(f, nodes);
            if (data != NULL) {
                data[(size_t)r * row] = file->light.data[(size_t)f * row];
                for (int p = 1; p < TRACK_NODE_LIGHT_POINTS; p++) data[(size_t)r * row + (size_t)((p + 4) % 10 + 1)] = file->light.data[(size_t)f * row + (size_t)p];
            }
            if (set != NULL) set[r] = file->row_set[f];
        }
        if (data != NULL) memcpy(file->light.data, data, (size_t)nodes * row);
        if (set != NULL) memcpy(file->row_set, set, nodes);
        free(data), free(set);
    }
    if (file->light.tunnel != NULL) {
        uint8_t *const marks = malloc(nodes);
        if (marks == NULL) return 0;
        for (uint32_t f = 0; f < nodes; f++) marks[node_of(f, nodes)] = file->light.tunnel[f];
        memcpy(file->light.tunnel, marks, nodes);
        free(marks);
    }
    return 1;
}

int track_light_file_mirror(TrackLightFile *file, uint32_t nodes)
{
    if (file == NULL || nodes < 2 || (file->light.data != NULL && file->light.nodes != nodes)) return 0;
    if (!mirror_rows(file, nodes)) return 0;
    TrackLightTweaks *const t = &file->tweaks;
    for (int i = 0; i < t->brightness_count; i++) {
        TrackLightBrightness *const b = &t->brightness[i];
        range_of(&b->from, &b->to, nodes);
        b->side = side_of(b->side), b->mask = mask_of(b->mask);
    }
    for (int i = 0; i < t->tone_run_count; i++) {
        range_of(&t->tone_runs[i].from, &t->tone_runs[i].to, nodes);
        t->tone_runs[i].mask = mask_of(t->tone_runs[i].mask);
    }
    for (int i = 0; i < t->zone_count; i++) range_of(&t->zones[i].from, &t->zones[i].to, nodes);
    for (int i = 0; i < t->gradient_count; i++) range_of(&t->gradients[i].from, &t->gradients[i].to, nodes);
    for (int i = 0; i < t->step_count; i++) range_of(&t->steps[i].from, &t->steps[i].to, nodes);
    for (int i = 0; i < t->lamp_count; i++) t->lamps[i].node = node_of(t->lamps[i].node, nodes), t->lamps[i].mask = mask_of(t->lamps[i].mask);
    for (int i = 0; i < t->tunnel_lamp_count; i++)
        if (!t->tunnel_lamps[i].general) range_of(&t->tunnel_lamps[i].from, &t->tunnel_lamps[i].to, nodes);
    return 1;
}
