/* The in-car dashboard's frame (0042270c). */
#include "game/race/cockpit.h"

#include "game/gfx/x86_int.h"

enum {
    SHAKE_LANDING = -0x8000, SHAKE_SPEED = 0x10000, SHAKE_RANGE = 0x80, SHAKE_MIDDLE = 0x40,
    NOTICE_TICK = -1000,
    SCREEN_KIND_STRIP = 3, RUNS_STRIP = 2, RUNS_STRIP_AFTER = 3,
    BOX_W_HIGH = 0x206, BOX_W_LOW = 0x102,
    BOX_H_LIVE_HIGH = 0x1e0, BOX_H_LIVE_LOW = 0xc8, BOX_H_HIGH = 0x1c0, BOX_H_LOW = 0xb4,
    MODEL_SKIP_SPED = 0xb,
    MAP_X_HIGH = 0x206, MAP_Y_HIGH = 0x130, MAP_X_LOW = 0x103, MAP_Y_LOW = 0x7e,
    FRAMES_W_LEFT = 0, FRAMES_W_RIGHT = 1, FRAMES_T_LEFT = 2, FRAMES_S_LEFT = 3, FRAMES_T_RIGHT = 4, FRAMES_S_RIGHT = 5
};

/* x86's neg for the negative ones: INT32_MIN stays negative. */
static int32_t x_abs(int32_t v)
{
    return v > 0 ? v : x_sub(0, v);
}

static int32_t steer_frame(int32_t a, int32_t high)
{
    if (high != 0) {
        static const int32_t edges[] = { 0x21999, 0x70000, 0xc8000, 0x133333, 0x1b0000 };
        int32_t i = 0;
        while (i < 5 && a >= edges[i]) i++;
        return i;
    }
    if (a < 0x74ccc) return a < 0x26666 ? 0 : a < 0x66666 ? 1 : a < 0xa9999 ? 2 : 3;
    return a < 0x154ccc ? 4 : a < 0x1c6666 ? 5 : 6;
}

static void select_screen(const CockpitFrameOps *ops, const CockpitFrameInputs *in)
{
    ops->select(ops->ctx, in->back_buffer != 0 ? HUD_SURFACE_SCREEN : HUD_SURFACE_DEFAULT);
}

static void shake(const CockpitFrameOps *ops, RaceState *race, const Car *car)
{
    if (race->landing_shake > 1) {
        race->camera_pitch = SHAKE_LANDING;
        return;
    }
    race->camera_pitch = 0;
    if (car->off_road != 0 && x_abs(car->local_velocity_z) > SHAKE_SPEED)
        race->camera_pitch = x_sub(x_shl(ops->rand_below(ops->ctx, SHAKE_RANGE), 8), SHAKE_MIDDLE);
}

/* The view through the .fmm's run lists. */
static void view_runs(const CockpitFrameOps *ops, const Cockpit *c, const CockpitFrameWords *w,
                      const CockpitFrameInputs *in)
{
    if (in->screen_kind == SCREEN_KIND_STRIP) {
        if (*w->clear_view != 0) {
            ops->select(ops->ctx, HUD_SURFACE_VIEW);
            ops->clip(ops->ctx, 0, in->width, 0, in->height);
            ops->fill(ops->ctx, in->strip_colour);
            select_screen(ops, in);
            ops->runs(ops->ctx, c->fmm_words[RUNS_STRIP]);
            *w->clear_view = 0;
        }
        ops->runs(ops->ctx, c->fmm_words[RUNS_STRIP_AFTER]);
    } else if (in->window_size >= 0 && in->window_size < COCKPIT_FMM_WORDS) {
        ops->runs(ops->ctx, c->fmm_words[in->window_size]);
    }
}

