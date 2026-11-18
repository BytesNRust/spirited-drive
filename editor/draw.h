/* The editor's drawing: the grid, the road as its nodes (red where a limit breaks), the control points and the text (SDL's debug font). */
#ifndef TNFS_TRACK_EDITOR_DRAW_H
#define TNFS_TRACK_EDITOR_DRAW_H

#include "app.h"

#include <stddef.h>
#include <SDL3/SDL.h>

/* draws the whole window for `a` (the view's size is the renderer's output size) */
void draw_scene(SDL_Renderer *ren, const App *a);

/* writes what the renderer has drawn last to a PNG at `path`; 1, or 0 with `why` */
int draw_save_png(SDL_Renderer *ren, const char *path, char *why, size_t why_cap);

#endif
