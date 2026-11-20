// The editor's side panels (panels.cpp): the control points as a table, the limits' summary, the list of the nodes that break one, the project and its Save / Build /
// Install buttons. Each changes the App through app.h and interact.h, like the shortcuts do.
#ifndef TNFS_TRACK_EDITOR_PANELS_H
#define TNFS_TRACK_EDITOR_PANELS_H

#include "imgui.h"

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
void race_panel(App *a, ImTextureID photo);   // Test in race and Race photo, the picture (a texture, 0 for none) and the game's log
void mask_edit_panel(App *a, ImTextureID picture);   // Edit mask: the mask over its texture (a texture of mask_edit_compose, 0 for none), the legend, the tools
void preview_panel(App *a, struct SDL_Renderer *ren);   // Preview: the textures as the race paints them, in a paint of the card's
void preview_free(void);
void export_popup(App *a);    // the Export original car window, up while a->export_dialog is set

// the panels' names, as the dock layout places them
extern const char *const PANEL_POINTS, *const PANEL_LIMITS, *const PANEL_PROBLEMS, *const PANEL_NODE, *const PANEL_PROJECT, *const PANEL_TRACK;
extern const char *const PANEL_CAR, *const PANEL_TEXTURES, *const PANEL_MASKS, *const PANEL_REHACER, *const PANEL_RACE, *const PANEL_MASK_EDIT, *const PANEL_PREVIEW;

#endif
