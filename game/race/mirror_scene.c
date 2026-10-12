/* The rear-view mirror's scene (0044baa4). */
#include "game/race/mirror_scene.h"

#include "game/gfx/x86_int.h"

void mirror_bounds(const ViewLayout *layout, MirrorBounds *b)
{
    b->left = layout->left;
    b->top = layout->top;
    b->right = x_add(b->left, layout->unknown_20);
    b->bottom = x_add(b->top, layout->unknown_24);
}

int mirror_quad_visible(const MirrorBounds *b, const int32_t screen[8])
{
    int left = 1, right = 1;
    for (int i = 0; i < 4; i++) {
        if (screen[2 * i] >= b->left) left = 0;
        if (screen[2 * i] <= b->right) right = 0;
    }
    if (left || right) return 0;
    return b->bottom >= screen[3] || b->bottom >= screen[5];
}
