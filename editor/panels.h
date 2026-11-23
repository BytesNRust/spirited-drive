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
void road_extend_menu(App *a, int point, unsigned mask);   // in a right-click menu of a set value: «Extend to next change» and «Extend to the end» (key_ops.h keys_extend)
void road_reach_hover(App *a, int point, unsigned mask);   // right after an item of the value `mask` on `point`: while it is hovered the Viewport shows how far the value reaches
void road_key_panel(App *a);   // the selected point's road key, each side (Left | Right): the Road profile window's content
void road_key_window(App *a);   // the Road profile window (PANEL_ROAD_KEY), beside Limits and Problems
void strip_panel(App *a);   // the Distance strip window (strip_panel.cpp): each road value's stretches along the road, a tab beside Limits
void limits_panel(App *a);
void problems_panel(App *a);
void node_panel(App *a);
void project_panel(App *a);
void track_panel(App *a);
void drive_panel(App *a);   // Test drive of the open track: the car, the buttons, a state line, the look
ImGuiID panels_limits_dock(void);   // the dock node Limits is in (0 before it is drawn): the windows added since dock there as tabs
void race_look_controls(RaceLook *look);   // the race look's switches and resolution, under a folded "Look" header (race_look_ui.cpp): the Race and Drive windows'

// the Track light window (light_panel.cpp): the file's controls and rows, and the photo (a texture of the job's picture, 0 for none)
void light_panel(App *a);
// the rows of the Track light window by stretch (light_tone_panel.cpp), called by light_panel
void light_track_rows(App *a);   // the Track light window's lists for the time shown: the sun, the tunnels, the lamps and the retouches (light_track_panel.cpp)
// the comparison beside the photo (light_compare_panel.cpp): Off, Before (the files on disk) or the four times of day; the textures are the pictures of the four jobs, 0 for none
extern const char *const PANEL_LIGHT_COMPARE;
void light_compare_panel(App *a, const ImTextureID photos[LIGHT_COMPARE_SLOTS]);
void light_photo_panel(App *a, ImTextureID photo);
// an original track's masks (track_masks_panel.cpp): the rows and the picked mask over its texture (a texture of a->tmask_rgba, 0 for none)
extern const char *const PANEL_TRACK_MASKS;
void track_masks_panel(App *a, ImTextureID picture);
void light_shortcuts(App *a);   // Cmd+S saves the light file

// the start screen (nothing open) and the open car's panels: its card, its textures, its masks
void start_screen(App *a);
void car_panel(App *a);
void car_shortcuts(App *a);   // Cmd+S, Cmd+Z, Cmd+Shift+Z / Cmd+Y on a Mac; Ctrl elsewhere (command_keys.h)
void masks_panel(App *a, ImTextureID picture);   // Masks: the changes, Convert PSD, a row for each texture with its mask's state, and the picked mask over its texture (a texture of mask_edit_compose, 0 for none)
void race_panel(App *a, ImTextureID photo, ImTextureID sheet);   // Test in race and Race photo, the picture (a texture, 0 for none) and the game's log
void photo_view_panel(App *a, ImTextureID photo);   // the photo in a large window of its own (a->photo_view_open)
void model_panel(App *a, SDL_Renderer *ren);   // the open car in 3D: drag to turn, wheel to zoom, F S R T Home
void model_panel_preset(double yaw, double pitch, int dist);
void model_panel_nudge(void);   // the camera moved as a hand would (a drag and two wheel notches): the step camera:1
int model_panel_save_picture(const char *path);   // the picture as drawn (the game's frame at the render scale, not the screen's copy of it) as a PNG, and "model picture: W x H, N drawn" on stdout; 0 when there is none: --model-shot
void model_panel_state(int *big, int *docked, int *scale, int *soft);   // the Model window floating enlarged, docked when last drawn, its render scale and its Soft edges switch: the step camera:3
void model_panel_camera(int *yaw, int *pitch, int *dist);   // where the camera is now (1/16 degrees, model units): the step camera:0
int model_panel_backdrop(App *a, int kind, int r, int g, int b, int bottom);   // the card photo's backdrop (the step card-backdrop): kind 0 plain, 1 gradient, 2 picture; the colour is the top one, or the bottom one when `bottom`
const char *model_panel_backdrop_path();   // the backdrop's picture now ("" for none)
void model_panel_backdrop_file(const char *path);   // a picture as the backdrop (--card-backdrop)
int model_panel_card_view(App *a, bool card, bool as_card);   // the Card view and As on the card boxes set (the step card-view): 1 when the car is open
int model_panel_card_photo(App *a);   // «Use as card photo» as its button does it, without the question about a card.png that is there (the step card-photo, through ui_card_photo): 1 when it was made
void model_panel_big(bool on);   // the Model window floating large (Enlarge) or docked again
void model_panel_render_scale(int scale);   // the game's render scale 1..3 for this window (the car drawn on a frame 480 * scale lines tall, shown stretched)
void model_panel_soft(bool on);   // the shine's soft mask edges on or off (the player's shine_soft by default)
void model_panel_shine(bool on);   // the body shine on or off (on by default)   // an orbit for the next car opened (degrees, degrees, model units; dist 0 keeps the home distance)
void mask_view_section(App *a, ImTextureID picture);   // in Masks: the list of masks with their state and the open mask over its texture
void preview_panel(App *a, struct SDL_Renderer *ren);   // Preview: the textures as the race paints them, in a paint of the card's
void preview_free(void);
void masks_changes_section(App *a);   // in Masks: the project's masks against the game's, a button copies the list
void export_popup(App *a);    // the Export original car window, up while a->export_dialog is set

// the panels' names, as the dock layout places them
extern const char *const PANEL_POINTS, *const PANEL_LIMITS, *const PANEL_PROBLEMS, *const PANEL_NODE, *const PANEL_PROJECT, *const PANEL_TRACK, *const PANEL_ROAD_KEY;
extern const char *const PANEL_MODEL, *const PANEL_DRIVE, *const PANEL_LIGHT, *const PANEL_LIGHT_PHOTO, *const PANEL_STRIP;
extern const char *const PANEL_CAR, *const PANEL_MASKS, *const PANEL_RACE, *const PANEL_PREVIEW;

#endif
