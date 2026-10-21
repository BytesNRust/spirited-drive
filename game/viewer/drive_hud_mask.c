/* port: drive_hud.c's frame through the cockpit mask (00421c18's surfaces and calls), its own file. */
#include "viewer/drive_hud_mask.h"

#include "game/gfx/blit.h"
#include "game/gfx/flat.h"
#include "game/sys/shape_bank.h"
#include "game/race/race_chase.h"
#include "game/race/race_view.h"
#include "game/race/hud_mask.h"

#include <string.h>

/* 00421c18's surfaces: the view surface and the frame (004c2da0 and the
 * default are both the frame here). */
typedef struct {
    DriveHud *h;
    QuadTarget screen, view, *target;
} MaskSurfaces;

static void mask_select(void *ctx, HudSurface s)
{
    MaskSurfaces *m = ctx;
    m->target = s == HUD_SURFACE_VIEW ? &m->view : &m->screen;
}

static void mask_clip(void *ctx, int32_t left, int32_t right, int32_t top, int32_t bottom)
{
    MaskSurfaces *m = ctx;
    m->target->clip_left = left, m->target->clip_right = right;
    m->target->clip_top = top, m->target->clip_bottom = bottom;
}

static void mask_fill(void *ctx, uint32_t colour)
{
    const QuadTarget *t = ((MaskSurfaces *)ctx)->target;
    flat_fill_rect(t, t->clip_left, t->clip_top, t->clip_right - t->clip_left, t->clip_bottom - t->clip_top, colour);
}

static void mask_draw(void *ctx, uint8_t *shape)
{
    blit_shape_placed_transparent(((MaskSurfaces *)ctx)->target, shape);
}

HudViewClipInputs drive_hud_view_clip_inputs(int width, int height)
{
    return (HudViewClipInputs){ DRIVE_HUD_LIVE_RACE, DRIVE_HUD_HIGH_RES, width, height, width, height };
}

static void mask_view_clip(void *ctx)
{
    MaskSurfaces *m = ctx;
    const HudViewClipInputs in = drive_hud_view_clip_inputs(DRIVE_VIEW_W, DRIVE_VIEW_H);
    hud_view_clip(m->target, &m->h->hud, &in);
}

static void mask_present(void *ctx)
{
    MaskSurfaces *m = ctx;
    blit_shape(m->target, m->h->view, 0, 0, BLIT_OPAQUE);
}

static void mask_runs(void *ctx, const uint8_t *runs)
{
    MaskSurfaces *m = ctx;
    blit_runs(m->target, DRIVE_VIEW_W, m->h->view, runs);
}

/* port: the outside view without its strip draws below the exe's row, where the mask's runs (the exe's rows) never
 * reach, so those lines would keep the frame the view was last presented whole. They are copied from the view as
 * the runs do above, and the tach (which the runs go round) put back over them; the needle and the map come after. */
static void view_below_runs(DriveHud *h, const RaceCamera *camera, const QuadTarget *screen, const QuadTarget *view)
{
    enum { HIGH_SCREEN = 1 };
    if (camera == NULL || camera->layout_row == NULL || camera->layout < 0 || camera->layout >= VIEW_LAYOUTS) return;
    const ViewLayout *l = camera->layout_row, *exe = &race_view_layouts[HIGH_SCREEN][camera->layout];
    const int top = exe->top + exe->y + exe->unknown_24, bottom = l->top + l->y + l->unknown_24;   /* the clip boxes */
    const int left = l->left + l->x, right = left + l->width;
    if (bottom <= top || left < 0 || right > screen->pitch || bottom > screen->clip_bottom) return;
    for (int y = top; y < bottom; y++)
        memcpy(screen->pixels + (size_t)y * screen->pitch + left, view->pixels + (size_t)y * view->pitch + left,
               (size_t)(right - left));
    const uint8_t *tach = shape_bank_find(h->art.bank, (const uint8_t *)"tach");
    if (tach == NULL) return;
    QuadTarget below = *screen;
    below.clip_top = top;
    blit_shape_placed_transparent(&below, tach);
}

void drive_hud_mask(DriveHud *h, const RaceState *race, uint8_t *pixels, int width, int height, int32_t changed)
{
    MaskSurfaces m = { h, { pixels, width, 0, 0, width, height },
                       { h->view + DRIVE_VIEW_HEADER, DRIVE_VIEW_W, 0, 0, DRIVE_VIEW_W, DRIVE_VIEW_H }, NULL };
    m.target = &m.screen;
    const HudMaskOps ops = { &m, mask_select, mask_clip, mask_fill, mask_draw, mask_view_clip, mask_present, mask_runs };
    const RaceCamera *camera = race->chase.camera;
    const int32_t use_parts = h->keys.use_parts != 0 && h->parts[0][0] != NULL;  /* 004f9b3c, while the parts were read */
    const HudMaskInputs in = { camera != NULL ? camera->view : 0, 0, 0, use_parts, 0, width, height, h->strip_colour };
    uint8_t clear_view = 0;
    hud_mask_draw(&ops, &h->art, &h->hud, &clear_view, &in, changed);
    if (changed == 0) view_below_runs(h, camera, &m.screen, &m.view);
}
