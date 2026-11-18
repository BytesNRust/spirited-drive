/* The editor's interface layer, Dear ImGui over the SDL renderer (third_party/imgui, docking branch). The only C++ of the editor: the rest
 * (app, view, project, build, draw, main) is C and does not know ImGui. This is the layer's C face. */
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

/* Gives ImGui an event. 1 when ImGui took it (a click on a panel, a key into a text field): the editor must not act on it too. */
int ui_event(const SDL_Event *ev);

/* One frame of the panels over what draw_scene drew: call after draw_scene and before SDL_RenderPresent. */
void ui_frame(SDL_Renderer *ren, const App *a);

#ifdef __cplusplus
}
#endif

#endif
