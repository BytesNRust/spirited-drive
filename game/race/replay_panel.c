/* The replay's panel (replay_panel.h). */
#include "game/race/replay_panel.h"


enum {
    BUTTON_X = 0xc, BUTTON_Y = 0xe,  /* an FSH entry's position words */
    SHAPE_W = 4, SHAPE_H = 6,        /* its width and height */
    GAP_HIGH = 10, GAP_LOW = 5, LETTERS = 10, NO_GLYPH = -1,
    MARK_X_HIGH = 4, MARK_X_LOW = 3, MARK_Y = 3,
    MARK_W_HIGH = 5, MARK_H_HIGH = 6, MARK_W_LOW = 2, MARK_H_LOW = 1,
    STRIP_RIGHT_HIGH = 640, STRIP_TOP_HIGH = 448, STRIP_BOTTOM_HIGH = 480,
    STRIP_RIGHT_LOW = 320, STRIP_TOP_LOW = 180, STRIP_BOTTOM_LOW = 200,
    VIEW_DONE = 1, VIEW_BACK = 2, VIEW_FIRST_CAR = 3, VIEW_LAST_CAR = 10, VIEW_ON = 11,
    RANGE_TOP_HIGH = 450, RANGE_TOP_LOW = 180
};

static int32_t half_at(const uint8_t *b, int at) { return (int16_t)(b[at] | b[at + 1] << 8); }

void replay_panel_marks(const ReplayPanelMarks *in, const ReplayPanelDrawOps *ops)
{
    const int32_t dx = in->high != 0 ? MARK_X_HIGH : MARK_X_LOW;
    const int32_t w = in->high != 0 ? MARK_W_HIGH : MARK_W_LOW, h = in->high != 0 ? MARK_H_HIGH : MARK_H_LOW;
    const int32_t y = half_at(in->buttons[0], BUTTON_Y) + MARK_Y;
    for (int32_t k = 0; k < REPLAY_PANEL_CARS; k++) {
        int32_t x = half_at(in->buttons[k], BUTTON_X) + dx;
        const int taken = REPLAY_PANEL_FIRST_CAR_VIEW + k == in->view;
        if (taken) x++;
        ops->rect(ops->ctx, x, taken && in->high != 0 ? y + 1 : y, w, h, in->colours[k]);
    }
}

/* A name byte's glyph, NO_GLYPH for none */
static int32_t glyph_of(uint8_t c)
{
    if (c >= '0' && c <= '9') return c - '0';
    int32_t g = (int32_t)c - 'a';
    if (g < 0) g += 'a' - 'A';
    g += LETTERS;
    return g >= 0 && g < REPLAY_PANEL_GLYPHS ? g : NO_GLYPH;
}

void replay_panel_name(const ReplayPanelName *in, const ReplayPanelDrawOps *ops)
{
    if (in->view_taken == 0) ops->blit_own(ops->ctx, in->box);
    int32_t length = 0;
    while (length <= REPLAY_PANEL_NAME && in->name[length] != 0) length++;
    if (length < 1 || length > REPLAY_PANEL_NAME) return;
    const int32_t gap = in->high != 0 ? GAP_HIGH : GAP_LOW;
    int32_t glyph[REPLAY_PANEL_NAME], width = 0;
    for (int32_t i = 0; i < length; i++) {
        glyph[i] = glyph_of((uint8_t)in->name[i]);
        width += glyph[i] == NO_GLYPH ? gap : half_at(in->glyphs[glyph[i]], SHAPE_W);
    }
    int32_t x = half_at(in->box, BUTTON_X) + (half_at(in->box, SHAPE_W) >> 1) - (width >> 1);
    for (int32_t i = 0; i < length; i++) {
        if (glyph[i] == NO_GLYPH) {
            x += gap;
            continue;
        }
        const uint8_t *g = in->glyphs[glyph[i]];
        ops->blit(ops->ctx, g, x, half_at(g, BUTTON_Y));
        x += half_at(g, SHAPE_W);
    }
}

void replay_panel_buttons(ReplayPanel *p, const ReplayArt *art, int32_t viewed, const ReplayPanelDrawOps *ops,
                          const ReplayPanelParts *parts)
{
    if (p->whole != 0) {
        p->redraw = 1;
        ops->blit_own(ops->ctx, art->replay);
        ops->blit_own(ops->ctx, art->bar);
        ops->blit_own(ops->ctx, art->button[1]);
    }
    if (p->view != 0) {
        ops->blit_own(ops->ctx, art->button_down[p->view]);
        p->cursor = p->view;
    } else if (p->cursor != p->lit || p->redraw != 0) {
        ops->blit_own(ops->ctx, art->button[p->lit]);
        p->lit = p->cursor;
        ops->blit_own(ops->ctx, art->button_up[p->cursor]);
    }
    parts->marks(parts->ctx);
    const uint8_t *lit = art->view_lit[art->camera];
    if (p->view == REPLAY_PANEL_CAMERA_VIEW)
        ops->blit_at(ops->ctx, lit, half_at(lit, BUTTON_X) + 1, half_at(lit, BUTTON_Y) + 1);
    else
        ops->blit_own(ops->ctx, p->cursor == REPLAY_PANEL_CAMERA_VIEW ? lit : art->view[art->camera]);
    if (p->driver != viewed || p->whole != 0) {
        p->driver = viewed;
        parts->name(parts->ctx);
    }
}

