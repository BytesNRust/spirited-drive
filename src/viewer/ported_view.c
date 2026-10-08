#include "viewer/ported_view.h"

#include "game/race/track_render.h"
#include "game/race/view.h"
#include "viewer/raster.h"

#include <stdlib.h>

enum {
    WIDTH = 640,
    HEIGHT = 480,
    FOV = 96,              /* chase views (004b78a8, view 1) */
    WORLD_SHIFT = 11,      /* 004c52bc */
    EYE_HEIGHT = 111411    /* camera mode 2 (004b6bb0): 1.7 m */
};

static void draw_uv(void *user, const FshEntry *texture, const int32_t uv[8], const int32_t screen[8])
{
    raster_screen_quad(user, screen, uv, texture, texture->type == TRACK_TEXTURE_TRANSPARENT_TYPE);
}

static void draw_full(void *user, const FshEntry *texture, const int32_t screen[8])
{
    const int32_t w = (texture->width - 1) * 65536, h = (texture->height - 1) * 65536;
    const int32_t uv[8] = { 0, 0, w, 0, w, h, 0, h };
    raster_screen_quad(user, screen, uv, texture, texture->type == TRACK_TEXTURE_TRANSPARENT_TYPE);
}

void ported_view_render(const Tri *tri, const TrackArt *art, uint32_t node, int32_t yaw, uint8_t *pixels)
{
    static TrackRender render;
    static View view;
    TriNode n;
    if (tri_node(tri, node, &n) != TRI_OK) return;
    view_init(&view, 0, 0, WIDTH, HEIGHT, FOV, FOV);
    RasterTarget target = { pixels, WIDTH, HEIGHT, 0, 0 };
    DrawContext ctx = { &view, { 0, 0, WIDTH, HEIGHT }, WORLD_SHIFT, { 0, 0, 0x1999 }, { &target, draw_uv, draw_full } };
    TrackCamera cam = { { n.position.x, n.position.y + EYE_HEIGHT, n.position.z }, { 0 }, (int32_t)node };
    track_camera_matrix(0, n.orientation * 0x400 + yaw, 0, 0, cam.matrix);
    track_render_build(&render, tri, art, (int32_t)node);
    track_render_transform(&render, &cam, &ctx);
    track_render_draw(&render, &cam, &ctx, 0);
}
