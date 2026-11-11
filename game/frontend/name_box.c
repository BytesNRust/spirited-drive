#include "game/frontend/name_box.h"
#include "game/sys/port_log.h"

/* 004667fc */
int32_t name_box_show(char *name, int german, const MessageBannerInputs *in, NameBoxState *s,
                      const NameBoxOps *ops)
{
    port_log("mark:screen=name_box\n");   /* port: the headless walk (tools/menu_walk.sh) waits for it */
    enum { W = 0x1e0, H = 0x64, X = 0x50, Y = 0xbe, DARK = 0x6000, RED = 0xff0000, LENGTH = 8, SOUND = 0x48 };
    const MessageBannerOps *d = ops->draw;
    uint8_t shade[MESSAGE_BANNER_SHADE];
    const int32_t under = d->picture_new(d->ctx, W, H, 0);
    const int32_t box = d->picture_new(d->ctx, W, H, 0);
    d->shade_table(d->ctx, DARK, DARK, DARK, in->palette, shade);
    ops->pointer_hide(ops->ctx);
    d->select(d->ctx, in->screen);
    d->grab(d->ctx, d->picture_shape(d->ctx, under), X, Y);
    d->select(d->ctx, box);
    d->blit(d->ctx, d->picture_shape(d->ctx, under), 0, 0);
    d->shade_set(d->ctx, shade);
    const int32_t corners[8] = { 0, 0, W - 1, 0, W - 1, H - 1, 0, H - 1 };
    d->shade_polygon(d->ctx, 4, corners);
    d->frame(d->ctx, 2, 2, W - 3, H - 3, d->colour(d->ctx, RED));
    d->frame(d->ctx, 3, 3, W - 4, H - 4, d->colour(d->ctx, RED));
    d->text_styles(d->ctx);
    d->text_select(d->ctx, 2);
    d->text_centred(d->ctx, german ? "Bitte Namen Eingeben" : "Please Enter Name", 0xf0, 0xa);
    d->text_select(d->ctx, 1);
    ops->pointer_hide(ops->ctx);
    d->select(d->ctx, in->screen);
    d->blit(d->ctx, d->picture_shape(d->ctx, box), X, Y);
    d->select_default(d->ctx);
    d->blit_own(d->ctx, d->picture_shape(d->ctx, in->screen));
    d->flip(d->ctx);
    const int32_t taken = ops->text_field(ops->ctx, name, LENGTH, 0x140, 0xe6, 1);
    ops->sound(ops->ctx, SOUND);
    if ((int16_t)taken) *s->keys_50a = 0, *s->keys_506 = 0;
    ops->pointer_hide(ops->ctx);
    d->select(d->ctx, in->screen);
    d->blit(d->ctx, d->picture_shape(d->ctx, under), X, Y);
    d->select_default(d->ctx);
    d->blit_own(d->ctx, d->picture_shape(d->ctx, in->screen));
    d->flip(d->ctx);
    d->picture_free(d->ctx, under);
    d->picture_free(d->ctx, box);
    return taken;
}
