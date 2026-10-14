#include "game/race/race_frame.h"

#include "game/gfx/x86_int.h"

enum {
    FLAG_EVEN = 1, FLAG_FOURTH = 4, FLAG_EIGHTH = 0x10,
    PHASE_STOPPED = 4, PHASE_HELD = 2, TICK_STEP = 2
};

void race_frame(const RaceFrameOps *ops, RaceState *race, int16_t music_on, int16_t frame)
{
    if (music_on != 0)
        ops->music(ops->ctx);
    else
        ops->service(ops->ctx);
    ops->schedule(ops->ctx, frame);
    ops->menu_keys(ops->ctx, frame);
    ops->keys(ops->ctx);
    if (race->unknown_513e64 == 0 || race->unknown_513e60 != 0) return;
    uint8_t flags = 0;
    if ((race->tick & 1) == 0) flags = FLAG_EVEN;
    if ((race->tick & 3) == 0) flags |= FLAG_FOURTH;
    if ((race->tick & 7) == 0) flags |= FLAG_EIGHTH;
    race->tick_flags = flags;
    ops->tick(ops->ctx);
    if (race->phase == PHASE_STOPPED) return;
    if (race->phase == PHASE_HELD && race->unknown_4c5398 == 0 && race->unknown_4c539c <= 0) return;
    if (race->unknown_4c539c > 0) race->unknown_4c539c--;
    race->tick = x_add(race->tick, TICK_STEP);
}

void race_draw_frame(const RaceDrawOps *ops, HudState *hud, RaceState *race, int32_t display_changes,
                     int32_t *display_seen, int32_t *dash_redraw)
{
    if (*display_seen != display_changes) {
        hud_clear_request(hud);
        *dash_redraw = 1;
        race->view_taken = 1;
        *display_seen = display_changes;
    }
    ops->draw(ops->ctx);
}
