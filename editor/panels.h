// The editor's side panels (panels.cpp): the control points as a table, the limits' summary, the list of the nodes that break one, the project and its Save / Build /
// Install buttons. Each changes the App through app.h and interact.h, like the shortcuts do.
#ifndef TNFS_TRACK_EDITOR_PANELS_H
#define TNFS_TRACK_EDITOR_PANELS_H

extern "C" {
#include "app.h"
}

void points_panel(App *a);
void limits_panel(App *a);
void problems_panel(App *a);
void project_panel(App *a);

// the panels' names, as the dock layout places them
extern const char *const PANEL_POINTS, *const PANEL_LIMITS, *const PANEL_PROBLEMS, *const PANEL_PROJECT;

#endif
