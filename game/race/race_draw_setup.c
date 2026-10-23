/* The race's drawing set up (race_draw_setup.h), as 0044ca3c runs it. */
#include "game/race/race_draw_setup.h"

enum {
    PALETTE_COLOURS = 0x100, SCREEN_BITS = 8,
    BACK_W_HIGH = 0x71, BACK_H_HIGH = 0x8c, BACK_W_LOW = 0x39, BACK_H_LOW = 0x34,
    MAP_W_HIGH = 0x74, MAP_H_HIGH = 0x8e, MAP_W_LOW = 0x3a, MAP_H_LOW = 0x36,
    KIND_DEFAULT = 8, REPLAY_KINDS = 0xe,
    SEGMENTS_DETAIL_0 = 0x1c, SEGMENTS_DETAIL_1 = 0x12, SEGMENTS_DETAIL_2 = 0xd,
    SCREEN_INTERLACED = 3
};

int32_t race_draw_detail_segments(int detail)
{
    if (detail == 0) return SEGMENTS_DETAIL_0;
    if (detail == 1) return SEGMENTS_DETAIL_1;
    if (detail == 2) return SEGMENTS_DETAIL_2;
    return -1;
}

/* 0044ca7c..0044cad5: the low resolution's palette written back around the screen mode. */
static void low_screen(const RaceDrawSetupOps *ops, const RaceDrawSetup *s)
{
    uint8_t rgb[DRAW_SETUP_PALETTE];
    ops->palette_get(ops->ctx, rgb);
    ops->front(ops->ctx);
    ops->palette_set(ops->ctx, 0, PALETTE_COLOURS, rgb);
    ops->clear(ops->ctx, 0);
    ops->screen_mode(ops->ctx, s->width, s->height, SCREEN_BITS);
    ops->front(ops->ctx);
    ops->palette_set(ops->ctx, 0, PALETTE_COLOURS, rgb);
    ops->clear(ops->ctx, 0);
}

/* 0044cad8..0044cb65 */
static void surfaces(const RaceDrawSetupOps *ops, RaceDrawSetup *s)
{
    s->view_surface = ops->surface(ops->ctx, s->width, s->height, 0);
    s->unknown_4f9b64 = 0;
    if (s->high != 0) {
        s->back_surface = ops->surface(ops->ctx, BACK_W_HIGH, BACK_H_HIGH, 0);
        s->map_surface = ops->surface(ops->ctx, MAP_W_HIGH, MAP_H_HIGH, 0);
    } else {
        s->back_surface = ops->surface(ops->ctx, BACK_W_LOW, BACK_H_LOW, 0);
        s->map_surface = ops->surface(ops->ctx, MAP_W_LOW, MAP_H_LOW, 0);
    }
    if (s->back_buffer != 0) s->screen_surface = ops->surface(ops->ctx, s->width, s->height, 0);
}

void race_draw_setup(const RaceDrawSetupOps *ops, RaceDrawSetup *s)
{
    s->restart = 1;
    s->unknown_512010 = s->mirror_state = 0;
    s->use_parts = 1;
    if (s->high == 0) low_screen(ops, s);
    surfaces(ops, s);
    ops->track_load(ops->ctx);
    ops->textures(ops->ctx);
    if (s->replay_kind != 0 && s->replay_kind < REPLAY_KINDS) {
        ops->follow(ops->ctx, s->chase_kind);
        s->next_kind = s->replay_next;
    } else {
        ops->follow(ops->ctx, KIND_DEFAULT);
    }
    ops->views(ops->ctx);
    s->mirror_count = DRAW_SETUP_MIRROR_COUNT;
    ops->setup_404404(ops->ctx);
    ops->setup_44c140(ops->ctx, 0);
    ops->setup_44c148(ops->ctx, 0);
    s->unknown_511fb4 = 1;
    ops->setup_449be4(ops->ctx);
    ops->setup_449430(ops->ctx);
    s->unknown_4c2d88 = 1;
    const int32_t segments = race_draw_detail_segments(s->detail);
    if (segments != -1) s->pending_segments = segments;
    if (s->screen_kind == SCREEN_INTERLACED) s->clear_view = 1;
    ops->screen(ops->ctx, s->window_size, s->screen_kind);
}

int32_t race_draw_start(const RaceDrawSetupOps *ops, RaceDrawSetup *s)
{
    s->unknown_511fc4 = 1;
    s->unknown_511fc0 = 0;
    race_draw_setup(ops, s);
    return 1;
}
