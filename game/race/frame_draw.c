/* The race's frame draw (frame_draw.h), as 0044cc74 runs it. */
#include "game/race/frame_draw.h"

#include "game/race/mirror_scene.h"
#include "game/race/race_state.h"
#include "game/gfx/x86_int.h"

enum {
    NET_EVERYONE = -1, NET_READY = 8,
    RATE_SLACK = 1,
    FILL_WHITE = 0xff,
    MONO_ATTRIBUTE = 0x0f00, MONO_SECOND_LINE = 0x50,
    TEXT_LETTER_BASE = 0x41, TEXT_RAISE = 0x10,
    SCREEN_INTERLACED = 3,
    CHANGED_F9 = 999,
    RATE_EVERY = 100, RATE_TENTHS = 1000, RATE_WHOLE = 100, RATE_TENTH = 10, RATE_MINUTE = 6000
};

/* 0044ccb6..0044cd1f: the first frame of a race. In a network game (004fb7d4
 * 1, 00524e9f 0) everyone is told and waited for; a failed wait drops it. */
static void restart(const FrameDrawOps *ops, FrameDraw *d)
{
    d->restart = 0;
    d->changed = 1;
    ops->mirror_create(ops->ctx);
    d->last_time = d->run_frames = d->rate_frames = d->run_time = d->rate_elapsed = 0;
    if (d->net_start != 1 || d->laps_low != 0) return;
    d->unknown_4fcdac = d->waiting = 1;
    ops->net_send(ops->ctx, NET_EVERYONE);
    if (ops->net_wait(ops->ctx, NET_READY) == 0) d->dropped = 1;
    d->unknown_4fcdac = d->waiting = 0;
}

/* 0044cd29..0044cd74: the draw distance asked for taken; with 005104cc,
 * stepped by one towards the rate's target when it is more than one off. */
static void detail(const FrameDrawOps *ops, FrameDraw *d)
{
    if (d->pending_segments != -1) {
        d->draw_segments = d->pending_segments;
        ops->restream(ops->ctx);
        d->pending_segments = -1;
    }
    if (d->auto_detail == 0) return;
    const int32_t off = x_sub(d->rate.rate, d->rate_target);
    if (off > RATE_SLACK)
        race_detail_step(1, d->draw_segments, &d->pending_segments);
    else if (off < -RATE_SLACK)
        race_detail_step(-1, d->draw_segments, &d->pending_segments);
}

/* 0044cda5..0044ce06: a view asked for last frame taken at the window's size
 * without counting as a change; a change takes the view at size 0 and
 * redraws the HUD's tiles, and asks for the view again next frame. */
static void choose_view(const FrameDrawOps *ops, FrameDraw *d)
{
    if (d->view_pending != 0) {
        const int32_t changed = d->changed;
        ops->view_choose(ops->ctx, d->chase->camera->view, d->window_size);
        d->changed = changed;
        d->view_pending = 0;
    }
    if (d->changed == 0) return;
    ops->view_choose(ops->ctx, d->chase->camera->view, 0);
    ops->tiles(ops->ctx);
    d->view_pending = 1;
}

/* 0044ce4b..0044cee2: the mirror (mirror_frame) */
static void mirror(const FrameDrawOps *ops, FrameDraw *d)
{
    const MirrorFrameOps mops = { ops->ctx, ops->view, ops->mirror_transform, ops->mirror_select, ops->mirror_sky,
                                  ops->mirror_scene, ops->mirror_flip };
    const MirrorFrameInputs in = { d->chase->unknown_68, d->unknown_4b91c8, d->alternate, d->mirror_count };
    mirror_frame(&mops, &in, &d->mirror_state);
}

/* The clip to the camera's layout box (+0x18 left, +0x1c top, +0x20 wide,
 * +0x24 high). */
static void clip_view(const FrameDrawOps *ops, const FrameDraw *d)
{
    const ViewLayout *box = d->camera->layout_row;
    ops->clip(ops->ctx, box->left, x_add(box->unknown_20, box->left), box->top, x_add(box->unknown_24, box->top));
}

/* 0044cf97 / 0044cfc9: the text as words with attribute 0x0f, up to its
 * NUL (port guard: within its 0xa0 bytes and the screen). */
static void mono_line(uint16_t *screen, int32_t at, const char *text)
{
    if (screen == NULL) return;  /* port guard: no monochrome screen */
    for (int32_t i = 0; i < FRAME_TEXT && text[i] != 0 && at + i < FRAME_MONO_WORDS; i++)
        screen[at + i] = (uint16_t)(MONO_ATTRIBUTE | (uint8_t)text[i]);
}

/* 0044cf74..0044d020: the texts on the monochrome screen; the debug line
 * (the camera's node and kind as a letter) 16 rows above the bottom. */
