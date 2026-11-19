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
void node_panel(App *a);
void project_panel(App *a);
void track_panel(App *a);

// the start screen (nothing open) and the open car's panels: its card, its textures, its masks
void start_screen(App *a);
void car_panel(App *a);
void rehacer_panel(App *a);   // Rehacer's status and log
void car_shortcuts(App *a);   // Ctrl+S, Ctrl+Z, Ctrl+Shift+Z / Ctrl+Y (Cmd on a Mac)
void textures_panel(App *a);
void masks_panel(App *a);
void export_popup(App *a);    // the Export original car window, up while a->export_dialog is set

// the panels' names, as the dock layout places them
extern const char *const PANEL_POINTS, *const PANEL_LIMITS, *const PANEL_PROBLEMS, *const PANEL_NODE, *const PANEL_PROJECT, *const PANEL_TRACK;
extern const char *const PANEL_CAR, *const PANEL_TEXTURES, *const PANEL_MASKS, *const PANEL_REHACER;

#endif
