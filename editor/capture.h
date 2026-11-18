/* The editor's picture of its window: what the renderer has drawn last, as a PNG (the --shot of a headless run). */
#ifndef TNFS_TRACK_EDITOR_CAPTURE_H
#define TNFS_TRACK_EDITOR_CAPTURE_H

#include <SDL3/SDL.h>
#include <stddef.h>

/* writes what the renderer has drawn last to a PNG at `path`; 1, or 0 with `why` */
int capture_save_png(SDL_Renderer *ren, const char *path, char *why, size_t why_cap);

#endif
