#include "viewer/ported_view.h"

#include "game/race/horizon.h"
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

/* The 3D engine is not ported: the viewer's stand-in draws the models. */
static const ViewModel *models;
static const int *model_ok;
static RasterTarget model_target;
static Camera model_camera;

static void draw_model(void *user, int model, const int32_t position[3], uint32_t heading)
{
    (void)user;
    if (models == NULL || model < 0 || model >= PORTED_VIEW_MODELS || !model_ok[model]) return;
    model_draw(&models[model], &model_target, &model_camera, (TrackPoint){ position[0], position[1], position[2] },
               heading);
}

void ported_view_set_models(const ViewModel *list, const int *ok)
{
    models = list;
    model_ok = ok;
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
    DrawContext ctx = { &view, { 0, 0, WIDTH, HEIGHT }, WORLD_SHIFT, { 0, 0, 0x1999 },
                        { &target, draw_uv, draw_full, draw_full, draw_model } };
    TrackCamera cam = { { n.position.x, n.position.y + EYE_HEIGHT, n.position.z }, { 0 }, (int32_t)node };
    const int32_t cam_yaw = n.orientation * 0x400 + yaw;
    track_camera_matrix(0, cam_yaw, 0, 0, cam.matrix);
    /* TODO(00473680): the frame is cleared first; colour 0 for now. */
    memset(pixels, 0, WIDTH * HEIGHT);
    if (art->has_horizon) {
        /* TODO(0043b298): ring tracks (info->ring) use the 3D sky ring; drawn
         * flat until it is ported. */
        (void)info;
        const HorizonTarget ht = { pixels, WIDTH };
        horizon_draw_flat(&horizon, &ht, &ctx.clip, 0, &art->horizon, cam_yaw, 0,
                          horizon_line(&view, WORLD_SHIFT, cam_yaw, cam.matrix));
    }
    track_render_build(&render, tri, art, (int32_t)node);
    track_render_transform(&render, &cam, &ctx);
    /* The stand-in model drawing works in metres with float axes. */
    model_target = (RasterTarget){ pixels, WIDTH, HEIGHT, (float)view.focal_x, 0.3f };
    model_camera.eye = (Vec3){ cam.position[0] / 65536.0f, cam.position[1] / 65536.0f, cam.position[2] / 65536.0f };
    model_camera.right = (Vec3){ cam.matrix[0] / 65536.0f, cam.matrix[3] / 65536.0f, cam.matrix[6] / 65536.0f };
    model_camera.up = (Vec3){ cam.matrix[1] / 65536.0f, cam.matrix[4] / 65536.0f, cam.matrix[7] / 65536.0f };
    model_camera.forward = (Vec3){ cam.matrix[2] / 65536.0f, cam.matrix[5] / 65536.0f, cam.matrix[8] / 65536.0f };
    track_render_draw(&render, &cam, &ctx, 0, 0);
}
