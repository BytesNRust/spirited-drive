#include "game/frontend/yes_no_box.h"

enum {
    BOX_X = 0x96, BOX_Y = 0xbe, BOX_W = 0x154, BOX_H = 0x64, SHADE = 0x9000, RED = 0xff0000,
    TITLE_STYLE = 2, TITLE_Y = 0x14, LABEL_Y = 0x3c, OPTION_Y = 0xfa, YES = 0, NO = 1,
    SOUND_MOVE = 0x47, SOUND_SELECT = 0x48, BUTTON_RIGHT = 2,
    KEY_SPACE = 0x20, KEY_ENTER = 0xd, KEY_Y = 0x15, KEY_J = 0x24, KEY_N = 0x31, KEY_ESCAPE = 0x1b,
    KEY_LEFT = 0x4b00, KEY_RIGHT = 0x4d00
};

static const int16_t POINTER_X[2] = { 225, 420 };  /* 00461f4c */
static const int16_t POINTER_Y[2] = { 260, 260 };  /* 00461f50 */
static const int32_t LABEL_X[2] = { 0x3c, 0xfa };    /* in the box */
static const int32_t OPTION_X[2] = { 0xd2, 0x190 };  /* on the screen */

static const char *label(int german, int option)
{
    if (option == YES) return german ? "Ja" : "Yes";    /* 004b1818 / 004b181c */
    return german ? "Nein" : "No";                       /* 004b1820 / 004b1828 */
}

/* the pointer over an option: x in [x0, x1], y in [0xf5, 0x118] */
static int over(const DialogPointer *p, int32_t x0, int32_t x1)
{
    return p->x >= x0 && p->x <= x1 && p->y >= 0xf5 && p->y <= 0x118;
}

static void pointer_to(DialogPointer *p, int option, const YesNoOps *ops)
{
    ops->pointer_warp(ops->ctx, POINTER_X[option], POINTER_Y[option]);
    p->x = POINTER_X[option], p->y = POINTER_Y[option];
    ops->pointer_mark(ops->ctx, p);
}

/* the box's picture on the screen, then the screen picture at its own place */
static void show(const MessageBannerInputs *in, int32_t picture, const YesNoOps *ops)
{
    const MessageBannerOps *d = ops->draw;
    ops->pointer_hide(ops->ctx);
    d->select(d->ctx, in->screen);
    d->blit(d->ctx, d->picture_shape(d->ctx, picture), BOX_X, BOX_Y);
    d->select_default(d->ctx);
    d->blit_own(d->ctx, d->picture_shape(d->ctx, in->screen));
    d->flip(d->ctx);
    ops->pointer_show(ops->ctx);
}

static void draw_box(const char *title, int german, const MessageBannerInputs *in, int32_t box, uint8_t *shade,
                     const YesNoOps *ops)
{
    const MessageBannerOps *d = ops->draw;
    d->select(d->ctx, box);
    d->shade_set(d->ctx, shade);
    static const int32_t corners[8] = { 0, 0, BOX_W - 1, 0, BOX_W - 1, BOX_H - 1, 0, BOX_H - 1 };
    d->shade_polygon(d->ctx, 4, corners);
    d->frame(d->ctx, 2, 2, BOX_W - 3, BOX_H - 3, d->colour(d->ctx, RED));
    d->frame(d->ctx, 3, 3, BOX_W - 4, BOX_H - 4, d->colour(d->ctx, RED));
    d->text_styles(d->ctx);
    d->text_select(d->ctx, TITLE_STYLE);
    d->text_centred(d->ctx, title, BOX_W / 2, TITLE_Y);
    for (int option = YES; option <= NO; option++) {
        d->text_select(d->ctx, option == NO);
        ops->text_shadowed(ops->ctx, label(german, option), LABEL_X[option], LABEL_Y);
    }
    show(in, box, ops);
}

