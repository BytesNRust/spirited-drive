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
int ui_open(SDL_Window *win, SDL_Renderer *ren, const char *ini, char *why, size_t why_cap);
void ui_close(void);

/* 1 when a menu or a popup is open: Escape then closes it and is not the window's own. */
int ui_popup_open(void);

/* Gives ImGui an event: the panels read the mouse and the keys from it. */
void ui_event(const SDL_Event *ev);

/* One frame: the dock space and its panels (the top view, the project), and the shortcuts; drawn over the cleared window, before
 * SDL_RenderPresent. The panels change `a`: the points, the view, the selection. */
void ui_frame(SDL_Renderer *ren, App *a);

#ifdef __cplusplus
}
#endif

#endif
