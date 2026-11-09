/* port: the frame the 3D view draws into and where the view sits on it, moved out of ported_view.c */
#include "game/viewer/view_screen.h"

PortedViewport view_viewport = { 0, 0, WIDTH, HEIGHT, FOV, 0x10000, 0, 0, WIDTH, HEIGHT, 0, 0, 1 };

/* port: the frame the 3D view draws into, 640 x 480 but on a wide screen (ported_view_set_screen) */
int32_t view_screen_w = WIDTH, view_screen_h = HEIGHT;

void ported_view_set_screen(int width, int height)
{
    view_screen_w = width, view_screen_h = height;
}

void ported_view_set_viewport(const PortedViewport *v)
{
    view_viewport = *v;
}
