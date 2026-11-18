#include "app.h"

#include "build.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_status(App *a, const char *text)
{
    snprintf(a->status, sizeof a->status, "%s", text);
}

/* the last component of `dir` without trailing slashes: the id a new project takes */
static void folder_name(const char *dir, char *out, size_t cap)
{
    size_t n = strlen(dir);
    while (n > 1 && dir[n - 1] == '/') --n;
    size_t start = n;
    while (start > 0 && dir[start - 1] != '/') --start;
    const size_t len = n - start < cap - 1 ? n - start : cap - 1;
    memcpy(out, dir + start, len);
    out[len] = '\0';
}

static void drop_road(App *a)
{
    free(a->nodes);
    free(a->flags);
    a->nodes = NULL;
    a->flags = NULL;
    a->node_count = 0;
    a->road_made = 0;
    a->turn_nodes = 0;
    a->radius_nodes = 0;
}

int app_open(App *a, const char *dir, const char *game, const char *user)
{
    memset(a, 0, sizeof *a);
    snprintf(a->dir, sizeof a->dir, "%s", dir);
    snprintf(a->game, sizeof a->game, "%s", game);
    snprintf(a->user, sizeof a->user, "%s", user);
    a->selected = -1;
    char id[MOD_INI_ID_MAX + 1], why[APP_MSG];
    folder_name(dir, id, sizeof id);
    if (!project_create(dir, id, why, sizeof why) || !project_open(dir, &a->project, &a->trace, why, sizeof why)) {
        set_status(a, why);
        return 0;
    }
    a->dirty = 1;
    app_recompute(a);
    view_fit(&a->view, a->trace.points, a->trace.count, 960, 600);
    set_status(a, "opened: arrows and keys are listed at the bottom");
    return 1;
}

void app_free(App *a)
{
    free(a->trace.points);
    drop_road(a);
    a->trace.points = NULL;
    a->trace.count = 0;
}

void app_recompute(App *a)
{
    if (!a->dirty) return;
    a->dirty = 0;
    drop_road(a);
    a->length = track_trace_length(a->trace.points, a->trace.count);
    ExchangeError err;
    memset(&err, 0, sizeof err);
    a->road_text[0] = '\0';
    if (!track_trace_nodes(a->trace.points, a->trace.count, a->trace.width, &a->nodes, &a->node_count, &err)) {
        snprintf(a->road_text, sizeof a->road_text, "%s", err.message);
        return;
    }
    a->flags = calloc((size_t)a->node_count, 1);
    if (a->flags == NULL) {
        drop_road(a);
        snprintf(a->road_text, sizeof a->road_text, "out of memory");
        return;
    }
    a->road_made = 1;
    track_trace_flags(a->nodes, a->node_count, a->flags);
    for (int i = 0; i < a->node_count; ++i) {
        a->turn_nodes += (a->flags[i] & TRACE_FLAG_TURN) != 0;
        a->radius_nodes += (a->flags[i] & TRACE_FLAG_RADIUS) != 0;
    }
    if (a->turn_nodes > 0)
        snprintf(a->road_text, sizeof a->road_text, "%d node(s) turn more than %g degrees", a->turn_nodes, TRACE_TURN_MAX_DEGREES);
    else if (a->radius_nodes > 0)
        snprintf(a->road_text, sizeof a->road_text, "%d node(s) on a curve tighter than %g units", a->radius_nodes, TRACE_RADIUS_MIN);
}

int app_node_count(const App *a)
{
    return a->road_made ? a->node_count : 4 * track_trace_segments(a->length) + 1;
}

int app_save(App *a)
{
    char why[APP_MSG];
    if (!project_save_trace(a->dir, &a->trace, why, sizeof why)) {
        set_status(a, why);
        return 0;
    }
    set_status(a, "saved trace.json");
    return 1;
}

int app_build(App *a)
{
    char why[APP_MSG], zip[APP_PATH];
    if (!build_project(a->dir, &a->project, &a->trace, zip, sizeof zip, why, sizeof why)) {
        snprintf(a->status, sizeof a->status, "build refused: %s", why);
        return 0;
    }
    snprintf(a->zip, sizeof a->zip, "%s", zip);
    snprintf(a->status, sizeof a->status, "built %s", strrchr(zip, '/') != NULL ? strrchr(zip, '/') + 1 : zip);
    return 1;
}

