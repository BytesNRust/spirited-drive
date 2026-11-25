/* SPDX-License-Identifier: GPL-3.0-or-later */
/* window_fit: the windowed size and place that stay inside the display's usable area when the window comes out of fullscreen. */
#include "game/platform/window_fit.h"
#include "test.h"

int main(void)
{
    const WindowRect screen = { 0, 0, 1920, 1040 };   /* 1080p less a 40 pixel taskbar */
    const WindowBorders frame = { 31, 8, 8, 8 };       /* a title bar and thin sides */
    /* the old size, the screen's own height (what a window read before it left fullscreen would give), is cut to what holds */
    WindowRect r = window_fit(640, 480, 1080, 0, 0, &screen, &frame);
    CHECK_EQ_INT(r.h, 1040 - 31 - 8);
    CHECK_EQ_INT(r.w, r.h * 640 / 480 > 1920 - 16 ? 1920 - 16 : r.h * 640 / 480);
    CHECK(r.x >= 8 && r.x + r.w + 8 <= 1920 && r.y >= 31 && r.y + r.h + 8 <= 1040);
    /* a size that fits is kept, in the framebuffer's shape */
    r = window_fit(640, 480, 720, 100, 100, &screen, &frame);
    CHECK_EQ_INT(r.h, 720);
    CHECK_EQ_INT(r.w, 960);
    CHECK_EQ_INT(r.x, 100);
    CHECK_EQ_INT(r.y, 100);
    /* a wide framebuffer on a narrow area: the width rules and the shape holds */
    const WindowRect narrow = { 0, 0, 800, 1000 };
    r = window_fit(854, 480, 900, 0, 0, &narrow, &frame);
    CHECK_EQ_INT(r.w, 800 - 16);
    CHECK_EQ_INT(r.h, (800 - 16) * 480 / 854);
    /* a window left partly below the taskbar is moved up, one to the right of the screen moved left */
    r = window_fit(640, 480, 600, 1500, 900, &screen, &frame);
    CHECK_EQ_INT(r.y, 1040 - 8 - 600);
    CHECK_EQ_INT(r.x, 1920 - 8 - 800);
    /* a second display to the right: its own origin counts */
    const WindowRect second = { 1920, 0, 2560, 1400 };
    r = window_fit(640, 480, 1440, 1920, 0, &second, &frame);
    CHECK_EQ_INT(r.h, 1400 - 39);
    CHECK(r.x >= 1920 + 8 && r.x + r.w + 8 <= 1920 + 2560);
    /* never zero */
    const WindowRect tiny = { 0, 0, 10, 10 };
    r = window_fit(640, 480, 0, 0, 0, &tiny, &frame);
    CHECK(r.w >= 1 && r.h >= 1);
    return TEST_REPORT();
}
