/* The replay's controls on the dash (dash_controls.h). */
#include "game/race/dash_controls.h"

enum {
    SHAPE_X = 0xc, SHAPE_Y = 0xe, DONE_SHIFT = 10,
    STRIP_RIGHT_HIGH = 640, STRIP_TOP_HIGH = 448, STRIP_BOTTOM_HIGH = 480,
    STRIP_RIGHT_LOW = 320, STRIP_TOP_LOW = 180, STRIP_BOTTOM_LOW = 200
};

/* The clock and the bar at each resolution */
typedef struct {
    int32_t x, y, step, space, bar_y, bar_w, bar_h;
} ClockPlace;

static const ClockPlace CLOCK_HIGH = { 0x41, 0x1c5, 13, 7, 0x1d8, 241, 6 };
static const ClockPlace CLOCK_LOW = { 0x2d, 0xba, 6, 2, 0xc3, 122, 3 };

static int32_t half_at(const uint8_t *b, int at) { return (int16_t)(b[at] | b[at + 1] << 8); }

static void placed_by(const ReplayPanelDrawOps *ops, const uint8_t *shape, int32_t dx, int32_t dy)
{
    ops->blit_at(ops->ctx, shape, half_at(shape, SHAPE_X) + dx, half_at(shape, SHAPE_Y) + dy);
}

static int32_t in_box(const DashArt *art, int32_t control, DashInside inside, void *ctx)
{
    const DashRect *b = &art->control_box[control];
    return inside(ctx, (int16_t)b->left, (int16_t)b->right, (int16_t)b->top, (int16_t)b->bottom);
}

/* The camera or speed button: taken, lit and placed one right and down;
 * under the cursor, lit while the pointer is in its box; else unlit */
static void choice(const DashArt *art, int32_t control, uint8_t *const lit, uint8_t *const unlit,
                   const ReplayPanelDrawOps *ops, DashInside inside)
{
    const int32_t *w = art->unknown_513888;
    if (w[DASH_PRESSED] == control)
        placed_by(ops, lit, 1, 1);
    else if (w[DASH_CURSOR] == control)
        ops->blit_own(ops->ctx, in_box(art, control, inside, ops->ctx) ? lit : unlit);
    else
        ops->blit_own(ops->ctx, unlit);
}

void dash_controls_buttons(const DashArt *art, int32_t high, int16_t german, const ReplayPanelDrawOps *ops,
                           DashInside inside)
{
    const int32_t *w = art->unknown_513888;
    const int low_english = high == 0 && german == 0;
    ops->blit_own(ops->ctx, art->replay);
    if (w[DASH_PRESSED] != 0) {
        if (w[DASH_PRESSED] == DASH_CONTROL_DONE && low_english)
            placed_by(ops, art->control_down[DASH_CONTROL_DONE], DONE_SHIFT, 1);
        else
            ops->blit_own(ops->ctx, art->control_down[w[DASH_PRESSED]]);
    } else if (w[DASH_CURSOR] == DASH_CONTROL_DONE) {
        if (low_english)
            placed_by(ops, art->control_lit[DASH_CONTROL_DONE], DONE_SHIFT, 0);
        else
            ops->blit_own(ops->ctx, art->control_lit[DASH_CONTROL_DONE]);
    } else if (w[DASH_CURSOR] != 0 && in_box(art, w[DASH_CURSOR], inside, ops->ctx)) {
        ops->blit_own(ops->ctx, art->control_lit[w[DASH_CURSOR]]);
    }
    choice(art, DASH_CONTROL_VIEW, art->view_lit[w[DASH_VIEW]], art->view[w[DASH_VIEW]], ops, inside);
    choice(art, DASH_CONTROL_SPEED, art->speed_lit[w[DASH_SPEED]], art->speed[w[DASH_SPEED]], ops, inside);
    if (w[DASH_CURSOR] == DASH_CONTROL_DONE) return;
    if (high != 0)
        ops->blit_own(ops->ctx, art->control[DASH_CONTROL_DONE]);
    else if (german == 0)
        placed_by(ops, art->control[DASH_CONTROL_DONE], DONE_SHIFT, 1);
}

/* The strip clipped (with the buttons on a redraw), the clock and the bar */
static void draw_clock(int with_buttons, const DashControlsFrameInputs *in, const DashControlsFrameOps *ops)
{
    const ClockPlace *c = in->high != 0 ? &CLOCK_HIGH : &CLOCK_LOW;
    if (in->high != 0)
        ops->clip(ops->ctx, 0, STRIP_RIGHT_HIGH, STRIP_TOP_HIGH, STRIP_BOTTOM_HIGH);
    else
        ops->clip(ops->ctx, 0, STRIP_RIGHT_LOW, STRIP_TOP_LOW, STRIP_BOTTOM_LOW);
    if (with_buttons) ops->controls(ops->ctx);
    ops->time(ops->ctx, in->tick, c->x, c->y, c->step, c->space);
    const int32_t played = (int32_t)((uint32_t)in->tick * (uint32_t)c->bar_w) / (in->end_clock + 1);
    ops->rect(ops->ctx, c->x, c->bar_y, played, c->bar_h, in->played);
    ops->rect(ops->ctx, c->x + played, c->bar_y, c->bar_w - played, c->bar_h, in->rest);
}

void dash_controls_frame(int32_t words[DASH_STATE_WORDS], int32_t *drawn_tick, const DashControlsFrameInputs *in,
                         const DashControlsFrameOps *ops)
{
    ops->hide(ops->ctx);
    if (words[DASH_PRESSED] == 0) (void)ops->buttons(ops->ctx);
    if (words[DASH_REDRAW] != 0) {
        *drawn_tick = in->tick;
        words[DASH_REDRAW] = 0;
        draw_clock(1, in, ops);
    } else {
        if (*drawn_tick != in->tick) draw_clock(0, in, ops);
        *drawn_tick = in->tick;
    }
    ops->show(ops->ctx, words[DASH_PRESSED] > 0);
}