int app_install(App *a)
{
    if (a->zip[0] == '\0') {
        set_status(a, "build first (B): install takes the built package");
        return 0;
    }
    if (a->user[0] == '\0') {
        set_status(a, "install needs the player folder: start the editor with --user DIR");
        return 0;
    }
    char why[APP_MSG];
    if (!install_zip(a->game, a->zip, a->user, why, sizeof why)) {
        snprintf(a->status, sizeof a->status, "install failed: %s", why);
        return 0;
    }
    snprintf(a->status, sizeof a->status, "installed into %s", a->user);
    return 1;
}

/* a copy of the trace's points with `point` put at `index` (0..count: index == count appends) */
static TracePoint *points_with(const App *a, int index, TracePoint point)
{
    TracePoint *more = malloc(sizeof *more * (size_t)(a->trace.count + 1));
    if (more == NULL) return NULL;
    memcpy(more, a->trace.points, sizeof *more * (size_t)index);
    more[index] = point;
    memcpy(more + index + 1, a->trace.points + index, sizeof *more * (size_t)(a->trace.count - index));
    return more;
}

static void put_points(App *a, TracePoint *points, int count, int selected)
{
    free(a->trace.points);
    a->trace.points = points;
    a->trace.count = count;
    a->selected = selected;
    a->dirty = 1;
}

/* the point after the last one, along the last segment */
static TracePoint extension(const App *a)
{
    const TracePoint *last = &a->trace.points[a->trace.count - 1], *prev = &a->trace.points[a->trace.count - 2];
    const double dx = last->x - prev->x, dz = last->z - prev->z, len = hypot(dx, dz);
    TracePoint p = *last;
    if (len > 0) {
        p.x += dx / len * APP_NEW_STEP;
        p.z += dz / len * APP_NEW_STEP;
    } else {
        p.z += APP_NEW_STEP;
    }
    return p;
}

static int can_add(App *a)
{
    if (a->trace.count < PROJECT_POINTS_MAX) return 1;
    set_status(a, "the trace has the most points it can hold");
    return 0;
}

void app_add_point(App *a)
{
    if (!can_add(a)) return;
    TracePoint *more = points_with(a, a->trace.count, extension(a));
    if (more == NULL) {
        set_status(a, "out of memory");
        return;
    }
    put_points(a, more, a->trace.count + 1, a->trace.count);
    set_status(a, "point added at the end");
}

void app_insert_point(App *a)
{
    if (a->selected < 0) {
        set_status(a, "select a point first (click it)");
        return;
    }
    if (a->selected == a->trace.count - 1) {
        app_add_point(a);
        return;
    }
    if (!can_add(a)) return;
    const TracePoint *p = &a->trace.points[a->selected], *q = &a->trace.points[a->selected + 1];
    const TracePoint mid = { (p->x + q->x) / 2, (p->z + q->z) / 2 };
    TracePoint *more = points_with(a, a->selected + 1, mid);
    if (more == NULL) {
        set_status(a, "out of memory");
        return;
    }
    put_points(a, more, a->trace.count + 1, a->selected + 1);
    set_status(a, "point inserted after the selected one");
}

void app_remove_point(App *a)
{
    if (a->selected < 0) {
        set_status(a, "select a point first (click it)");
        return;
    }
    if (a->trace.count <= 2) {
        set_status(a, "a trace keeps 2 points");
        return;
    }
    TracePoint *fewer = malloc(sizeof *fewer * (size_t)(a->trace.count - 1));
    if (fewer == NULL) {
        set_status(a, "out of memory");
        return;
    }
    memcpy(fewer, a->trace.points, sizeof *fewer * (size_t)a->selected);
    memcpy(fewer + a->selected, a->trace.points + a->selected + 1, sizeof *fewer * (size_t)(a->trace.count - a->selected - 1));
    const int next = a->selected < a->trace.count - 1 ? a->selected : a->trace.count - 2;
    put_points(a, fewer, a->trace.count - 1, next);
    set_status(a, "point removed");
}
