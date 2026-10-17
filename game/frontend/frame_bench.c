#include "game/frontend/frame_bench.h"

/* 004670e8 */
int32_t frame_benchmark(int32_t screen, int32_t screen_shape, uint8_t screen_palette[PALETTE_PUT_BYTES],
                        const FrameBenchOps *ops)
{
    enum { COLOURS = 0x100, FRAMES = 100 };
    uint8_t pal[PALETTE_PUT_BYTES];
    ops->pointer_hide(ops->ctx);
    ops->fill(ops->ctx, 0);
    ops->unlock(ops->ctx);
    ops->flip(ops->ctx);
    ops->pointer_hide(ops->ctx);
    const int32_t bank = ops->load_art(ops->ctx, "title.qfs");
    const uint8_t *rgb = ops->shape(ops->ctx, bank, "!pal");
    palette_put(pal, 0, COLOURS, rgb + 0x10, rgb[0], screen_palette);
    ops->select(ops->ctx, screen);
    ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "bgnd"));
    ops->select_default(ops->ctx);
    ops->blit_at(ops->ctx, screen_shape, 0, 0);
    ops->flip(ops->ctx);
    ops->fade_in(ops->ctx, 0, COLOURS, pal);
    ops->ticks(ops->ctx);
    for (int frame = 0; frame < FRAMES; frame++) {
        ops->select(ops->ctx, screen);
        ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "bgnd"));
        ops->select_default(ops->ctx);
        ops->blit_at(ops->ctx, screen_shape, 0, 0);
        ops->flip(ops->ctx);
    }
    const int32_t ticks = ops->ticks(ops->ctx);
    ops->free(ops->ctx, bank);
    return ticks;
}