void replay_panel_frame(ReplayPanel *p, int32_t high, int32_t back_buffer, const ReplayPanelFrameOps *ops)
{
    ops->hide(ops->ctx);
    if (p->view == 0) (void)ops->buttons(ops->ctx);
    if (p->redraw != 0 || p->whole != 0) {
        ops->select(ops->ctx, back_buffer != 0 ? HUD_SURFACE_SCREEN : HUD_SURFACE_DEFAULT);
        if (high != 0)
            ops->clip(ops->ctx, 0, STRIP_RIGHT_HIGH, STRIP_TOP_HIGH, STRIP_BOTTOM_HIGH);
        else
            ops->clip(ops->ctx, 0, STRIP_RIGHT_LOW, STRIP_TOP_LOW, STRIP_BOTTOM_LOW);
        ops->draw(ops->ctx);
        p->redraw = 0;
        p->whole = 0;
    }
    ops->show(ops->ctx, p->view > 0);
}

/* 00471460's switch on the view taken */
static void take(ReplayPanel *p, ReplayPanelWorld *w, const ReplayPanelInputOps *ops)
{
    const int fresh = p->view != p->last;
    if (p->view == VIEW_DONE) {
        w->done = 1;
    } else if (p->view == VIEW_BACK && fresh) {
        if (--w->viewed < 0) w->viewed = w->racers - 1;
        ops->follow(ops->ctx, w->viewed);
    } else if (p->view >= VIEW_FIRST_CAR && p->view <= VIEW_LAST_CAR) {
        const int32_t car = p->view - VIEW_FIRST_CAR;
        if (car == w->viewed) return;
        w->viewed = car;
        ops->follow(ops->ctx, car);
        ops->engine(ops->ctx);
    } else if (p->view == VIEW_ON && fresh) {
        if (++w->viewed >= w->racers) w->viewed = 0;
        ops->follow(ops->ctx, w->viewed);
    } else if (p->view == REPLAY_PANEL_CAMERA_VIEW && fresh && w->viewed < w->humans) {
        ops->cycle(ops->ctx);
    }
}

void replay_panel_input(ReplayPanel *p, const ReplayArt *art, ReplayPanelWorld *w, const ReplayPanelInputOps *ops)
{
    int32_t hit = ops->mouse(ops->ctx);
    int keys = 0;
    if (hit == 0) {
        hit = ops->pick(ops->ctx);
        keys = hit != 0;
    }
    if (hit != 0) {
        take(p, w, ops);
    } else if (p->view != 0) {
        p->cursor = p->view;
        p->redraw = 1;
        p->view = 0;
    }
    p->last = p->view;
    if (p->view != 0) p->cursor = p->view;
    const uint8_t *b = art->button[p->cursor];
    if (keys && b != NULL)  /* button[0] is never loaded: the original reads through 0 there */
        ops->point(ops->ctx, art->box[p->cursor].left + (half_at(b, SHAPE_W) >> 1),
                   art->box[p->cursor].top + (half_at(b, SHAPE_H) >> 1));
}

/* A view button the mouse can take: the cars' only for the racers, the
 * camera only with a human's car shown, 'don' only in a replay */
static int mouse_can_take(int32_t k, const ReplayPanelMouseInputs *in)
{
    if (k == REPLAY_PANEL_CAMERA_VIEW && in->viewed >= in->humans) return 0;
    return k != VIEW_DONE || in->replay != 0;
}

int32_t replay_panel_mouse(ReplayPanel *p, const DashRect box[REPLAY_BUTTONS], ReplayPanelMouse *m,
                           const ReplayPanelMouseInputs *in, const ReplayPanelMouseOps *ops)
{
    int32_t hit = 0;
    if (in->high != 0)
        ops->range(ops->ctx, 0, RANGE_TOP_HIGH, STRIP_RIGHT_HIGH, STRIP_BOTTOM_HIGH);
    else
        ops->range(ops->ctx, 0, RANGE_TOP_LOW, STRIP_RIGHT_LOW, STRIP_BOTTOM_LOW);
    if (m->present != 0) m->buttons = ops->buttons(ops->ctx);
    m->held = m->buttons;
    if (m->last_x != in->x || m->last_y != in->y || m->buttons != 0) {
        for (int32_t k = 1; k < REPLAY_BUTTONS; k++) {
            if (k > VIEW_FIRST_CAR && k <= VIEW_LAST_CAR && k - VIEW_FIRST_CAR >= in->racers) {
                k = VIEW_LAST_CAR;
                continue;
            }
            if (!mouse_can_take(k, in) ||
                !ops->inside(ops->ctx, (int16_t)box[k].left, (int16_t)box[k].right, (int16_t)box[k].top, (int16_t)box[k].bottom))
                continue;
            if (m->buttons == 0 && k != p->cursor) {
                p->cursor = k, p->redraw = 1, p->view = 0;
            } else if (m->buttons != 0 && k != p->view) {
                p->view = k, p->redraw = 1, p->cursor = 0;
            }
            hit = 1;
        }
    }
    m->last_x = in->x;
    m->last_y = in->y;
    return hit;
}
