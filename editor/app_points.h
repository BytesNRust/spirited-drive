/* The track editor's group operations on the selected run of control points (dep87 1b): Copy and Paste (Cmd+C, Cmd+V) with the points' profile
 * keys, Mirror left/right and front/back, Reverse. Each is one step to undo; each wants a run without gaps (two points or more) and says why in the
 * status line when it refuses. The geometry is point_ops.h's, the keys key_ops.h's. */
#ifndef TNFS_TRACK_EDITOR_APP_POINTS_H
#define TNFS_TRACK_EDITOR_APP_POINTS_H

#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

/* the selected points as a run s..e: 1 when they are two or more with no gap, else 0 with the reason (for `what`, "copy") in the status line */
int app_points_run(App *a, const char *what, int *s, int *e);
/* the run copied, with its keys, into the editor's clipboard (kept until another copy, or another project is opened) */
int app_copy_points(App *a);
/* the clipboard's points put in after the last selected point (the road's last with none selected): its first point on that point, the block going
 * on along the road's heading there, the points after it following; the pasted run (that point included) becomes the selection */
int app_paste_points(App *a);
/* the run reflected about its first point's heading (`square` 0: left/right) or the line square to it (1: front/back), sides swapped */
int app_mirror_points(App *a, int square);
/* the run travelled the other way (the whole road: driven the other way, nothing moves) */
int app_reverse_points(App *a);

#ifdef __cplusplus
}
#endif

#endif
