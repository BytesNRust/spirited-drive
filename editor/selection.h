/* The control points selected in the track editor (dep87): a click selects one, Shift+click a range from the anchor, Cmd+click adds or
 * takes away one, a box drag the points inside it. The set follows the points when they come and go; the group moves and rotates together;
 * removing it keeps at least TRACE_POINTS_MIN points. Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_SELECTION_H
#define TNFS_TRACK_EDITOR_SELECTION_H

#include "formats/track_trace.h"

#include <stddef.h>

typedef enum { SEL_ONE, SEL_RANGE, SEL_TOGGLE } SelClick;   /* a plain click, Shift, Cmd */

typedef struct {
    unsigned char *on;   /* on[i]: point i is selected (malloc'd, `points` entries) */
    int points;          /* the points it covers */
    int anchor;          /* where Shift's range starts: the last plain or Cmd click; -1 for none */
} Selection;

void sel_init(Selection *s);
void sel_free(Selection *s);
/* covers `points` points: the ones past the end leave the set, new ones are not in it. 0 when memory is out */
int sel_resize(Selection *s, int points);
int sel_points(const Selection *s);

void sel_clear(Selection *s);
/* a click on point i (nothing when i is not a point) */
void sel_click(Selection *s, int i, SelClick how);
/* the points inside the box with corners (x0, z0) and (x1, z1), any order; `add`: added to the set (Shift), else the set is them */
void sel_box(Selection *s, const TracePoint *p, int n, double x0, double z0, double x1, double z1, int add);

int sel_has(const Selection *s, int i);
int sel_count(const Selection *s);
int sel_first(const Selection *s);   /* the lowest selected, -1 for none */
int sel_last(const Selection *s);
/* "3-7", "1, 3, 9, 14", "" for none; cut to `cap` ending in "..." */
void sel_describe(const Selection *s, char *out, size_t cap);

/* a point put in at index `at` (the ones from `at` on move up one), or the point at `at` taken out */
void sel_inserted(Selection *s, int at);
void sel_removed(Selection *s, int at);

/* the selected points of p[0..n) moved by (dx, dz) */
void sel_move(const Selection *s, TracePoint *p, int n, double dx, double dz);
/* the selected points set to `from`'s rotated by `degrees` about the first selected one, clockwise as the view shows them (z up): a live
 * field rotates from where it began each frame, never by small steps on top of each other */
void sel_rotate(const Selection *s, TracePoint *p, int n, const TracePoint *from, double degrees);

/* whether the selection can be removed: something selected and at least TRACE_POINTS_MIN points left; else 0 with the reason in `why` */
int sel_can_remove(const Selection *s, char *why, size_t cap);

#endif
