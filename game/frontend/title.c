#include "game/frontend/title.h"

/* 00467214's endless time */
enum { NO_WAIT = -1 };

/* 004372e4 */
void title_screen_show(int32_t state, TitleState *s, DialogPointer *p, const TitleOps *ops)
{
    enum { SHOWN = 0x1f4, STATE_INSTANT = 3, FADE_COUNT = 0x100 };
    ops->deadline_set(ops->ctx, SHOWN);
    ops->screen_show(ops->ctx, "title.qfs", state == STATE_INSTANT ? 3 : 1, 1);
    if (s->music_off && s->link_game && s->link_lost) {
        s->link_lost = 0, s->link_game = 0;
        ops->banner_wait(ops->ctx, s->german ? "Verbindung abgebrochen" : "Connection Terminated", NO_WAIT);
    }
    /* port (fresh press): a key or a button counts once it was seen up on this screen. The exe's 00464bd0 and [004c6194] read what is held, and the
     * press that stopped EA's movie was let go during its mode switch and this screen's fade; here they take no time, and the same press (or its
     * latch, key_latch.h) closed this screen too */
    int key_armed = 0, buttons_armed = 0, key = 0;
    do {
        if (p->active) p->buttons = (int16_t)ops->pointer_poll(ops->ctx);
        ops->music_service(ops->ctx);
        if (ops->deadline_passed(ops->ctx)) break;
        key = ops->key(ops->ctx);
        key_armed |= key == 0, buttons_armed |= p->buttons == 0;
    } while (!(key != 0 && key_armed) && !(p->buttons != 0 && buttons_armed));
    if (!s->music_off) ops->fade_out(ops->ctx, 0, FADE_COUNT);
}
