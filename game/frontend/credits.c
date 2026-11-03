#include "credits.h"

#include <stdio.h>

enum { PAGES = 5, ALL = 0x100, PAGE_TICKS = 2000 };
enum { SPACE = 0x20, ENTER = 0xd, ESCAPE = 0x1b, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };

/* one page: Escape, the right button or 0050a5b6 leave (1); Space, Enter, the left button or the time go on (0) */
static int page(int16_t n, const uint8_t *rgb, int32_t bank, int *released, const CreditsState *st,
                const CreditsOps *o)
{
    char tag[16];
    o->select(o->ctx, *st->screen);
    snprintf(tag, sizeof tag, "bgn%d", n);
    o->blit_own(o->ctx, o->shape(o->ctx, bank, tag));
    o->select_default(o->ctx);
    o->blit_at(o->ctx, o->picture_shape(o->ctx, *st->screen), 0, 0);
    o->unlock(o->ctx);
    o->flip(o->ctx);
    o->fade_in(o->ctx, 0, ALL, rgb);
    *st->idle = 0;
    for (;;) {
        o->music(o->ctx);
        if (*st->pointer_active) *st->buttons = (int16_t)o->pointer_read(o->ctx);
        if (*st->buttons == 0) *released = 1;
        const int32_t k = o->key(o->ctx);
        if (k == ESCAPE || (*st->buttons == RIGHT_BUTTON && *released) || *st->quit == 1) return 1;
        if (k == SPACE || k == ENTER || (*st->buttons == LEFT_BUTTON && *released) || *st->idle >= PAGE_TICKS)
            return 0;
    }
}

/* 004166e0 */
void credits_show(const CreditsState *st, const CreditsOps *ops)
{
    uint8_t rgb[CREDITS_PALETTE];
    const int32_t bank = ops->load_art(ops->ctx, "credits.fsh");
    const uint8_t *pal = ops->shape(ops->ctx, bank, "!pal");
    int released = 0, leave = 0;
    ops->palette_set(ops->ctx, rgb, 0, ALL, pal + 0x10, pal[0]);
    ops->music_feed(ops->ctx);
    for (int16_t n = 1; n <= PAGES && !leave; n++) {
        leave = page(n, rgb, bank, &released, st, ops);
        ops->fade_out(ops->ctx, 0, ALL);
        ops->music_feed(ops->ctx);
    }
    if (!leave && ops->extra != NULL) ops->extra(ops->ctx);
    ops->free_inverse(ops->ctx);
    ops->music_fade_out(ops->ctx);
    ops->fade_out(ops->ctx, 0, ALL);
    ops->music_end(ops->ctx);
    ops->free(ops->ctx, bank);
    ops->pointer_mark(ops->ctx);
}
