// The editor's side panels (panels.cpp): the control points as a table, the limits' summary, the list of the nodes that break one, the project and its Save / Build /
// Install buttons. Each changes the App through app.h and interact.h, like the shortcuts do.
#ifndef TNFS_TRACK_EDITOR_PANELS_H
#define TNFS_TRACK_EDITOR_PANELS_H

#include "imgui.h"

#include <SDL3/SDL.h>

extern "C" {
#include "app.h"
}

void points_panel(App *a);
float points_panel_width();   // the Points panel's width that shows all its columns (the first-run layout gives it that much when it can)
float road_key_panel_width();   // the Road profile window's width that shows its labels and both sides' combos whole
void road_key_panel(App *a);   // the selected point's road key, each side (Left | Right): the Road profile window's content
void road_key_window(App *a);   // the Road profile window (PANEL_ROAD_KEY), beside Limits and Problems
void limits_panel(App *a);
void problems_panel(App *a);
void node_panel(App *a);
void project_panel(App *a);
void track_panel(App *a);

// the start screen (nothing open) and the open car's panels: its card, its textures, its masks
void start_screen(App *a);
void car_panel(App *a);
void rehacer_panel(App *a);   // Rehacer's status and log
void car_shortcuts(App *a);   // Cmd+S, Cmd+Z, Cmd+Shift+Z / Cmd+Y on a Mac; Ctrl elsewhere (ImGuiMod_Ctrl is Cmd on a Mac: shortcut_keys.h)
void textures_panel(App *a);
void masks_panel(App *a);
void race_panel(App *a, ImTextureID photo);   // Test in race and Race photo, the picture (a texture, 0 for none) and the game's log
void model_panel(App *a, SDL_Renderer *ren);   // the open car in 3D: drag to turn, wheel to zoom, F S R T Home
void model_panel_preset(double yaw, double pitch, int dist);
void model_panel_shine(bool on);   // the body shine on or off (on by default)   // an orbit for the next car opened (degrees, degrees, model units; dist 0 keeps the home distance)
void mask_edit_panel(App *a, ImTextureID picture);   // Edit mask: the mask over its texture (a texture of mask_edit_compose, 0 for none), the legend, the tools
void preview_panel(App *a, struct SDL_Renderer *ren);   // Preview: the textures as the race paints them, in a paint of the card's
void preview_free(void);
void masks_changes_section(App *a);   // in Masks: the project's masks against the game's, a button copies the list
void export_popup(App *a);    // the Export original car window, up while a->export_dialog is set

// the panels' names, as the dock layout places them
extern const char *const PANEL_POINTS, *const PANEL_LIMITS, *const PANEL_PROBLEMS, *const PANEL_NODE, *const PANEL_PROJECT, *const PANEL_TRACK, *const PANEL_ROAD_KEY;
extern const char *const PANEL_MODEL;
extern const char *const PANEL_CAR, *const PANEL_TEXTURES, *const PANEL_MASKS, *const PANEL_REHACER, *const PANEL_RACE, *const PANEL_MASK_EDIT, *const PANEL_PREVIEW;

#endif
