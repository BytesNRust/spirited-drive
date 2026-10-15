/* --drive's control panels (drive_panel.h). */
#include "viewer/drive_panel.h"

#include "game/gfx/blit.h"
#include "game/gfx/clip.h"
#include "game/gfx/flat.h"
#include "game/race/dash_controls.h"

enum { HIGH_RES = 1, ENGLISH = 0, START_VIEW = 3, NAME_STRIDE = 0x95 };

typedef struct {
    DrivePanel *p;
    const DrivePanelDraw *d;
    QuadTarget t;               /* the frame, its clip the panel's own */
} PanelCalls;

static void rect(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t colour)
{
    flat_fill_rect(&((PanelCalls *)ctx)->t, x, y, w, h, colour);  /* 004829b0 */
}

static void blit(void *ctx, const uint8_t *shape, int32_t x, int32_t y)
{
    if (shape != NULL) blit_shape_at(&((PanelCalls *)ctx)->t, shape, x, y);  /* 00473764 / 004737d0 */
}

static void blit_own(void *ctx, const uint8_t *shape)
{
    if (shape != NULL) blit_shape_placed(&((PanelCalls *)ctx)->t, shape);  /* 004737ec */
}

static void clip(void *ctx, int32_t left, int32_t right, int32_t top, int32_t bottom)
{
    PanelCalls *c = ctx;
    clip_set(&c->t, left, right, top, bottom, c->d->surface_w, c->d->surface_h);  /* 0047383c */
}

/* 00441938 / 004418a4 / [004c6194] / 00463f80: no mouse */
static void no_pointer(void *ctx) { (void)ctx; }
static void no_pointer_show(void *ctx, int32_t pointer) { (void)ctx, (void)pointer; }
static int32_t no_buttons(void *ctx) { return (void)ctx, 0; }
static int32_t never_inside(void *ctx, int16_t l, int16_t r, int16_t t, int16_t b)
{
    return (void)ctx, (void)l, (void)r, (void)t, (void)b, 0;
}
/* 00473680(004c2da0) / 004735dc: the frame either way */
static void select_frame(void *ctx, HudSurface s) { (void)ctx, (void)s; }

static ReplayPanelDrawOps draw_ops(PanelCalls *c) { return (ReplayPanelDrawOps){ c, rect, blit, blit_own, blit }; }

void drive_panel_start(DrivePanel *p)
{
    *p = (DrivePanel){ .panel = { .view = START_VIEW, .cursor = START_VIEW, .lit = 1, .last = START_VIEW } };
}

static void dash_buttons(void *ctx)
{
    PanelCalls *c = ctx;
    const ReplayPanelDrawOps ops = draw_ops(c);
    dash_controls_buttons(c->d->dash, HIGH_RES, ENGLISH, &ops, never_inside);
}

static void dash_time(void *ctx, int32_t ticks, int32_t x, int32_t y, int32_t step, int32_t space)
{
    PanelCalls *c = ctx;
    hud_draw_time(&c->t, c->d->hud, c->d->dash->grey_shown, ticks, x, y, step, space);  /* 0041e794 */
}

void drive_panel_dash(DrivePanel *p, const DrivePanelDraw *d)
{
    PanelCalls c = { p, d, *d->screen };
    int32_t *w = d->dash->unknown_513888;
    RaceReplayDirector *dir = d->director;
    if (dir != NULL) w[DASH_PRESSED] = dir->unknown_513888, w[DASH_SPEED] = dir->unknown_513894;
    w[DASH_REDRAW] = 1;  /* the frame is drawn whole each time here: the buttons with it (the original keeps them) */
    const DashControlsFrameInputs in = { HIGH_RES, d->race->tick, dir != NULL ? dir->end_clock : 0, d->played, d->rest };
    const DashControlsFrameOps ops = { &c, no_pointer, no_buttons, clip, dash_buttons, dash_time, rect, no_pointer_show };
    dash_controls_frame(w, &p->dash_drawn_tick, &in, &ops);
    if (dir != NULL) dir->unknown_513898 = w[DASH_REDRAW];
}

static void replay_marks(void *ctx)
{
    PanelCalls *c = ctx;
    const ReplayPanelMarks in = { { c->d->replay->button_up[3], c->d->replay->button_up[4], c->d->replay->button_up[5],
                                    c->d->replay->button_up[6], c->d->replay->button_up[7], c->d->replay->button_up[8],
                                    c->d->replay->button_up[9], c->d->replay->button_up[10] },
                                  HIGH_RES, c->p->panel.view, c->d->car_colours };
    const ReplayPanelDrawOps ops = draw_ops(c);
    replay_panel_marks(&in, &ops);
}

static void replay_name(void *ctx)
{
    PanelCalls *c = ctx;
    const RaceState *race = c->d->race;
    const ReplayArt *art = c->d->replay;
    ReplayPanelName in = { art->names, { 0 }, (const char *)race->game_state + NAME_STRIDE * race->viewed, HIGH_RES,
                           race->view_taken };
    for (int k = 0; k < REPLAY_PANEL_GLYPHS; k++) in.glyphs[k] = art->glyph[k];
    const ReplayPanelDrawOps ops = draw_ops(c);
    replay_panel_name(&in, &ops);
}

static void replay_draw(void *ctx)
{
    PanelCalls *c = ctx;
    const ReplayPanelParts parts = { c, replay_marks, replay_name };
    const ReplayPanelDrawOps ops = draw_ops(c);
    replay_panel_buttons(&c->p->panel, c->d->replay, c->d->race->viewed, &ops, &parts);
}

void drive_panel_replay(DrivePanel *p, const DrivePanelDraw *d)
{
    PanelCalls c = { p, d, *d->screen };
    p->panel.whole = 1;  /* the frame is drawn whole each time here: the panel with it (the original keeps it) */
    const ReplayPanelFrameOps ops = { &c, no_pointer, no_buttons, select_frame, clip, replay_draw, no_pointer_show };
    replay_panel_frame(&p->panel, HIGH_RES, 1, &ops);
    d->race->view_taken = p->panel.whole;
}
