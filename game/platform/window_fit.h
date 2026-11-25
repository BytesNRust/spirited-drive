/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: the windowed window kept inside the part of the screen a window can use (the display's usable area: the desktop less the taskbar or the menu bar, and less the window's own title bar and borders). */
#ifndef TNFS_WINDOW_FIT_H
#define TNFS_WINDOW_FIT_H

typedef struct {
    int x, y, w, h;
} WindowRect;

typedef struct {
    int top, left, bottom, right;
} WindowBorders;

/* The client area of a window that keeps the framebuffer's shape (`fb_w` x `fb_h`) and is as tall as `want_h` when that fits: its size is cut down, in the same shape, to what `usable` (the display's usable area) holds
 * once the window's `borders` are counted. (`x`, `y`) is where its client area is now; the result moves it the least that puts the whole window (client area and borders) inside `usable`. Never smaller than 1 x 1; the
 * result is the client area. */
WindowRect window_fit(int fb_w, int fb_h, int want_h, int x, int y, const WindowRect *usable, const WindowBorders *borders, double density);

/* `density` is the window's pixels per point (SDL_GetWindowPixelDensity; 0 or less counts as 1). Whole (integer scale) counts pixels: at 1.25 or 1.5 a window of
 * fb points is 800 or 960 pixels wide for a 640 pixel picture and the largest whole scale is 1x, with a thick black frame. When the density is not a whole number
 * the window is sized so that its pixel size is `fb * N` (N the whole scale nearest to the size asked for, cut down to what the usable area holds, never below 1):
 * the points are the least whose pixels are not fewer than that, so the picture fills the window at N (the window can be one pixel wider when the system rounds
 * up). With a whole density (1.0 on most screens, 2.0 on a retina one) every point is a whole number of pixels and nothing changes from what is described above. */
int window_fit_density_is_whole(double density);

#endif