static void texts(const FrameDrawOps *ops, const FrameDraw *d)
{
    if (d->mono != 0) mono_line(d->mono_screen, 0, d->rate_text);
    if (d->mono != 0) mono_line(d->mono_screen, MONO_SECOND_LINE, d->text_2);
    if (d->debug_text == 0) return;
    ops->select_front(ops->ctx);
    ops->text_colour(ops->ctx, d->text_colour, 0);
    ops->text(ops->ctx, 0, x_sub(d->height, TEXT_RAISE), d->chase->node, x_add(d->chase->kind, TEXT_LETTER_BASE));
}

static Car *car_at(const FrameDraw *d, int32_t slot)
{
    return d->cars[race_slot(slot)];  /* port guard */
}

/* 0044d03c..0044d111: in the car (kind 0, 004f9b7c 0) the cockpit, its
 * request cleared when the last frame was not; else, the screen kind 3 with
 * a view asked for, every other row from 1 drawn in the line colour, and
 * the HUD. */
static void panels(const FrameDrawOps *ops, FrameDraw *d)
{
    if (d->chase->kind == 0 && d->unknown_4f9b7c == 0) {
        if (d->in_car == 0) ops->hud_clear(ops->ctx);
        ops->cockpit(ops->ctx, car_at(d, d->followed));
        d->in_car = 1;
        return;
    }
    if (d->view_pending != 0 && d->screen_kind == SCREEN_INTERLACED) {
        ops->select(ops->ctx, HUD_SURFACE_VIEW);
        clip_view(ops, d);
        for (int32_t y = 1; y < d->height; y += 2) ops->line(ops->ctx, 0, y, d->width, y, d->line_colour);
    }
    ops->hud(ops->ctx, car_at(d, d->viewed), d->changed);
    d->in_car = 0;
}

/* 0044d17e..0044d240: the frame counted and timed (0044c150); every 100
 * of time or more the rate text ("%2ld.%1ld") and frames a minute made,
 * all in unsigned 32 bits, and the counts started again. */
static void count(const FrameDrawOps *ops, FrameDraw *d)
{
    d->run_frames = x_add(d->run_frames, 1);
    d->rate_frames = x_add(d->rate_frames, 1);
    d->last_time = ops->time(ops->ctx);
    race_frame_rate(&d->rate, d->last_time);
    d->run_time = x_add(d->run_time, d->last_time);
    d->rate_elapsed = x_add(d->rate_elapsed, d->last_time);
    const uint32_t elapsed = (uint32_t)d->rate_elapsed, frames = (uint32_t)d->rate_frames;
    if (elapsed < RATE_EVERY) return;
    const uint32_t tenths = frames * RATE_TENTHS / elapsed;
    ops->format_rate(ops->ctx, d->rate_text, (int32_t)(frames * RATE_WHOLE / elapsed), (int32_t)(tenths % RATE_TENTH));
    d->per_minute = (int32_t)(frames * RATE_MINUTE / elapsed);
    d->rate_elapsed = d->rate_frames = 0;
}

void frame_draw(const FrameDrawOps *ops, FrameDraw *d)
{
    if (d->unknown_513e60 != 0 || d->restart != 0) ops->chase_tick(ops->ctx);
    if (d->restart != 0) restart(ops, d);
    detail(ops, d);
    ops->stream(ops->ctx);
    ops->view(ops->ctx, 0);
    ops->service(ops->ctx);
    ops->select(ops->ctx, HUD_SURFACE_VIEW);
    choose_view(ops, d);
    ops->service(ops->ctx);
    ops->matrix(ops->ctx);
    ops->transform(ops->ctx);
    ops->service(ops->ctx);
    ops->rows(ops->ctx, d->chase->node);
    ops->ahead(ops->ctx, d->chase->node);
    ops->service(ops->ctx);
    mirror(ops, d);
    ops->objects_from_last(ops->ctx);
    ops->service(ops->ctx);
    d->unknown_511fc4 = 1;
    ops->select(ops->ctx, HUD_SURFACE_VIEW);
    clip_view(ops, d);
    ops->horizon(ops->ctx);
    if (d->unknown_511fbc != 0) ops->fill(ops->ctx, FILL_WHITE);
    ops->near(ops->ctx, d->draw_segments, d->chase->node);
    ops->map(ops->ctx);
    d->unknown_4c2d8c = 1;
    texts(ops, d);
    if (d->changed != 0 && d->screen_kind == SCREEN_INTERLACED) d->clear_view = 1;
    panels(ops, d);
    if (d->back_buffer != 0) {
        ops->select_front(ops->ctx);
        ops->show(ops->ctx, HUD_SURFACE_SCREEN);
        ops->video_flip(ops->ctx);
    } else {
        ops->present(ops->ctx);
    }
    ops->service(ops->ctx);
    d->changed = d->changed == CHANGED_F9;
    if (d->unknown_51205c == 0) d->unknown_512010 = 1;
    count(ops, d);
    if (d->music_on != 0) ops->music(ops->ctx);
    ops->service(ops->ctx);
    ops->time(ops->ctx);
}
