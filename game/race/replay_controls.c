/* 0044f55c: the replay controls' input */
#include "replay_controls.h"

#include <stddef.h>

enum { KEY_LEFT = 0x4b00, KEY_RIGHT = 0x4d00, KEY_SPACE = 0x20, KEY_RETURN = 0xd, ESCAPE_AFTER = 15, CAMERA_KIND = 5,
       SPEED_BUTTON = 6 };

static const int32_t SPEEDS[REPLAY_INPUT_SPEEDS] = { 6, 6, 7, 8 };  /* 004c5374 */

static int32_t half(const uint8_t *shape, int at) { return (int16_t)(uint16_t)(shape[at] | shape[at + 1] << 8) >> 1; }

/* The buttons under the pointer; 1 when one is */
static int pointer_buttons(ReplayInput *in, const ReplayInputOps *ops)
{
    int any = 0;
    for (int32_t i = 1; i <= REPLAY_INPUT_BUTTONS; i++) {
        if (i == REPLAY_INPUT_BUTTONS && in->kind == CAMERA_KIND) continue;
        const int32_t *r = in->rects[i];
        if (!ops->in_rect(ops->ctx, (int16_t)r[0], (int16_t)r[2], (int16_t)r[1], (int16_t)r[3])) continue;
        if (in->mouse_buttons == 0) {
            if (i != in->cursor) in->cursor = i, in->pressed = 0, in->redraw = 1;
        } else if (i != in->pressed) {
            in->pressed = i, in->cursor = 0, in->redraw = 1;
        }
        any = 1;
    }
    return any;
}

/* Left and right step the cursor (the pointer onto it), space and return press it; 1 when any did */
static int keys(ReplayInput *in, const ReplayInputOps *ops, int32_t key)
{
    int any = 0, moved = 0;
    if (key != in->last_key && key == KEY_LEFT) {
        if (--in->cursor < 1) in->cursor = REPLAY_INPUT_BUTTONS;
        if (in->kind == CAMERA_KIND && in->cursor == REPLAY_INPUT_BUTTONS) in->cursor = REPLAY_INPUT_BUTTONS - 1;
        in->redraw = 1, any = moved = 1;
    }
    if (key != in->last_key && key == KEY_RIGHT) {
        if (++in->cursor > REPLAY_INPUT_BUTTONS) in->cursor = 1;
        if (in->kind == CAMERA_KIND && in->cursor == REPLAY_INPUT_BUTTONS) in->cursor = 1;
        in->redraw = 1, any = moved = 1;
    }
    const int32_t c = in->cursor;
    if (moved && c >= 0 && c <= REPLAY_INPUT_BUTTONS && in->shapes[c] != NULL)  /* port guard */
        ops->place(ops->ctx, half(in->shapes[c], 4) + in->rects[c][0], half(in->shapes[c], 6) + in->rects[c][1]);
    if (key == KEY_SPACE || key == KEY_RETURN) in->pressed = in->cursor, in->redraw = 1, any = 1;
    return any;
}

/* The pressed button's effect */
static void press(ReplayInput *in)
{
    switch (in->pressed) {
    case 1: in->request = 7, in->speed = 0; break;
    case 2: in->button = 1, in->speed = 0; break;
    case 3: in->button = 3, in->speed = 0; break;
    case 4: in->button = 4, in->speed = 0; break;
    case 5: in->button = 2, in->speed = 0; break;
    case SPEED_BUTTON:
        if (in->last_pressed != SPEED_BUTTON) {
            if (in->phase == 2 && ++in->speed > 3) in->speed = 1;
            if ((uint32_t)in->speed < REPLAY_INPUT_SPEEDS) in->button = SPEEDS[in->speed];  /* port guard */
        }
        in->unknown_5138a0 = 1;
        break;
    case 7: if (in->last_pressed != 7) in->button = 5, in->speed = 0; break;
    case 8: if (in->last_pressed != 8) in->button = 10; break;
    default: break;
    }
}

void replay_controls_input(ReplayInput *in, const ReplayInputOps *ops)
{
    if (in->high) ops->window_size(ops->ctx, 0, 0x1c2, 0x280, 0x1e0);
    else ops->window_size(ops->ctx, 0, 0xb4, 0x140, 0xc8);
    if (in->mouse != 0) in->mouse_buttons = ops->buttons(ops->ctx);
    in->buttons_seen = in->mouse_buttons;
    int any = 0;
    if (in->last_x != in->pointer_x || in->pointer_y != in->last_y || in->mouse_buttons != 0 || in->cursor == SPEED_BUTTON)
        any = pointer_buttons(in, ops);
    in->last_x = in->pointer_x, in->last_y = in->pointer_y;
    const int32_t key = ops->key(ops->ctx);
    if (key != 0 && keys(in, ops, key)) any = 1;
    if (in->escape != 0 && in->timer > ESCAPE_AFTER) in->pressed = 1, any = 1, in->redraw = 1;
    in->last_key = key;
    if (!any && in->pressed != 0) in->cursor = in->pressed, in->pressed = 0, in->redraw = 1;
    press(in);
    in->last_pressed = in->pressed;
}
