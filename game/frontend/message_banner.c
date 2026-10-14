#include "game/frontend/message_banner.h"

/* 0046667c */
void message_banner_show(const char *text, const MessageBannerInputs *in, const MessageBannerOps *ops)
{
    enum { PAD = 0x14, H = 0x3c, CENTRE_X = 0x140, Y = 0xd2, SHADE = 0x9000, RED = 0xff0000, STYLE = 2,
           TEXT_Y = 0x12 };
    const int32_t w = (int16_t)(ops->text_width(ops->ctx, text) + PAD);
    const int32_t half = w / 2;
    const int32_t x = (int16_t)(CENTRE_X - half);
    const int32_t picture = ops->picture_new(ops->ctx, w, H, 0);
    uint8_t shade[MESSAGE_BANNER_SHADE];
    ops->shade_table(ops->ctx, SHADE, SHADE, SHADE, in->palette, shade);
    ops->select(ops->ctx, in->screen);
    ops->grab(ops->ctx, ops->picture_shape(ops->ctx, picture), x, Y);
    ops->select(ops->ctx, picture);
    ops->shade_set(ops->ctx, shade);
    const int32_t box[8] = { 0, 0, w - 1, 0, w - 1, H - 1, 0, H - 1 };
    ops->shade_polygon(ops->ctx, 4, box);
    ops->frame(ops->ctx, 2, 2, w - 3, H - 3, ops->colour(ops->ctx, RED));
    ops->frame(ops->ctx, 3, 3, w - 4, H - 4, ops->colour(ops->ctx, RED));
    ops->text_styles(ops->ctx);
    ops->text_select(ops->ctx, STYLE);
    ops->text_centred(ops->ctx, text, half, TEXT_Y);
    ops->select(ops->ctx, in->screen);
    ops->blit(ops->ctx, ops->picture_shape(ops->ctx, picture), x, Y);
    ops->select_default(ops->ctx);
    ops->blit_own(ops->ctx, ops->picture_shape(ops->ctx, in->screen));
    ops->flip(ops->ctx);
    ops->picture_free(ops->ctx, picture);
}

/* 00467214 */
void message_banner_wait(const char *text, int32_t time, int32_t screen, int *pointer_active,
                         int *pointer_buttons, const MessageBannerWaitOps *ops)
{
    enum { X = 0x64, Y = 0xd2, W = 0x1b8, H = 0x3c, FOREVER = -1, SETTLE = 10 };
    ops->music_service(ops->ctx);
    const int32_t picture = ops->picture_new(ops->ctx, W, H, 0);
    ops->select(ops->ctx, screen);
    ops->grab(ops->ctx, ops->picture_shape(ops->ctx, picture), X, Y);
    ops->banner(ops->ctx, text);
    if (time != FOREVER) ops->deadline_set(ops->ctx, time);
    ops->select(ops->ctx, screen);
    ops->blit(ops->ctx, ops->picture_shape(ops->ctx, picture), X, Y);
    ops->picture_free(ops->ctx, picture);
    if (time == FOREVER) return;
    *pointer_buttons = 0;
    ops->pointer_show(ops->ctx);
    while (!ops->deadline_passed(ops->ctx) && !ops->key(ops->ctx) && *pointer_buttons == 0) {
        if ((int16_t)*pointer_active != 0) *pointer_buttons = (int16_t)ops->pointer_buttons(ops->ctx);
        ops->music_service(ops->ctx);
    }
    ops->deadline_set(ops->ctx, SETTLE);
    while (!ops->deadline_passed(ops->ctx)) ops->music_service(ops->ctx);
    ops->select_default(ops->ctx);
    ops->blit_own(ops->ctx, ops->picture_shape(ops->ctx, screen));
    ops->flip(ops->ctx);
    ops->select(ops->ctx, screen);
    ops->pointer_show(ops->ctx);
}
