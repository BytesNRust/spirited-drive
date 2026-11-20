/* The track editor's group operations on the selected run of control points (app_points.h). */
#include "app_points.h"

#include "app_select.h"
#include "key_ops.h"
#include "point_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int app_points_run(App *a, const char *what, int *s, int *e)
{
    if (a->kind != APP_KIND_TRACK) return 0;
    app_sel_sync(a);
    const int first = sel_first(&a->sel), last = sel_last(&a->sel), count = sel_count(&a->sel);
    if (count < 2 || last - first + 1 != count) {
        snprintf(a->status, sizeof a->status, "%s wants a run of points without gaps, two or more (Shift+click its last point)", what);
        return 0;
    }
    *s = first, *e = last;
    return 1;
}

/* the trace's points and keys become these (the old ones freed); the run s..e is the selection */
static void take(App *a, TracePoint *points, int count, TraceKey *keys, int key_count, int s, int e)
{
    free(a->trace.points), free(a->trace.keys);
    a->trace.points = points, a->trace.count = count;
    a->trace.keys = keys, a->trace.key_count = a->trace.key_cap = key_count;
    sel_resize(&a->sel, count);
    sel_clear(&a->sel);
    sel_click(&a->sel, s, SEL_ONE);
    sel_click(&a->sel, e, SEL_RANGE);
    a->selected = s;
    a->dirty = 1;
    app_track_commit(a);   /* one step to undo */
}

static void run_text(const App *a, int s, int e, char *out, size_t cap)
{
    if (s == 0 && e == a->trace.count - 1) snprintf(out, cap, "the whole road");
    else snprintf(out, cap, "points %d-%d", s, e);
}

int app_copy_points(App *a)
{
    int s, e;
    if (!app_points_run(a, "Copy", &s, &e)) return 0;
    TraceKey *keys = NULL;
    int key_count = 0;
    TracePoint *points = malloc(sizeof *points * (size_t)(e - s + 1));
    if (points == NULL || !keys_clip(&a->trace, s, e, &keys, &key_count)) {
        free(points);
        snprintf(a->status, sizeof a->status, "Copy: out of memory");
        return 0;
    }
    memcpy(points, a->trace.points + s, sizeof *points * (size_t)(e - s + 1));
    free(a->clip_points), free(a->clip_keys);
    a->clip_points = points, a->clip_count = e - s + 1, a->clip_keys = keys, a->clip_key_count = key_count;
    snprintf(a->status, sizeof a->status, "%d points copied (%d-%d)", a->clip_count, s, e);
    return 1;
}

int app_paste_points(App *a)
{
    if (a->kind != APP_KIND_TRACK) return 0;
    if (a->clip_points == NULL) {
        snprintf(a->status, sizeof a->status, "Paste: copy a run of points first");
        return 0;
    }
    app_sel_sync(a);
    const int anchor = sel_count(&a->sel) > 0 ? sel_last(&a->sel) : a->trace.count - 1, m = a->clip_count;
    TraceKey *keys = NULL;
    int key_count = 0;
    TracePoint *points = pts_paste(a->trace.points, a->trace.count, anchor, a->clip_points, m);
    if (points == NULL || !keys_paste(&a->trace, anchor, a->clip_keys, a->clip_key_count, m, &keys, &key_count)) {
        free(points);
        snprintf(a->status, sizeof a->status, "Paste: out of memory");
        return 0;
    }
    take(a, points, a->trace.count + m - 1, keys, key_count, anchor, anchor + m - 1);
    snprintf(a->status, sizeof a->status, "%d points pasted after point %d", m - 1, anchor);
    return 1;
}

int app_mirror_points(App *a, int square)
{
    int s, e;
    if (!app_points_run(a, "Mirror", &s, &e)) return 0;
    TraceKey *keys = NULL;
    int key_count = 0;
    TracePoint *points = square ? pts_mirror_fb(a->trace.points, a->trace.count, s, e) : pts_mirror_lr(a->trace.points, a->trace.count, s, e);
    if (points == NULL || !keys_mirror(&a->trace, s, e, &keys, &key_count)) {
        free(points);
        snprintf(a->status, sizeof a->status, "Mirror: out of memory");
        return 0;
    }
    char run[48];
    run_text(a, s, e, run, sizeof run);
    take(a, points, a->trace.count, keys, key_count, s, e);
    snprintf(a->status, sizeof a->status, "%s mirrored %s", run, square ? "front/back" : "left/right");
    return 1;
}

int app_reverse_points(App *a)
{
    int s, e;
    if (!app_points_run(a, "Reverse", &s, &e)) return 0;
    TraceKey *keys = NULL;
    int key_count = 0;
    TracePoint *points = pts_reverse(a->trace.points, a->trace.count, s, e);
    if (points == NULL || !keys_reverse(&a->trace, s, e, &keys, &key_count)) {
        free(points);
        snprintf(a->status, sizeof a->status, "Reverse: out of memory");
        return 0;
    }
    char run[48];
    run_text(a, s, e, run, sizeof run);
    take(a, points, a->trace.count, keys, key_count, s, e);
    snprintf(a->status, sizeof a->status, "%s reversed", run);
    return 1;
}
