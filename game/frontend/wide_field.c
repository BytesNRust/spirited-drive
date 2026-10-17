#include "game/frontend/wide_field.h"

#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, BACKSPACE = 8, DELETE = 0x5300, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };

/* until no key is pending and no button held: the music served, the buttons polled while the pointer is active */
static void settle(WideFieldState *st, const WideFieldOps *o)
{
    while (o->keys_pending(o->ctx) || st->buttons != 0) {
        o->music(o->ctx);
        if (st->pointer_active) st->buttons = (int16_t)o->pointer_poll(o->ctx);
    }
}

/* The box redrawn, the music served between the steps: the screen under it, the text at x, shown at left, top. */
static void put(const WideFieldOps *o, int32_t box, int32_t under, const char *text, int32_t x, int32_t left,
                int32_t top)
{
    o->select(o->ctx, box);
    o->music(o->ctx);
    o->blit(o->ctx, o->picture_shape(o->ctx, under), 0, 0);
    o->music(o->ctx);
    o->text_shadowed(o->ctx, text, x, 0);
    o->music(o->ctx);
    o->select_default(o->ctx);
    o->pointer_hide(o->ctx);
    o->music(o->ctx);
    o->show(o->ctx, o->picture_shape(o->ctx, box), left, top);
    o->music(o->ctx);
    o->unlock(o->ctx);
    o->flip(o->ctx);
}

/* the cursor text: the typed one and a '_' (004b16a0) */
static void cursor_of(char *cursor, const char *typed)
{
    strcpy(cursor, typed);
    strcat(cursor, "_");
}

/* CP437's ü ä ö ß µ Ä Ü Ö ° §, in the order 00461f90 tests them */
static int umlaut(int16_t k)
{
    return k == 0x81 || k == 0x84 || k == 0x94 || k == 0xe1 || k == 0xe6 || k == 0x8e || k == 0x9a || k == 0x99 ||
           k == 0xf8 || k == 0x15;
}

/* 00461f90 */
int32_t wide_field_edit(char *text, int32_t max, int32_t x, int32_t y, int32_t free_text, int german,
                        WideFieldState *st, const WideFieldOps *ops)
{
    enum { H = 0x23, HALF_WIDE = 0x140, FULL = 0x262, BLINK = 0xf, STYLE = 1, ERASED = 0x2d };
    char typed[WIDE_FIELD_TYPED] = { 0 };
    char cursor[WIDE_FIELD_TYPED + 2];
    const int16_t most = (int16_t)max;
    const int16_t left = (int16_t)(x - HALF_WIDE);
    const int32_t top = (int16_t)y;
    ops->drain(ops->ctx);
    if (st->pointer_active) st->buttons = (int16_t)ops->pointer_poll(ops->ctx);
    settle(st, ops);
    if (german) ops->layout_on(ops->ctx);
    const int32_t width = (int16_t)(x + HALF_WIDE) - left;
    const int16_t half = (int16_t)(width / 2);
    const int32_t box = ops->picture_new(ops->ctx, width, H, 0);
    const int32_t under = ops->picture_new(ops->ctx, width, H, 0);
    int16_t n = (int16_t)strlen(text);
    strncpy(typed, text, WIDE_FIELD_KEEP);
    cursor_of(cursor, typed);
    ops->text_select(ops->ctx, STYLE);
    ops->select_default(ops->ctx);
    ops->grab(ops->ctx, ops->picture_shape(ops->ctx, under), left, top);
    ops->unlock(ops->ctx);
    ops->flip(ops->ctx);
    ops->select(ops->ctx, box);
    ops->blit(ops->ctx, ops->picture_shape(ops->ctx, under), 0, 0);
    int16_t w = (int16_t)ops->text_width(ops->ctx, typed);
    ops->text_shadowed(ops->ctx, typed, half - w / 2, 0);
    ops->select_default(ops->ctx);
    ops->show(ops->ctx, ops->picture_shape(ops->ctx, box), left, top);
    ops->flip(ops->ctx);
    settle(st, ops);
    ops->drain(ops->ctx);
    int32_t sum = 0;
    int blink = 0;
    int16_t k;
    for (;;) {
        st->buttons = (int16_t)ops->pointer_poll(ops->ctx);
        ops->music(ops->ctx);
        ops->joystick_poll(ops->ctx);
        k = (int16_t)ops->key_read(ops->ctx);
        if (k == 0) {
            if (st->buttons == LEFT_BUTTON || st->joystick_key != 0) k = ENTER;
            else if (st->buttons == RIGHT_BUTTON) k = ESCAPE;
        } else if (german && (k == SPACE || k == BACKSPACE)) {
            ops->key_read(ops->ctx);
        }
        if (k == 0) {
            sum += ops->ticks(ops->ctx);
            if ((int16_t)sum < BLINK) continue;
            blink ^= 1;
            put(ops, box, under, blink ? cursor : typed, half - w / 2, left, top);
            sum = 0;
            continue;
        }
        if (k == ENTER || k == ESCAPE) break;
        if ((k == SPACE || k == '.' || k == '\\' || k == '/') && !(int16_t)free_text) {
            /* refused */
        } else if (k == BACKSPACE && n > 0) {
            typed[--n] = 0;
        } else if (k == DELETE && n > 0) {
            n = 0;
            memset(typed, 0, ERASED);
        } else if (n < most && ops->text_width(ops->ctx, typed) < FULL && k > 0x1f && k < 0x7f) {
            typed[n++] = (char)k;
        } else if (german && n < most && ops->text_width(ops->ctx, typed) < FULL && umlaut(k)) {
            typed[n++] = (char)k;
        }
        w = (int16_t)ops->text_width(ops->ctx, typed);
        cursor_of(cursor, typed);
        put(ops, box, under, typed, half - w / 2, left, top);
        ops->ticks(ops->ctx);
    }
    ops->select_default(ops->ctx);
    if (k == ENTER) {
        typed[WIDE_FIELD_KEEP - 1] = 0;
        strncpy(text, typed, WIDE_FIELD_KEEP);
    }
    ops->pointer_hide(ops->ctx);
    ops->blit(ops->ctx, ops->picture_shape(ops->ctx, under), left, top);
    ops->unlock(ops->ctx);
    ops->flip(ops->ctx);
    ops->picture_free(ops->ctx, box);
    ops->picture_free(ops->ctx, under);
    if (german) ops->layout_off(ops->ctx);
    return k == ENTER;
}
