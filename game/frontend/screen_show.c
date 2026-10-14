#include "game/frontend/screen_show.h"

/* 00466f68 */
int screen_show(const char *name, int32_t mode, int32_t music, int32_t screen, int32_t screen_shape,
                uint8_t screen_palette[PALETTE_PUT_BYTES], DialogPointer *p, const ScreenShowOps *ops)
{
    enum { COLOURS = 0x100, WAIT_MUSIC = 0x12c, WAIT_QUIET = 0x1f4, SPACE = 0x20, ENTER = 0xd, ESCAPE = 0x1b };
    const int16_t how = (int16_t)mode, with_music = (int16_t)music;
    uint8_t pal[PALETTE_PUT_BYTES];
    int skipped = 0;
    if (how != SCREEN_SHOW_SWITCH) {
        ops->pointer_hide(ops->ctx);
        ops->fill(ops->ctx, 0);
        ops->unlock(ops->ctx);
        ops->flip(ops->ctx);
    }
    const int32_t bank = ops->load_art(ops->ctx, name);
    const uint8_t *rgb = ops->shape(ops->ctx, bank, "!pal");
    palette_put(pal, 0, COLOURS, rgb + 0x10, rgb[0], screen_palette);
    if (with_music) ops->music_feed(ops->ctx);
    ops->select(ops->ctx, screen);
    ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "bgnd"));
    ops->select_default(ops->ctx);
    ops->blit_at(ops->ctx, screen_shape, 0, 0);
    ops->flip(ops->ctx);
    if (how != SCREEN_SHOW_SWITCH) ops->fade_in(ops->ctx, 0, COLOURS, pal);
    if (how == SCREEN_SHOW_WAIT) {
        ops->deadline_set(ops->ctx, with_music ? WAIT_MUSIC : WAIT_QUIET);
        while (!ops->deadline_passed(ops->ctx) && !skipped) {
            if (with_music) ops->music_service(ops->ctx);
            if (p->active) p->buttons = (int16_t)ops->pointer_poll(ops->ctx);
            const int32_t key = ops->key(ops->ctx);
            if (key == SPACE || key == ENTER || key == ESCAPE || p->buttons != 0) skipped = 1;
        }
        ops->fade_out(ops->ctx, 0, COLOURS);
    }
    if (with_music) ops->music_feed(ops->ctx);
    ops->free(ops->ctx, bank);
    return skipped;
}
