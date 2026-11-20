/* What the editor does to its view and its selected point: the verbs the window's panels and shortcuts call (the state is app.h's). */
#ifndef TNFS_TRACK_EDITOR_INTERACT_H
#define TNFS_TRACK_EDITOR_INTERACT_H

#include "app.h"

#define ROTATE_KEY_STEP 1.0         /* degrees [ and ] rotate the selected points */
#define ROTATE_KEY_STEP_SHIFT 5.0   /* with Shift */

enum { INTERACT_NUDGE = 1,    /* units an arrow key moves the selected points */
       INTERACT_PICK_REACH = 12 };   /* pixels a click may be from a point and still take it */

/* the selected points moved by (dx, dz) units; nothing when none is selected */
void interact_nudge(App *a, double dx, double dz);

/* zooms by `factor` keeping the world point under the view's (sx, sy) where it is */
void interact_zoom(App *a, double sx, double sy, double factor);

/* the view that shows every point of the trace in the view's size */
void interact_fit(App *a);

#endif
