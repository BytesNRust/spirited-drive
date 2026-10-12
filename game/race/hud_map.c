/* The dash's track map (00422ce0) and its pieces. */
#include "game/race/hud_map.h"

#include "game/gfx/blit.h"
#include "game/gfx/clip.h"
#include "game/gfx/flat.h"
#include "game/sys/shape_bank.h"

/* 00423aec */
void hud_map_marker(QuadTarget *t, uint8_t *bank, int32_t high, int32_t x, int32_t y, uint32_t colour,
                    int32_t surface_w, int32_t surface_h)
{
    if (high != 0) {
        clip_set(t, 2, 0x72, 9, 0x85, surface_w, surface_h);
        flat_fill_rect(t, x - 3, y - 3, 7, 5, colour);
        flat_line(t, x - 1, y - 4, x + 1, y - 4, colour);
        flat_line(t, x - 1, y + 2, x + 1, y + 2, colour);
        const uint8_t *pos = shape_bank_find(bank, (const uint8_t *)"pos1");
        if (pos == NULL) return;
        blit_shape_at_transparent(t, pos, x - 4, y - 5);
        clip_set(t, 2, 0x72, 2, 0x8c, surface_w, surface_h);
        return;
    }
    clip_set(t, 1, 0x39, 6, 0x30, surface_w, surface_h);
    flat_fill_rect(t, x - 2, y + 1, 4, 4, colour);
    const uint8_t *pos = shape_bank_find(bank, (const uint8_t *)"pos1");
    if (pos == NULL) return;
    blit_shape_at_transparent(t, pos, x - 3, y);
    clip_set(t, 1, 0x39, 1, 0x35, surface_w, surface_h);
}
