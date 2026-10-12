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
