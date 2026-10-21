#include "game/race/track_fence.h"

#include "game/gfx/x86_int.h"

enum {
    TALL = 0x20000,
    LOW = 0xE666
};

/* 0044a2b4 */
void track_fence_draw(const TrackFenceOps *ops, const TrackFenceInputs *in, const uint8_t pair[2], int32_t base,
                      const TrackUv *uv, int32_t variant, int32_t texture, int tall)
{
    const int32_t height = tall ? TALL : LOW;
    const int32_t a = base + pair[0], b = base + pair[1];
    if (in->camera[b][2] < in->near) return;
    int32_t screen[8] = { 0, 0, 0, 0, in->screen[a][0], in->screen[a][1], in->screen[b][0], in->screen[b][1] };
    const int32_t top_b[3] = { in->world[b][0], x_add(in->world[b][1], height), in->world[b][2] };
    const int32_t top_a[3] = { in->world[a][0], x_add(in->world[a][1], height), in->world[a][2] };
    int32_t cam_tb[3], cam_ta[3];
    ops->point_screen(ops->ctx, top_b, cam_tb, &screen[0]);
    ops->point_screen(ops->ctx, top_a, cam_ta, &screen[2]);
    if (outcode_shared(&in->clip, screen, 4) != 0) return;
    const FshEntry *t = ops->texture(ops->ctx, x_add(x_add(uv->variant, variant), x_shl(texture, 3)));
    if (t == NULL) return;
    const int32_t *const corners[4] = { cam_tb, cam_ta, in->camera[a], in->camera[b] };
    if (uv->variant == 0) {
        ops->subdivide(ops->ctx, t, uv->u_level, uv->u_cell, corners, screen, in->near);
    } else if (ops->full_camera != NULL) {
        ops->full_camera(ops->ctx, t, corners);
    } else {
        ops->full_alt(ops->ctx, t, screen);
    }
}
