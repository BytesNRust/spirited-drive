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
    do {
        if (p->active) p->buttons = (int16_t)ops->pointer_poll(ops->ctx);
        ops->music_service(ops->ctx);
    } while (!ops->deadline_passed(ops->ctx) && !ops->key(ops->ctx) && p->buttons == 0);
    if (!s->music_off) ops->fade_out(ops->ctx, 0, FADE_COUNT);
}
