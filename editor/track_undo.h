/* Undo and redo of a track project (owner 2026-11-20: undo, redo and unsaved changes in both editors): whole-project snapshots, the
 * trace (its points and road keys, every field of them) and project.ini's fields. A step is taken when the project differs from the
 * last one taken (track_undo_commit, called when no field is being typed in and no point dragged: one step a drag, not one a frame).
 * The stacks hold TRACK_UNDO_MAX steps; the oldest falls off. The state as saved is kept too: the project is modified when it is not
 * that, so undoing back to it is not a change to save. Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_TRACK_UNDO_H
#define TNFS_TRACK_EDITOR_TRACK_UNDO_H

#include "project.h"

enum { TRACK_UNDO_MAX = 64 };

typedef struct {
    Project project;
    Trace trace;   /* its own copies of the points and keys */
} TrackSnap;

typedef struct {
    TrackSnap *undo, *redo;   /* malloc'd, TRACK_UNDO_MAX each */
    int undo_count, redo_count;
    TrackSnap now;            /* the last step taken: what the project was when it was last committed */
    TrackSnap saved;          /* the project as its files have it */
    int open;
} TrackUndo;

/* A project just opened (or saved as new): no steps, `project` and `trace` as saved. 1, or 0 when memory is out. */
int track_undo_open(TrackUndo *u, const Project *project, const Trace *trace);
void track_undo_free(TrackUndo *u);

/* A step when the project is not what was last taken: that goes on the undo stack and the redo stack is emptied. 1 for a step. */
int track_undo_commit(TrackUndo *u, const Project *project, const Trace *trace);

/* The project as it was a step back (forward): `project` and `trace` are set to it (the trace's arrays made again). A change not yet
 * taken is taken first, so it is what an undo takes back. 1 when there was a step. */
int track_undo_undo(TrackUndo *u, Project *project, Trace *trace);
int track_undo_redo(TrackUndo *u, Project *project, Trace *trace);
int track_undo_can_undo(const TrackUndo *u, const Project *project, const Trace *trace);
int track_undo_can_redo(const TrackUndo *u);

/* The files now have `project` and `trace` (a Save). */
int track_undo_saved(TrackUndo *u, const Project *project, const Trace *trace);
/* 1 when the project is not what its files have. */
int track_undo_modified(const TrackUndo *u, const Project *project, const Trace *trace);

#endif