static void mirror(const CockpitFrameOps *ops, const Cockpit *c, const CockpitFrameInputs *in)
{
    if (in->back_buffer == 0) ops->restore(ops->ctx), ops->select(ops->ctx, HUD_SURFACE_DEFAULT);
    ops->mirror(ops->ctx, c->dash_points[0].x, c->dash_points[0].y);
    if (in->back_buffer == 0) ops->restore(ops->ctx), ops->select(ops->ctx, HUD_SURFACE_DEFAULT);
    const int32_t high = in->high != 0;
    const int32_t h = in->unknown_4c5390 == 1 ? (high ? BOX_H_LIVE_HIGH : BOX_H_LIVE_LOW) : (high ? BOX_H_HIGH : BOX_H_LOW);
    ops->clip_box(ops->ctx, 0, 0, high ? BOX_W_HIGH : BOX_W_LOW, h);
}

/* 'tach', 'sped', the needles and the 't' / 's' frames on the view surface,
 * then the two gauge boxes of it on the screen. */
static void gauges(const CockpitFrameOps *ops, const Cockpit *c, const CockpitFrameInputs *in, int left, int32_t frame)
{
    ops->select(ops->ctx, HUD_SURFACE_VIEW);
    ops->clip(ops->ctx, 0, in->width, 0, in->height);
    ops->place(ops->ctx, c->tach);
    if (in->model == MODEL_SKIP_SPED) ops->place_skip(ops->ctx, c->sped);
    else ops->place(ops->ctx, c->sped);
    ops->gauges(ops->ctx);
    ops->place_skip(ops->ctx, c->frames[left ? FRAMES_T_LEFT : FRAMES_T_RIGHT][frame]);
    ops->place_skip(ops->ctx, c->frames[left ? FRAMES_S_LEFT : FRAMES_S_RIGHT][frame]);
    select_screen(ops, in);
    ops->clip(ops->ctx, c->tach_x, x_add(c->tach_x, c->tach_w), c->tach_y, x_add(c->tach_y, c->tach_h));
    ops->present(ops->ctx);
    ops->clip(ops->ctx, c->sped_x, x_add(c->sped_x, c->sped_w), c->sped_y, x_add(c->sped_y, c->sped_h));
    ops->present(ops->ctx);
}

void cockpit_frame(const CockpitFrameOps *ops, HudState *hud, RaceState *race, const Cockpit *c, const Car *car,
                   const CockpitFrameWords *w, const CockpitFrameInputs *in)
{
    if (in->unknown_509d70 != 0) return;
    shake(ops, race, car);
    if (race->stop_notice[0] != 0 || race->stop_notice[1] != 0) {
        ops->police(ops->ctx);
        hud->unknown_4f9a20[1] = 1;
        ops->replay(ops->ctx);
        return;
    }
    *w->loading = 1;
    ops->reload(ops->ctx);
    *w->loading = 0;
    race->stop_notice_tick = NOTICE_TICK;
    if (hud->unknown_4f9a20[1] != 0 || hud->unknown_4f9a20[2] != 0) {
        ops->redraw(ops->ctx);
        hud->unknown_4f9a20[1] = 0, hud->unknown_4f9a20[2] = 0;
    }
    const int32_t steer = car->steer;
    const int32_t frame = steer_frame(x_abs(steer), in->high);
    select_screen(ops, in);
    view_runs(ops, c, w, in);
    *w->unknown_4b956c = in->unknown_4b9570;
    mirror(ops, c, in);
    const int32_t shown = steer < 0 ? -frame : frame;
    const int left = (steer < 0) != (race->mirrored != 0);
    if (shown != hud->unknown_4f9a20[0]) ops->draw(ops->ctx, c->frames[left ? FRAMES_W_LEFT : FRAMES_W_RIGHT][frame]);
    hud->unknown_4f9a20[0] = shown;
    gauges(ops, c, in, left, frame);
    ops->gate(ops->ctx);
    ops->lights(ops->ctx);
    ops->keys(ops->ctx);
    ops->top_row(ops->ctx);
    if (in->use_parts != 0)
        ops->map(ops->ctx, in->high != 0 ? MAP_X_HIGH : MAP_X_LOW, in->high != 0 ? MAP_Y_HIGH : MAP_Y_LOW);
    ops->replay(ops->ctx);
}
