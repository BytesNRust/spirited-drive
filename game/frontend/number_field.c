#include "game/frontend/number_field.h"

#include <stdio.h>
#include <string.h>

/* The box redrawn: the screen under it, the text at x, shown at left, top;
 * the blink serves the music between the steps. */
static void put(const NumberFieldOps *o, int32_t box, int32_t under, const char *text, int32_t x, int32_t left,
                int32_t top, int served)
{
    o->select(o->ctx, box);
    if (served) o->music(o->ctx);
    o->blit(o->ctx, o->picture_shape(o->ctx, under), 0, 0);
    if (served) o->music(o->ctx);
    o->text_shadowed(o->ctx, text, x, 0);
    if (served) o->music(o->ctx);
    o->select_default(o->ctx);
    o->pointer_hide(o->ctx);
    if (served) o->music(o->ctx);
    o->show(o->ctx, o->picture_shape(o->ctx, box), left, top);
    if (served) o->music(o->ctx);
    o->unlock(o->ctx);
    o->flip(o->ctx);
}

static int is_sign(int16_t k)
{
    return k == '-' || k == '(' || k == ')' || k == '+' || k == '/' || k == ',' || k == '#' || k == '!';
}

/* 004632b8 */
int32_t number_field_edit(char *text, int32_t max, int32_t x, int32_t y, int german, const NumberFieldOps *ops)
{
    enum { H = 0x23, STEP = 30, MARGIN = 5, BLINK = 0xf, ENTER = 0xd, ESCAPE = 0x1b, DELETE = 0x5300,
           BACKSPACE = 8, SPACE = 0x20 };
    char typed[NUMBER_FIELD_TYPED] = { 0 };
    char cursor[NUMBER_FIELD_TYPED + 2];
    const int16_t most = (int16_t)max;
    const int32_t span = most / 2 * STEP;
    const int16_t left = (int16_t)((int16_t)x - span - MARGIN);
    const int32_t width = (int16_t)((int16_t)x + span + MARGIN) - left;
    const int16_t half = (int16_t)(width / 2);
    const int32_t top = (int16_t)y;
    const int32_t box = ops->picture_new(ops->ctx, width, H, 0);
    const int32_t under = ops->picture_new(ops->ctx, width, H, 0);
    int16_t n = (int16_t)strlen(text);
    memcpy(typed, text, (size_t)n + 1);
    snprintf(cursor, sizeof cursor, "%s_", typed);
    ops->select_default(ops->ctx);
    ops->pointer_hide(ops->ctx);
    ops->grab(ops->ctx, ops->picture_shape(ops->ctx, under), left, top);
    ops->unlock(ops->ctx);
    ops->flip(ops->ctx);
    ops->select(ops->ctx, box);
    ops->blit(ops->ctx, ops->picture_shape(ops->ctx, under), 0, 0);
    int16_t w = (int16_t)ops->text_width(ops->ctx, typed);
    ops->text_shadowed(ops->ctx, typed, half - w / 2, 0);
    ops->select_default(ops->ctx);
    ops->pointer_hide(ops->ctx);
    ops->show(ops->ctx, ops->picture_shape(ops->ctx, box), left, top);
    ops->unlock(ops->ctx);
    ops->flip(ops->ctx);
    while (ops->keys_pending(ops->ctx)) ops->music(ops->ctx);
    ops->drain(ops->ctx);
    int32_t sum = 0;
    int blink = 0;
    int32_t taken;
    for (;;) {
        ops->pointer_poll(ops->ctx);
        ops->music(ops->ctx);
        const int16_t k = (int16_t)ops->key_read(ops->ctx);
        if (k == 0) {
            sum += ops->ticks(ops->ctx);
            if ((int16_t)sum < BLINK) continue;
            blink ^= 1;
            put(ops, box, under, blink ? cursor : typed, half - w / 2, left, top, 1);
            sum = 0;
            continue;
        }
        if (german && (k == SPACE || k == BACKSPACE)) ops->key_read(ops->ctx);
        if (k == ENTER || k == ESCAPE) {
            taken = k == ENTER;
            break;
        }
        if ((k == DELETE || k == BACKSPACE) && n > 0) typed[--n] = 0;
        else if (n < most && (is_sign(k) || (k > '/' && k < ':'))) typed[n++] = (char)k;
        w = (int16_t)ops->text_width(ops->ctx, typed);
        snprintf(cursor, sizeof cursor, "%s_", typed);
        put(ops, box, under, typed, half - w / 2, left, top, 0);
        ops->ticks(ops->ctx);
    }
    if (taken) strncpy(text, typed, NUMBER_FIELD_KEEP);
    ops->select_default(ops->ctx);
    ops->pointer_hide(ops->ctx);
    ops->blit(ops->ctx, ops->picture_shape(ops->ctx, under), left, top);
    ops->unlock(ops->ctx);
    ops->flip(ops->ctx);
    ops->picture_free(ops->ctx, box);
    ops->picture_free(ops->ctx, under);
    return taken;
}
