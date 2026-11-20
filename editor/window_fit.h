/* The size of Studio's window on a normal run: what the display leaves for it. A headless run (--frames, --shot) and --window keep their own size. */
#ifndef EDITOR_WINDOW_FIT_H
#define EDITOR_WINDOW_FIT_H

enum {
    WINDOW_FIT_W = 1280, WINDOW_FIT_H = 800,   /* the most it opens at: 16:10 */
    WINDOW_LEAST_W = 960, WINDOW_LEAST_H = 600, /* the least, the size the panels were laid out for */
    WINDOW_TITLE_H = 28                        /* the title bar above the window's content (a macOS window's) */
};

/* `w` x `h` for a display whose usable area (SDL_GetDisplayUsableBounds: no menu bar, no Dock) is usable_w x usable_h, in points: WINDOW_FIT_W x
 * WINDOW_FIT_H when it fits with the title bar, else the largest 16:10 that fits, never under WINDOW_LEAST_W x WINDOW_LEAST_H */
void window_fit(int usable_w, int usable_h, int *w, int *h);

#endif
