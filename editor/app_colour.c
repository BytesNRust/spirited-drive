#include "app_colour.h"

#include "app_select.h"

#include <stdio.h>
#include <stdlib.h>

void app_set_colour_by(App *a, int mode)
{
    a->colour_by = mode > 0 && mode < TRACE_COLOUR_MODES ? mode : TRACE_COLOUR_LIMITS;
}

const uint8_t *app_colour_classes(App *a)
{
    if (a->colour_by == TRACE_COLOUR_LIMITS || !a->road_made || a->node_count < 1) return NULL;
    if (a->node_class == NULL || a->node_class_mode != a->colour_by) {
        uint8_t *classes = realloc(a->node_class, (size_t)a->node_count);
        if (classes == NULL) return NULL;
        a->node_class = classes;
        a->node_class_mode = a->colour_by;
        track_trace_node_classes(a->nodes, a->node_count, (TraceColourBy)a->colour_by, a->node_class);
    }
    return a->node_class;
}

int app_select_stretch(App *a, int node, int add)
{
    const uint8_t *classes = app_colour_classes(a);
    int first, last;
    if (classes == NULL || !track_trace_stretch_of(classes, a->node_count, node, &first, &last)) return 0;
    int lo = -1, hi = -1;
    for (int i = 0; i < a->point_count && i < a->trace.count; ++i) {
        const int at = app_nearest_node(a, i);
        if (at < first || at > last) continue;
        if (lo < 0) lo = i;
        hi = i;
    }
    if (lo < 0) {   // no point inside: the one nearest the stretch
        int best = -1, gap = 0;
        for (int i = 0; i < a->point_count && i < a->trace.count; ++i) {
            const int at = app_nearest_node(a, i), d = abs(at - node);
            if (best < 0 || d < gap) best = i, gap = d;
        }
        if (best < 0) return 0;
        lo = hi = best;
    }
    if (add) {   // one more stretch: its points join the set (Cmd's toggle only for those not in it yet)
        for (int i = lo; i <= hi; ++i)
            if (!sel_has(&a->sel, i)) app_sel_click(a, i, SEL_TOGGLE);
    } else {
        app_sel_click(a, lo, SEL_ONE);
        if (hi > lo) app_sel_click(a, hi, SEL_RANGE);
    }
    const char *how = add ? "added" : "selected";
    if (hi > lo) snprintf(a->status, sizeof a->status, "stretch %s: points %d to %d", how, lo, hi);
    else snprintf(a->status, sizeof a->status, "stretch %s: point %d", how, lo);
    return 1;
}

void app_colour_summary(App *a, char *out, size_t cap)
{
    const uint8_t *classes = app_colour_classes(a);
    const int runs = classes != NULL ? track_trace_stretches(classes, a->node_count, NULL, 0) : 0;
    TraceStretch *stretch = runs > 0 ? malloc(sizeof *stretch * (size_t)runs) : NULL;
    size_t used = (size_t)snprintf(out, cap, "classes %d:", a->colour_by);
    if (stretch != NULL) track_trace_stretches(classes, a->node_count, stretch, runs);
    for (int k = 0; stretch != NULL && k < runs && used < cap; ++k)
        used += (size_t)snprintf(out + used, cap - used, " %d@%d-%d", stretch[k].klass, stretch[k].first, stretch[k].last);
    free(stretch);
}
