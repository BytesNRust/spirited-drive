/* The track editor's selected control points as the window uses them (dep87; the set is selection.h's, kept in App.sel). App.selected stays
 * the point the Node window and the Points table follow: the last one clicked, always in the set when there is one. Code that sets
 * App.selected alone (a pick in Problems, a new point) makes the set that point alone at the next app_sel_sync. */
#ifndef TNFS_TRACK_EDITOR_APP_SELECT_H
#define TNFS_TRACK_EDITOR_APP_SELECT_H

#include "app.h"

/* every frame: the set covers the trace's points and agrees with App.selected */
void app_sel_sync(App *a);
/* a click on point i (Shift: SEL_RANGE, Cmd: SEL_TOGGLE); i -1, a click on no point: nothing selected (a plain click), or the set kept */
void app_sel_click(App *a, int i, SelClick how);
/* the points in the box with corners (x0, z0) and (x1, z1), track units; `add`: added to the set (Shift) */
void app_sel_box(App *a, double x0, double z0, double x1, double z1, int add);
void app_sel_clear(App *a);
int app_sel_count(const App *a);

/* the selected points moved by (dx, dz) units together */
void app_sel_move(App *a, double dx, double dz);
/* the Rotate points field: the selected points rotated `degrees` from where they were when the field was taken (live while it is
 * dragged); app_sel_rotate_end when it is let go. app_sel_rotate_by: a key's step, from where they are. */
void app_sel_rotate_live(App *a, double degrees);
void app_sel_rotate_end(App *a);
void app_sel_rotate_by(App *a, double degrees);

/* the Road profile of several selected points (read only until dep87 part 2): row 0 the shoulder surface, row 1 + TRACE_SIDE_* a side
 * value; `side` TRACK_GEN_LEFT or _RIGHT. The text every selected point has ("Asphalt", "3.00", "Hard", "On"), or APP_SEL_MIXED; 1 when
 * the points differ, 0 when they share it, -1 when nothing is selected or memory is out */
#define APP_SEL_MIXED "Mixed"
enum { APP_SEL_PROFILE_ROWS = 1 + TRACE_SIDE_FIELDS };
int app_sel_profile(const App *a, int row, int side, char *out, size_t cap);

/* dep87 part 2: which road values differ between the selected points, as TRACE_KEY_ bits (a side's value its own bit); APP_SEL_MIXED_SURFACE(side) for
 * a side's shoulder surface (the surface bit covers both sides). 0 with fewer than two points selected. The Road profile shows «\xe2\x80\x94» for those. */
#define APP_SEL_MIXED_SURFACE(side) (1u << (30 + (side)))
unsigned app_sel_mixed(const App *a);

/* the selected points removed, their road keys with them; refused (in the status) when fewer than TRACE_POINTS_MIN would remain */
void app_remove_selection(App *a);

#endif
