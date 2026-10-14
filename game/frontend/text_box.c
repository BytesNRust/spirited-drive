#include "game/frontend/text_box.h"

/* 00463934 */
void text_box_show(const char *text, int32_t x, int32_t y, int32_t screen, const TextBoxOps *ops)
{
    enum { MARGIN = 10, HEIGHT = 0x2d, SHADOW = 2 };
    const int32_t picture = ops->picture_new(ops->ctx, ops->text_width(ops->ctx, text) + MARGIN, HEIGHT, 0);
    ops->select(ops->ctx, screen);
    ops->grab(ops->ctx, ops->picture_shape(ops->ctx, picture), x, y);
    ops->select(ops->ctx, picture);
    ops->ramp(ops->ctx, 1);
    ops->draw(ops->ctx, text, SHADOW, SHADOW);
    ops->ramp(ops->ctx, 0);
    ops->draw(ops->ctx, text, 0, 0);
    ops->pointer_hide(ops->ctx);
    ops->select(ops->ctx, screen);
    ops->blit(ops->ctx, ops->picture_shape(ops->ctx, picture), x, y);
    ops->select_default(ops->ctx);
    ops->blit_own(ops->ctx, ops->picture_shape(ops->ctx, screen));
    ops->flip(ops->ctx);
    ops->picture_free(ops->ctx, picture);
}
