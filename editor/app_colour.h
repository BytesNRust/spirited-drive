/* Colour by (dep95): the Viewport's mode, each node's class in it (formats/track_trace_stretch.h) and the double-click that selects a stretch. */
#ifndef TNFS_TRACK_EDITOR_APP_COLOUR_H
#define TNFS_TRACK_EDITOR_APP_COLOUR_H

#include "app.h"
#include "formats/track_trace_stretch.h"

/* the mode a window asks for (out of range: Limits) */
void app_set_colour_by(App *a, int mode);

/* each node's class in a->colour_by, made when asked and kept until the road is made again; NULL in Limits mode or with no road */
const uint8_t *app_colour_classes(App *a);

/* the stretch of the road that holds `node` in the current mode, its points selected (the first to the last whose node is in it; a stretch with none
 * selects the point nearest it); 1 when it did, 0 in Limits mode, with no road or off the road. Says which points in the status line. With `add` (Shift or Cmd
 * held on the double-click) the points are added to the selection, which stays; else they are the selection. */
int app_select_stretch(App *a, int node, int add);

/* the run-length summary --steps print-classes prints: "classes curve: straight 0-39, right 40-79 ..." */
void app_colour_summary(App *a, char *out, size_t cap);

#endif
