// The Points panel's Slope block (dep95 step 10, app_slope.h): grade or rise, the easings, keeping the road after it, Accept, and the line that says what Accept would make.
// While its fields are hovered or edited the Distance strip's profile draws the result (App.slope.preview).
#ifndef TNFS_TRACK_EDITOR_SLOPE_PANEL_H
#define TNFS_TRACK_EDITOR_SLOPE_PANEL_H

extern "C" {
#include "app.h"
}

// the folding block, closed to begin with; for the selected run of points
void slope_rows(App *a);

#endif
