/* The track editor's group operations on the selected run of control points (dep87 1b): Copy and Paste (Cmd+C, Cmd+V) with the points' profile
 * keys, Mirror left/right and front/back, Reverse. Each is one step to undo; each wants a run without gaps (two points or more) and says why in the
 * status line when it refuses. The geometry is point_ops.h's, the keys key_ops.h's. */
#ifndef TNFS_TRACK_EDITOR_APP_POINTS_H
#define TNFS_TRACK_EDITOR_APP_POINTS_H

#include "app.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the selected points as a run s..e: 1 when they are two or more with no gap, else 0 with the reason (for `what`, "copy") in the status line */
int app_points_run(App *a, const char *what, int *s, int *e);
/* the run copied, with its keys, into the editor's clipboard (kept until another copy, or another project is opened) */
int app_copy_points(App *a);
/* Copy, then the run removed (one step to undo); nothing is removed when the copy is refused or the road would be left too short (said in the status) */
int app_cut_points(App *a);
/* every point of the road selected */
int app_select_all_points(App *a);
/* the clipboard's points put in after the last selected point (the road's last with none selected): its first point on that point, the block going
 * on along the road's heading there, the points after it following; the pasted run (that point included) becomes the selection */
int app_paste_points(App *a);
/* the run reflected about its first point's heading (`square` 0: left/right) or the line square to it (1: front/back), sides swapped */
int app_mirror_points(App *a, int square);
/* the run travelled the other way (the whole road: driven the other way, nothing moves) */
int app_reverse_points(App *a);

/* Add curve / Add straight (dep88) with App.curve's fields, after the last selected point (the road's last with none selected), going on along
 * the road's heading there; the points after it follow. The new points and that point become the selection; one step to undo. The block takes the
 * profile the road has at that point; the road after it keeps its own. */
void app_curve_defaults(App *a);
int app_add_curve(App *a, int straight);
/* the road as Add curve (`straight` 0) or Add straight would make it, a new array of *count points (NULL when the fields are out of range), and
 * where the block starts in it (*from) and ends (*to) */
TracePoint *app_curve_preview(App *a, int straight, int *count, int *from, int *to);
/* how the curve's radius stands against Build's limits: 2 red (not built), 1 amber, 0 fine (a straight: 0) */
int app_curve_level(const App *a);
/* the fields' readout: "94 m, 6 points after point 7", with "; radius 15 m (under 20.9 m): not built" (Build's words) under a limit */
void app_curve_text(App *a, int straight, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
