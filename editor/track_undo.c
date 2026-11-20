#include "track_undo.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* A Trace is copied field by field below, its two arrays made again: a field added to it must be copied and compared here too (the points
 * and keys themselves are copied and compared byte for byte, so what is added to TracePoint or TraceKey needs nothing here). */
#if UINTPTR_MAX == 0xffffffffffffffffu
typedef char trace_fields_check[sizeof(Trace) == 40 ? 1 : -1];   /* points, count, width, keys, key_count, key_cap: a new field fails here */
#endif

static void snap_free(TrackSnap *s)
{
    free(s->trace.points), free(s->trace.keys);
    memset(s, 0, sizeof *s);
}

/* `to` made a copy of `project` and `trace` (its arrays its own, the keys' room just what they need); 0 when memory is out, `to` emptied */
static int snap_take(TrackSnap *to, const Project *project, const Trace *trace)
{
    snap_free(to);
    to->project = *project;
    to->trace.count = trace->count;
    to->trace.width = trace->width;
    to->trace.key_count = to->trace.key_cap = trace->key_count;
    const size_t points = sizeof *trace->points * (size_t)(trace->count > 0 ? trace->count : 0);
    const size_t keys = sizeof *trace->keys * (size_t)(trace->key_count > 0 ? trace->key_count : 0);
    to->trace.points = points > 0 ? malloc(points) : NULL;
    to->trace.keys = keys > 0 ? malloc(keys) : NULL;
    if ((points > 0 && to->trace.points == NULL) || (keys > 0 && to->trace.keys == NULL)) {
        snap_free(to);
        return 0;
    }
    if (points > 0) memcpy(to->trace.points, trace->points, points);
    if (keys > 0) memcpy(to->trace.keys, trace->keys, keys);
    return 1;
}

static int project_same(const Project *a, const Project *b)
{
    return strcmp(a->id, b->id) == 0 && strcmp(a->name, b->name) == 0 && strcmp(a->author, b->author) == 0 && strcmp(a->stem, b->stem) == 0
           && strcmp(a->art, b->art) == 0 && a->redistributable == b->redistributable && strcmp(a->edge, b->edge) == 0;
}

static int snap_is(const TrackSnap *s, const Project *project, const Trace *trace)
{
    const Trace *t = &s->trace;
    if (!project_same(&s->project, project) || t->count != trace->count || t->key_count != trace->key_count) return 0;
    if (memcmp(&t->width, &trace->width, sizeof t->width) != 0) return 0;
    if (t->count > 0 && memcmp(t->points, trace->points, sizeof *t->points * (size_t)t->count) != 0) return 0;
    return t->key_count == 0 || memcmp(t->keys, trace->keys, sizeof *t->keys * (size_t)t->key_count) == 0;
}

/* `project` and `trace` set to `s` (the trace's arrays made again); 0 when memory is out, nothing changed */
static int snap_put(const TrackSnap *s, Project *project, Trace *trace)
{
    TrackSnap copy = { 0 };
    if (!snap_take(&copy, &s->project, &s->trace)) return 0;
    free(trace->points), free(trace->keys);
    *project = copy.project;
    *trace = copy.trace;
    return 1;
}

int track_undo_open(TrackUndo *u, const Project *project, const Trace *trace)
{
    memset(u, 0, sizeof *u);
    u->undo = calloc(TRACK_UNDO_MAX, sizeof *u->undo);
    u->redo = calloc(TRACK_UNDO_MAX, sizeof *u->redo);
    if (u->undo == NULL || u->redo == NULL || !snap_take(&u->now, project, trace) || !snap_take(&u->saved, project, trace)) {
        track_undo_free(u);
        return 0;
    }
    u->open = 1;
    return 1;
}

void track_undo_free(TrackUndo *u)
{
    for (int i = 0; i < u->undo_count; i++) snap_free(&u->undo[i]);
    for (int i = 0; i < u->redo_count; i++) snap_free(&u->redo[i]);
    free(u->undo), free(u->redo);
    snap_free(&u->now), snap_free(&u->saved);
    memset(u, 0, sizeof *u);
}

static void redo_clear(TrackUndo *u)
{
    for (int i = 0; i < u->redo_count; i++) snap_free(&u->redo[i]);
    u->redo_count = 0;
}

/* `s` moved onto the top of `stack` (the oldest falls off a full one); `s` is left empty */
static void push(TrackSnap *stack, int *count, TrackSnap *s)
{
    if (*count == TRACK_UNDO_MAX) {
        snap_free(&stack[0]);
        memmove(&stack[0], &stack[1], sizeof *stack * (TRACK_UNDO_MAX - 1));
        memset(&stack[TRACK_UNDO_MAX - 1], 0, sizeof *stack);
        --*count;
    }
    stack[(*count)++] = *s;
    memset(s, 0, sizeof *s);
}

int track_undo_commit(TrackUndo *u, const Project *project, const Trace *trace)
{
    if (!u->open || snap_is(&u->now, project, trace)) return 0;
    TrackSnap next = { 0 };
    if (!snap_take(&next, project, trace)) return 0;
    push(u->undo, &u->undo_count, &u->now);
    u->now = next;
    redo_clear(u);
    return 1;
}

/* the top of `from` becomes the project, the state now goes on `to` */
static int step(TrackUndo *u, TrackSnap *from, int *from_count, TrackSnap *to, int *to_count, Project *project, Trace *trace)
{
    if (*from_count == 0 || !snap_put(&from[*from_count - 1], project, trace)) return 0;
    push(to, to_count, &u->now);
    u->now = from[--*from_count];
    memset(&from[*from_count], 0, sizeof *from);
    return 1;
}

int track_undo_undo(TrackUndo *u, Project *project, Trace *trace)
{
    if (!u->open) return 0;
    track_undo_commit(u, project, trace);   /* a change not yet taken is what this takes back */
    return step(u, u->undo, &u->undo_count, u->redo, &u->redo_count, project, trace);
}

int track_undo_redo(TrackUndo *u, Project *project, Trace *trace)
{
    if (!u->open || !snap_is(&u->now, project, trace)) return 0;   /* a change not yet taken: it is the newest, nothing to redo over it */
    return step(u, u->redo, &u->redo_count, u->undo, &u->undo_count, project, trace);
}

int track_undo_can_undo(const TrackUndo *u, const Project *project, const Trace *trace)
{
    return u->open && (u->undo_count > 0 || !snap_is(&u->now, project, trace));
}

int track_undo_can_redo(const TrackUndo *u)
{
    return u->open && u->redo_count > 0;
}

int track_undo_saved(TrackUndo *u, const Project *project, const Trace *trace)
{
    return u->open && snap_take(&u->saved, project, trace);
}

int track_undo_modified(const TrackUndo *u, const Project *project, const Trace *trace)
{
    return u->open && !snap_is(&u->saved, project, trace);
}
