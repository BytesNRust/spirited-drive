/* The size of Studio's window on a normal run (editor/window_fit.h): 1280 x 800 when the display's usable area (no menu bar, no Dock) holds it with its
 * title bar, else the largest 16:10 that does, never under 960 x 600 (the size every panel was laid out for). */
#include "window_fit.h"
#include "test.h"

static void check(int usable_w, int usable_h, int want_w, int want_h)
{
    int w = 0, h = 0;
    window_fit(usable_w, usable_h, &w, &h);
    CHECK_EQ_INT(w, want_w);
    CHECK_EQ_INT(h, want_h);
}

int main(void)
{
    check(1382, 875, 1280, 800);    /* the owner's MacBook Pro 13" (1440 x 900, Dock on the left): it fits */
    check(2560, 1415, 1280, 800);   /* a big display: 1280 x 800 all the same */
    check(1440, 828, 1280, 800);    /* just: 800 and the 28 of the title bar */
    check(1280, 775, 1195, 747);    /* a 1280 x 800 Air, Dock at the bottom hidden: the height decides, 16:10 kept */
    check(1024, 743, 1024, 640);    /* 1024 x 768: the width decides */
    check(800, 575, 960, 600);      /* smaller than Studio was made for: 960 x 600, as before */
    check(0, 0, 960, 600);          /* no display found */
    return TEST_REPORT();
}
