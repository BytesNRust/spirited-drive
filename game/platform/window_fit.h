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
WindowRect window_fit(int fb_w, int fb_h, int want_h, int x, int y, const WindowRect *usable, const WindowBorders *borders);

#endif
