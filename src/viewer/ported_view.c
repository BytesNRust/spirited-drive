#include "viewer/ported_view.h"

#include "game/race/horizon.h"
#include "game/race/sky_ring.h"
#include "game/race/track_render.h"
#include "game/race/view.h"
#include "viewer/raster.h"

#include <stdlib.h>
#include <string.h>

enum {
    WIDTH = 640,
    HEIGHT = 480,
    FOV = 96,              /* chase views (004b78a8, view 1) */
    WORLD_SHIFT = 11,      /* 004c52bc */
    EYE_HEIGHT = 111411,   /* camera mode 2 (004b6bb0): 1.7 m */
    SKY_WORLD_SHIFT = 8,   /* 004bb8d4 */
    SKY_POSITION_SHIFT = 10  /* 0043af80: -position >> 10 */
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

static void sky_triangle(void *user, const FshEntry *texture, const int32_t uv[6], const int32_t screen[6])
{
    raster_screen_triangle(user, screen, uv, texture, texture->type == TRACK_TEXTURE_TRANSPARENT_TYPE);
}

static void sky_fill(void *user, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t colour)
{
    raster_fill(user, x, y, w, h, colour);
}

/* 0043af80: the 3D sky ring of the tracks whose INFO asks for one. */
static void draw_sky_ring(const RasterTarget *target, const View *view, const TrackArt *art, const TrackInfo *info,
                          const TrackCamera *cam, int32_t yaw)
{
    static SkyRing ring;
    sky_ring_build(&ring, info, &art->horizon);
    if (!ring.enabled) return;
    SkyFrame frame = { view, cam->matrix, { 0 }, yaw, 0, 0, SKY_WORLD_SHIFT, 0, { 0 },
                       { 0x10000, WIDTH, 0, 0, WIDTH, HEIGHT }, WIDTH, WIDTH, 0 };
    for (int i = 0; i < 3; i++) frame.position[i] = -cam->position[i] >> SKY_POSITION_SHIFT;
    horizon_point(view, WORLD_SHIFT, yaw, cam->matrix, frame.horizon);
    const SkySink sink = { (void *)target, sky_triangle, sky_fill };
    sky_ring_draw(&ring, info, &frame, &art->horizon, &sink);
}

void ported_view_render(const Tri *tri, const TrackArt *art, const TrackInfo *info, uint32_t node, int32_t yaw,
                        uint8_t *pixels)
{
    static TrackRender render;
    static View view;
    static HorizonTables horizon;
    TriNode n;
    if (tri_node(tri, node, &n) != TRI_OK) return;
    view_init(&view, 0, 0, WIDTH, HEIGHT, FOV, FOV);
    horizon_tables(&horizon, WIDTH, FOV, 0x10000);
    RasterTarget target = { pixels, WIDTH, HEIGHT, 0, 0 };
    DrawContext ctx = { &view, { 0, 0, WIDTH, HEIGHT }, WORLD_SHIFT, { 0, 0, 0x1999 }, { &target, draw_uv, draw_full } };
    TrackCamera cam = { { n.position.x, n.position.y + EYE_HEIGHT, n.position.z }, { 0 }, (int32_t)node };
    const int32_t cam_yaw = n.orientation * 0x400 + yaw;
    track_camera_matrix(0, cam_yaw, 0, 0, cam.matrix);
    /* TODO(00473680): the frame is cleared first; colour 0 for now. */
    memset(pixels, 0, WIDTH * HEIGHT);
    if (art->has_horizon && info->ring != 0) {
        draw_sky_ring(&target, &view, art, info, &cam, cam_yaw);
    } else if (art->has_horizon) {
        const HorizonTarget ht = { pixels, WIDTH };
        horizon_draw_flat(&horizon, &ht, &ctx.clip, 0, &art->horizon, cam_yaw, 0,
                          horizon_line(&view, WORLD_SHIFT, cam_yaw, cam.matrix));
    }
    track_render_build(&render, tri, art, (int32_t)node);
    track_render_transform(&render, &cam, &ctx);
    track_render_draw(&render, &cam, &ctx, 0);
}
