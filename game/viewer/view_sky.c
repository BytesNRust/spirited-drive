/* port (the sky): the horizon and the 3D sky ring drawn over the view, one band at a time, taken out of ported_view.c.
 * ported_view.c builds the SkyStage and calls sky_stage_band; what this file takes of the view is in view_shared.h. */

#include "game/gfx/flat.h"
#include "game/viewer/view_screen.h"
#include "game/viewer/weather_hooks.h"
#include "viewer/psx_sky_draw.h"
#include "viewer/sky_smooth.h"
#include "viewer/view_sky.h"
#include "viewer/view_spray.h"

#include <string.h>

typedef struct {
    const QuadTarget *quad;        /* the box (a band's rows of it when banded) */
    const RasterTarget *raster;
    const HorizonSmooth *smooth;   /* port: EXTRAS' Texture Filter on the ring's slices (sky_smooth.h), NULL off */
    const QuadTarget *full;        /* port (render bands): the whole box, for the draws whose edges a narrowed clip would move */
    int banded, top, bottom;       /* and, when banded, the rows this band stores */
} SkyTargets;

/* 004f9e98 = 00425520 -> 004252ec */
static void sky_triangle(void *user, const FshEntry *texture, const int32_t uv[6], const int32_t screen[6])
{
    const SkyTargets *t = user;
    if (t->smooth != NULL && !transparent(texture)) sky_smooth_triangle(t->smooth, t->quad, texture, uv, screen);
    else tri_draw(t->quad, texture, uv, screen, transparent(texture));
}

static void sky_fill(void *user, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t colour)
{
    const SkyTargets *t = user;
    if (t->banded) {   /* a rectangle's rows: the band's part of it */
        const int64_t y0 = y < t->top ? t->top : y, y1 = (int64_t)y + h > t->bottom ? t->bottom : (int64_t)y + h;
        if (y1 <= y0) return;
        y = (int32_t)y0, h = (int32_t)(y1 - y0);
    }
    raster_fill(t->raster, x, y, w, h, t->smooth != NULL ? t->smooth->map[colour] : colour);   /* port: the PS1 / PC alt sky's bitmap index is of its own palette */
}

/* 00484389 */
static void sky_polygon(void *user, uint8_t colour, const int32_t points[8])
{
    const SkyTargets *t = user;
    const uint8_t shown = t->smooth != NULL ? t->smooth->map[colour] : colour;
    if (t->banded) flat_polygon_rows(t->full, shown, 4, points, t->top, t->bottom);
    else flat_polygon(t->quad, shown, 4, points);
}

static int sky_off;  /* 004fb786 */

/* 004fbad8, the race's: the "m" tracks drawn turned over (004496ac, 00449f0c, 0043ae64, 0043b298) */
int view_mirrored(void) { return shadow_race != NULL && shadow_race->mirrored != 0; }

void ported_view_set_sky_off(int off)
{
    sky_off = off;
}

SkyRing sky_ring;   /* built once a frame by the caller, read by every band */

static void draw_sky_ring(const SkyTargets *targets, const View *view, const FshEntry *horz, const TrackInfo *info,
                          const TrackCamera *cam, int32_t yaw, int32_t roll)
{
    const SkyRing *ring = &sky_ring;
    const int32_t rs = view_viewport.render_scale > 0 ? view_viewport.render_scale : 1;
    SkyFrame frame = { view, cam->matrix, { 0 }, yaw, roll, view_mirrored(), SKY_WORLD_SHIFT, 0, { 0 },
                       { view_viewport.scale, view_viewport.width, view_viewport.sky_left, view_viewport.sky_top, view_viewport.sky_width,
                         view_viewport.sky_height }, view_screen_w / rs, view_screen_w, 0, sky_off, 1, rs };   /* port: wide fields, render scale */
    for (int i = 0; i < 3; i++) frame.position[i] = -cam->position[i] >> SKY_POSITION_SHIFT;
    horizon_point(view, WORLD_SHIFT, yaw, cam->matrix, frame.horizon);
    const SkySink sink = { (void *)targets, sky_triangle, sky_fill, sky_polygon };
    sky_ring_draw(ring, info, &frame, horz, &sink);
}

void sky_stage_band(void *user, int top, int bottom)
{
    const SkyStage *st = user;
    for (int32_t y = top; y < bottom; y++) {   /* 0044cc74 clears nothing; the view only, for the frames without one */
        const int32_t x0 = st->box_left < 0 ? 0 : st->box_left, x1 = st->box_right > view_screen_w ? view_screen_w : st->box_right;
        if (x1 > x0) memset(st->pixels + (size_t)y * view_screen_w + x0, 0, (size_t)(x1 - x0));
    }
    QuadTarget quad = *st->quad;   /* the box's rows this band owns */
    ClipRect clip = *st->clip;
    if (st->banded) {
        if (top > quad.clip_top) quad.clip_top = top, clip.y0 = top;
        if (bottom < quad.clip_bottom) quad.clip_bottom = bottom, clip.y1 = bottom;
        if (quad.clip_top >= quad.clip_bottom) return;
    }
    if (st->ring) {  /* the circuits' 3D ring */
        const SkyTargets targets = { &quad, st->target, st->smooth, st->quad, st->banded, top, bottom };
        draw_sky_ring(&targets, st->view, st->ring_horz, st->info, st->cam, st->pc->yaw, st->pc->roll);
    } else if (st->flat) {
        const HorizonTarget ht = { st->pixels, view_screen_w };
        if (st->smooth != NULL) {  /* the magnified flat sky blended */
            horizon_draw_flat_smooth(st->smooth, st->horizon, &ht, &clip, st->horz, st->pc->yaw, view_mirrored(),
                                     sky_off, st->line);
        } else if (st->banded) {  /* the box is the whole clip's, only the stores are the band's */
            horizon_draw_flat_rows(st->horizon, &ht, st->clip, st->horz, st->pc->yaw, view_mirrored(), sky_off,
                                   st->line, top, bottom);
        } else {
            horizon_draw_flat(st->horizon, &ht, &clip, st->horz, st->pc->yaw, view_mirrored(), sky_off, st->line);
        }
    }
}
