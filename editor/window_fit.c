/* The size of Studio's window on a normal run (window_fit.h) */
#include "window_fit.h"

void window_fit(int usable_w, int usable_h, int *w, int *h)
{
    const int room_h = usable_h - WINDOW_TITLE_H;
    if (usable_w >= WINDOW_FIT_W && room_h >= WINDOW_FIT_H) {
        *w = WINDOW_FIT_W, *h = WINDOW_FIT_H;
    } else if ((long)usable_w * WINDOW_FIT_H <= (long)room_h * WINDOW_FIT_W) {   /* the width is the narrower side of 16:10: it decides */
        *w = usable_w, *h = (int)((long)usable_w * WINDOW_FIT_H / WINDOW_FIT_W);
    } else {
        *h = room_h, *w = (int)((long)room_h * WINDOW_FIT_W / WINDOW_FIT_H);
    }
    if (*w < WINDOW_LEAST_W || *h < WINDOW_LEAST_H) *w = WINDOW_LEAST_W, *h = WINDOW_LEAST_H;
}
