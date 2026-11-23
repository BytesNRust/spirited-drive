/* The editor's interface layer, Dear ImGui over the SDL renderer (third_party/imgui, docking branch). The only C++ of the editor: the rest
 * (app, view, project, build, interact, capture, main) is C and does not know ImGui. This is the layer's C face. */
#ifndef TNFS_TRACK_EDITOR_UI_H
#define TNFS_TRACK_EDITOR_UI_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "app.h"

/* Starts ImGui on the window and its renderer, with docking on. `ini` is where the layout is kept (NULL: not kept). 1, or 0 with `why`. */
/* a --steps run (slow under a sanitizer): two clicks are a double-click however far apart in time, as the steps send them frame by frame */
void ui_double_click_by_frames(void);
/* the Model window's «Use as card photo» (the step card-photo), without the question about a card.png that is there: 1 when card.png was made */
int ui_card_photo(App *a);
/* the Model's camera (the step camera): moved as a hand would, and where it is (1/16 degrees, model units) */
void ui_camera_nudge(void);
void ui_camera(int *yaw, int *pitch, int *dist);
/* the Model window's Card view and As on the card boxes (the step card-view): 1 when the car is open to show them */
int ui_card_view(App *a, int card, int as_card);
/* the card photo's backdrop (the steps card-backdrop:KIND,R,G,B and card-bd-bottom:R,G,B, and --card-backdrop FILE): 1 when the car is open to show it */
int ui_card_backdrop(App *a, int kind, int r, int g, int b, int bottom);
void ui_card_backdrop_file(const char *path);
/* the picture chosen as the backdrop now ("" for none): where the file dialog starts */
const char *ui_card_backdrop_path(void);
int ui_open(SDL_Window *win, SDL_Renderer *ren, const char *ini, char *why, size_t why_cap);
void ui_close(void);
/* the editor's settings the ini had (ui_open reads it), into `a`: the car Test drive gives the player. Before the first frame, so a headless run has them too. */
void ui_settings_to(App *a);

/* The window's size by default (window_fit's on a normal run, 960 x 600 headless): Window > Reset layout gives it back. */
void ui_window_default(int w, int h);

/* Window > Reset layout (Cmd+Shift+L): the window goes back to its default size, centred, and a few frames later (once the new size is in) the current
 * mode's panels go back to its first-run dock (a track's, a car's or a masks project's; the other modes' layouts are kept), uncollapsed, and the layout file
 * is rewritten with it. */
void ui_reset_layout(void);
/* the Window menu's INDEX-th panel clicked (closed when open, open when closed): 1 when the mode has one. For --steps. */
int ui_panel_toggle(const App *a, int index);
/* the mode's panels as the Window menu lists them (open or closed, drawn this frame or not), then `unlisted:` and the panels drawn that the menu does not list. For --steps. */
void ui_print_panels(const App *a);
/* the room on screen and the File menu's items in it, in order (`room:` and `file:` lines). For --steps. */
void ui_print_menu(const App *a);

/* The orbit the Model window starts the next car at (degrees, degrees, model units; dist 0 keeps the home distance): a headless picture of a view. */
void ui_model_orbit(double yaw, double pitch, int dist);
/* The Model window's body shine on or off (on by default). */
void ui_model_big(int on);
void ui_model_pixel(int pixel);
void ui_model_soft(int on);
void ui_model_shine(int on);

/* 1 when a menu or a popup is open: Escape then closes it and is not the window's own. */
int ui_popup_open(void);

/* Gives ImGui an event: the panels read the mouse and the keys from it. */
void ui_event(const SDL_Event *ev);

/* One frame: the dock space and its panels (the top view, the project), and the shortcuts; drawn over the cleared window, before
 * SDL_RenderPresent. The panels change `a`: the points, the view, the selection. */
void ui_frame(SDL_Renderer *ren, App *a);

/* ImGui's recoverable errors ("EndDisabled() too many times", a Begin with no End...): ui_open has ImGui tell them here, each is also printed on stderr as
 * "imgui error: ...". A headless run fails when any was told (main.c); the banner ImGui draws in the window is easy to miss in a test. */
int ui_error_count(void);
const char *ui_error_first(void);
/* a deliberate error, for the guard's test (--steps ui-fault) */
void ui_fault(void);

#ifdef __cplusplus
}
#endif

#endif
