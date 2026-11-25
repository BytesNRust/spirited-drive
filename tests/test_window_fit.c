/* SPDX-License-Identifier: GPL-3.0-or-later */
/* window_fit: the windowed size and place that stay inside the display's usable area when the window comes out of fullscreen. */
#include "game/platform/window_fit.h"
#include "test.h"

int main(void)
{
    const WindowRect screen = { 0, 0, 1920, 1040 };   /* 1080p less a 40 pixel taskbar */
    const WindowBorders frame = { 31, 8, 8, 8 };       /* a title bar and thin sides */
    /* the old size, the screen's own height (what a window read before it left fullscreen would give), is cut to what holds */
    WindowRect r = window_fit(640, 480, 1080, 0, 0, &screen, &frame, 1.0);
    CHECK_EQ_INT(r.h, 1040 - 31 - 8);
    CHECK_EQ_INT(r.w, r.h * 640 / 480 > 1920 - 16 ? 1920 - 16 : r.h * 640 / 480);
    CHECK(r.x >= 8 && r.x + r.w + 8 <= 1920 && r.y >= 31 && r.y + r.h + 8 <= 1040);
    /* a size that fits is kept, in the framebuffer's shape */
    r = window_fit(640, 480, 720, 100, 100, &screen, &frame, 1.0);
    CHECK_EQ_INT(r.h, 720);
    CHECK_EQ_INT(r.w, 960);
    CHECK_EQ_INT(r.x, 100);
    CHECK_EQ_INT(r.y, 100);
    /* a wide framebuffer on a narrow area: the width rules and the shape holds */
    const WindowRect narrow = { 0, 0, 800, 1000 };
    r = window_fit(854, 480, 900, 0, 0, &narrow, &frame, 1.0);
    CHECK_EQ_INT(r.w, 800 - 16);
    CHECK_EQ_INT(r.h, (800 - 16) * 480 / 854);
    /* a window left partly below the taskbar is moved up, one to the right of the screen moved left */
    r = window_fit(640, 480, 600, 1500, 900, &screen, &frame, 1.0);
    CHECK_EQ_INT(r.y, 1040 - 8 - 600);
    CHECK_EQ_INT(r.x, 1920 - 8 - 800);
    /* a second display to the right: its own origin counts */
    const WindowRect second = { 1920, 0, 2560, 1400 };
    r = window_fit(640, 480, 1440, 1920, 0, &second, &frame, 1.0);
    CHECK_EQ_INT(r.h, 1400 - 39);
    CHECK(r.x >= 1920 + 8 && r.x + r.w + 8 <= 1920 + 2560);
    /* a density that is not a whole number (Windows at 125, 150 %): the window's pixels are an exact multiple of the picture, so Whole fills it */
    r = window_fit(640, 480, 720, 100, 100, &screen, &frame, 1.5);   /* 720 points are 1080 pixels: 2x, 1280x960 pixels */
    CHECK_EQ_INT(r.w, 854);
    CHECK_EQ_INT(r.h, 640);
    r = window_fit(640, 480, 480, 100, 100, &screen, &frame, 1.25);   /* 480 points are 600 pixels: 1x */
    CHECK_EQ_INT(r.w, 512);
    CHECK_EQ_INT(r.h, 384);
    r = window_fit(640, 480, 480, 100, 100, &screen, &frame, 2.0);   /* a whole density: the points are the size asked for */
    CHECK_EQ_INT(r.w, 640);
    CHECK_EQ_INT(r.h, 480);
    CHECK(window_fit_density_is_whole(1.0) && window_fit_density_is_whole(2.0) && window_fit_density_is_whole(0.0) && !window_fit_density_is_whole(1.25) && !window_fit_density_is_whole(1.5));
    {
        static const double DENSITIES[] = { 1.25, 1.5, 1.75, 2.25, 2.5 };
        static const WindowRect AREAS[] = { { 0, 0, 1366, 728 }, { 0, 0, 1920, 1040 }, { 0, 0, 2560, 1400 } };
        static const int SHAPES[][2] = { { 640, 480 }, { 854, 480 }, { 320, 200 } };
        for (size_t d = 0; d < sizeof DENSITIES / sizeof *DENSITIES; d++)
            for (size_t a = 0; a < sizeof AREAS / sizeof *AREAS; a++)
                for (size_t k = 0; k < sizeof SHAPES / sizeof *SHAPES; k++)
                    for (int want = 100; want <= 1500; want += 70) {
                        const double den = DENSITIES[d];
                        const int fw = SHAPES[k][0], fh = SHAPES[k][1];
                        r = window_fit(fw, fh, want, 0, 0, &AREAS[a], &frame, den);
                        const double pw = r.w * den, ph = r.h * den;
                        const int nw = (int)((pw + 1e-6) / fw), nh = (int)((ph + 1e-6) / fh);
                        CHECK(r.w <= AREAS[a].w - 16 && r.h <= AREAS[a].h - 39);   /* inside the area (the smallest picture holds on every one of these) */
                        CHECK(nw >= 1 && nw == nh);                                  /* the same whole scale across and down */
                        CHECK(pw - fw * nw >= -1e-6 && ph - fh * nh >= -1e-6);       /* never a pixel short: a black line */
                        CHECK(pw - fw * nw < den + 1e-6 && ph - fh * nh < den + 1e-6);   /* and at most one point over */
                        CHECK(r.x >= 8 && r.y >= 31);
                    }
    }
    /* never zero */
    const WindowRect tiny = { 0, 0, 10, 10 };
    r = window_fit(640, 480, 0, 0, 0, &tiny, &frame, 1.0);
    CHECK(r.w >= 1 && r.h >= 1);
    return TEST_REPORT();
}
