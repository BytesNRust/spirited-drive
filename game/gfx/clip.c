/* The draw surface's clip rectangle (clip.h). */
#include "game/gfx/clip.h"

void clip_set(QuadTarget *t, int32_t left, int32_t right, int32_t top, int32_t bottom, int32_t surface_w,
              int32_t surface_h)
{
    t->clip_left = left < 0 ? 0 : left;
    t->clip_top = top < 0 ? 0 : top;
    t->clip_right = right > surface_w ? surface_w : right;
    t->clip_bottom = bottom > surface_h ? surface_h : bottom;
}

void clip_box(QuadTarget *t, int32_t x, int32_t y, int32_t w, int32_t h, int32_t surface_w, int32_t surface_h)
{
    clip_set(t, x, (int32_t)((uint32_t)w + (uint32_t)x), y, (int32_t)((uint32_t)h + (uint32_t)y), surface_w, surface_h);
}

void clip_screen(QuadTarget *t, int32_t screen_w, int32_t screen_h, int32_t surface_w, int32_t surface_h)
{
    clip_set(t, 0, screen_w, 0, screen_h, surface_w, surface_h);
}