/* style 1 is the selection */
static void draw_options(int german, int selected, const YesNoOps *ops)
{
    const MessageBannerOps *d = ops->draw;
    ops->sound(ops->ctx, SOUND_MOVE);
    ops->pointer_hide(ops->ctx);
    for (int option = YES; option <= NO; option++) {
        d->text_select(d->ctx, option == selected);
        ops->text_boxed(ops->ctx, label(german, option), OPTION_X[option], OPTION_Y);
    }
    ops->pointer_show(ops->ctx);
}

/* 00465f4c */
int yes_no_box(const char *title, int german, DialogPointer *p, const MessageBannerInputs *in, const YesNoOps *ops)
{
    const MessageBannerOps *d = ops->draw;
    int selected = NO, redraw = 1, keys_armed = 0, buttons_armed = 0, result = 0, done = 0;
    const int32_t saved = d->picture_new(d->ctx, BOX_W, BOX_H, 0);
    const int32_t box = d->picture_new(d->ctx, BOX_W, BOX_H, 0);
    uint8_t shade[MESSAGE_BANNER_SHADE];
    d->shade_table(d->ctx, SHADE, SHADE, SHADE, in->palette, shade);
    ops->pointer_hide(ops->ctx);
    d->select(d->ctx, in->screen);
    d->grab(d->ctx, d->picture_shape(d->ctx, saved), BOX_X, BOX_Y);
    d->grab(d->ctx, d->picture_shape(d->ctx, box), BOX_X, BOX_Y);
    if (p->active) ops->pointer_buttons(ops->ctx, p);  /* its answer dropped */
    else pointer_to(p, NO, ops);
    draw_box(title, german, in, box, shade, ops);
    while (ops->key(ops->ctx) != 0) {
        ops->music_service(ops->ctx);
        ops->pointer_buttons(ops->ctx, p);
    }
    ops->drain(ops->ctx);
    do {
        if (over(p, 0xbe, 0x104)) {
            if (selected != YES) redraw = 1, selected = YES;
        } else if (over(p, 0x17c, 0x1cc) && selected != NO) {
            selected = NO, redraw = 1;
        }
        if (redraw) {
            draw_options(german, selected, ops);
            redraw = 0;
        }
        if (p->active) p->buttons = (int16_t)ops->pointer_buttons(ops->ctx, p);
        if (!p->buttons) buttons_armed = 1;
        if (!p->active && (p->buttons || p->mark_x != p->x || p->mark_y != p->y)) p->active = 1;
        ops->music_service(ops->ctx);
        const uint16_t key = (uint16_t)ops->key(ops->ctx);
        if (key == KEY_SPACE || key == KEY_ENTER || (p->buttons && buttons_armed)) {
            ops->pointer_click(ops->ctx);
            if (keys_armed) {
                ops->sound(ops->ctx, SOUND_SELECT);
                if (p->buttons != BUTTON_RIGHT && selected == YES) result = 1;
                done = 1;
            }
        } else if (key != 0 && keys_armed) {
            if ((key == KEY_Y && !german) || (key == KEY_J && german)) {
                ops->sound(ops->ctx, SOUND_SELECT);
                result = 1, done = 1;
            } else if (key == KEY_N || key == KEY_ESCAPE) {
                ops->sound(ops->ctx, SOUND_SELECT);
                done = 1;
            } else if (key == KEY_LEFT || key == KEY_RIGHT) {
                if (p->active) {
                    ops->pointer_poll(ops->ctx, p);
                    ops->pointer_hide(ops->ctx);
                    p->active = 0;
                }
                selected ^= 1;
                pointer_to(p, selected, ops);
                redraw = 1;
            }
            keys_armed = 0;
        } else if (key == 0) {
            keys_armed = 1;
        }
    } while (!done);
    show(in, saved, ops);
    d->picture_free(d->ctx, saved);
    d->picture_free(d->ctx, box);
    return result;
}
