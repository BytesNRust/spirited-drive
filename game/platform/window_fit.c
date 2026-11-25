/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see window_fit.h. */
#include "game/platform/window_fit.h"

static int at_least_one(int v) { return v < 1 ? 1 : v; }

int window_fit_density_is_whole(double density)
{
    if (density <= 0) return 1;
    const double d = density - (double)(long long)(density + 0.5);
    return d > -0.001 && d < 0.001;
}

/* the least number of points whose pixels (points * density) are not fewer than `pixels` */
static int points_for(int pixels, double density)
{
    long long p = (long long)(pixels / density);
    while ((double)p * density < pixels - 1e-6) p++;
    while (p > 1 && (double)(p - 1) * density >= pixels - 1e-6) p--;
    return (int)p;
}

WindowRect window_fit(int fb_w, int fb_h, int want_h, int x, int y, const WindowRect *usable, const WindowBorders *borders, double density)
{
    const int max_w = at_least_one(usable->w - borders->left - borders->right);
    const int max_h = at_least_one(usable->h - borders->top - borders->bottom);
    fb_w = at_least_one(fb_w), fb_h = at_least_one(fb_h);
    int h = at_least_one(want_h);
    if (h > max_h) h = max_h;
    int w = at_least_one((int)((long long)h * fb_w / fb_h));
    if (w > max_w) {   /* a wide shape on a narrow area: the width rules */
        w = max_w;
        h = at_least_one((int)((long long)w * fb_h / fb_w));
    }
    if (!window_fit_density_is_whole(density)) {   /* a whole scale of the picture, in pixels */
        int n = (int)((double)at_least_one(want_h) * density / fb_h + 0.5);
        if (n < 1) n = 1;
        while (n > 1 && (points_for(fb_w * n, density) > max_w || points_for(fb_h * n, density) > max_h)) n--;
        if (points_for(fb_w, density) <= max_w && points_for(fb_h, density) <= max_h) {   /* even 1x holds: otherwise the shape's cut above stays */
            w = points_for(fb_w * n, density);
            h = points_for(fb_h * n, density);
        }
    }
    const int min_x = usable->x + borders->left, max_x = usable->x + usable->w - borders->right - w;
    const int min_y = usable->y + borders->top, max_y = usable->y + usable->h - borders->bottom - h;
    if (x > max_x) x = max_x;
    if (x < min_x) x = min_x;
    if (y > max_y) y = max_y;
    if (y < min_y) y = min_y;
    return (WindowRect){ x, y, w, h };
}
