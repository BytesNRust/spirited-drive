#include "game/viewer/render_list.h"

#include "game/sys/bands.h"
#include "game/viewer/modern_road.h"

#include <string.h>

enum { BAND_MIN = 8 };   /* a shorter list is replayed whole on the caller: a pool batch would cost more than it saves */

void render_list_begin(RenderList *l, const QuadTarget *target)
{
    l->target = target;
    l->count = 0;
}

static RenderCmd *next_cmd(RenderList *l, RenderKind kind, const FshEntry *texture, int transparent)
{
    if (l->count == RENDER_LIST_MAX) render_list_flush(l);
    RenderCmd *c = &l->cmds[l->count++];
    memset(c, 0, sizeof *c);
    c->kind = kind;
    c->texture = texture;
    c->transparent = transparent;
    return c;
}

/* the rows the four corners span, a pixel either side for the edge arithmetic's rounding */
static void rows_of(RenderCmd *c)
{
    int32_t lo = c->screen[1], hi = c->screen[1];
    for (int i = 1; i < 4; i++) {
        if (c->screen[2 * i + 1] < lo) lo = c->screen[2 * i + 1];
        if (c->screen[2 * i + 1] > hi) hi = c->screen[2 * i + 1];
    }
    c->top = lo > INT32_MIN + 1 ? lo - 1 : INT32_MIN;
    c->bottom = hi < INT32_MAX - 2 ? hi + 2 : INT32_MAX;
}

void render_list_add_quad(RenderList *l, const FshEntry *texture, const int32_t uv[8], const int32_t screen[8],
                          int transparent)
{
    RenderCmd *c = next_cmd(l, RENDER_QUAD, texture, transparent);
    memcpy(c->uv, uv, sizeof c->uv);
    memcpy(c->screen, screen, sizeof c->screen);
    rows_of(c);
}

void render_list_add_full(RenderList *l, const FshEntry *texture, const int32_t screen[8], int transparent)
{
    RenderCmd *c = next_cmd(l, RENDER_FULL, texture, transparent);
    memcpy(c->screen, screen, sizeof c->screen);
    rows_of(c);
}

void render_list_add_full_tri(RenderList *l, const FshEntry *texture, const int32_t screen[8], int transparent)
{
    RenderCmd *c = next_cmd(l, RENDER_FULL_TRI, texture, transparent);
    memcpy(c->screen, screen, sizeof c->screen);
    rows_of(c);
}

void render_list_add_road_cell(RenderList *l, const View *view, const FshEntry *texture, const int32_t uv[8],
                               const int32_t *const cam[4], int transparent)
{
    RenderCmd *c = next_cmd(l, RENDER_ROAD_CELL, texture, transparent);
    c->view = view;
    memcpy(c->uv, uv, sizeof c->uv);
    for (int i = 0; i < 4; i++) memcpy(c->cam[i], cam[i], sizeof c->cam[i]);
    c->top = INT32_MIN;   /* projected at replay: every band tries it, the clip drops what is not its */
    c->bottom = INT32_MAX;
}

/* t: the target narrowed to the band's rows, for the draws whose rows are independent; `whole`, the unnarrowed one, for
 * the quads (quad.c retraces a twisted quad on the first row it visits that needs it), which take the band as a window */
static void draw(const RenderCmd *c, const QuadTarget *t, const QuadTarget *whole)
{
    switch (c->kind) {
    case RENDER_QUAD:
        quad_draw_rows(whole, c->texture, c->uv, c->screen, c->transparent, t->clip_top, t->clip_bottom);
        break;
    case RENDER_FULL:
        quad_draw_full_rows(whole, c->texture, c->screen, c->transparent, t->clip_top, t->clip_bottom);
        break;
    case RENDER_FULL_TRI: quad_draw_full_tri(t, c->texture, c->screen, c->transparent); break;
    case RENDER_ROAD_CELL: {
        const int32_t *const cam[4] = { c->cam[0], c->cam[1], c->cam[2], c->cam[3] };
        modern_road_cell(t, c->view, c->texture, c->uv, cam, c->transparent);
        break;
    }
    }
}

void render_list_replay(const RenderList *l, int top, int bottom)
{
    QuadTarget band = *l->target;
    if (top > band.clip_top) band.clip_top = top;
    if (bottom < band.clip_bottom) band.clip_bottom = bottom;
    if (band.clip_top >= band.clip_bottom) return;
    for (int i = 0; i < l->count; i++) {
        const RenderCmd *c = &l->cmds[i];
        if (c->bottom <= band.clip_top || c->top >= band.clip_bottom) continue;
        draw(c, &band, l->target);
    }
}

static void replay_band(void *ctx, int top, int bottom) { render_list_replay(ctx, top, bottom); }

void render_list_flush(RenderList *l)
{
    if (l->count == 0) return;
    const QuadTarget *t = l->target;
    if (l->count < BAND_MIN) render_list_replay(l, t->clip_top, t->clip_bottom);
    else bands_rows(t->clip_top, t->clip_bottom, replay_band, l);
    l->count = 0;
}
