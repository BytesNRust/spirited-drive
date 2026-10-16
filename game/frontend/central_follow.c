/* Control Central (00432ff4): the pointer picks the item and its arrow (00433301..00433cc6). */
#include "game/frontend/central_run.h"

static int in(CentralRun *r, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    return r->ops->in_rect(r->ops->ctx, x0, x1, y0, y1) != 0;
}

static void set_item(CentralRun *r, int16_t item)
{
    if (r->item != item) r->item = item, r->changed = 1;
}

static void set_sub(CentralRun *r, int32_t sub)
{
    if ((int16_t)r->sub != sub) r->sub = sub, r->changed = 1;
}

static void none(CentralRun *r)
{
    if (r->item != CC_NONE) {
        r->item = CC_NONE;
        *r->st->net_hover = -1;
        r->changed = 1;
    }
}

/* an item's two arrows (y0..y1 shared) */
static void arrows(CentralRun *r, int32_t l0, int32_t l1, int32_t r0, int32_t r1, int32_t y0, int32_t y1)
{
    if (in(r, l0, l1, y0, y1)) set_sub(r, 1);
    else if (in(r, r0, r1, y0, y1)) set_sub(r, 2);
    else set_sub(r, 0);
}

/* 00433a06..00433aa5: each player's box on the panel, all of them tried */
static void players(CentralRun *r)
{
    ControlCentralState *st = r->st;
    for (int16_t i = 0; i < st->players; i++) {
        const int32_t x0 = (i % 4) * 0xa0, y0 = (i / 4) * 0x1e;
        *st->net_second = 0;
        if (!in(r, (int16_t)x0, (int16_t)(x0 + 0xa0), (int16_t)y0, (int16_t)(y0 + 0x1e))) continue;
        if (i == 1) *st->net_second = i;
        if (*st->race_type == 1 || i == *st->net_hover) continue;
        *st->net_hover = i;
        *st->net_player = i;
        r->item = CC_OPPONENT;
        *st->net_self = 0;
        st->was[WAS_OPPONENT] = 0x14;
        *st->opponent = st->other_car[i];
        r->changed = 1;
    }
}

/* the race type's box: its arrows outside a link game, or in a link single race or tournament unless the
 * player is himself without the host's say */
static void race_type_box(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const int arrows_on = st->lost == 0 ||
                          (!(st->net_host == 0 && *st->net_self != 0) && (*st->race_type == 3 || *st->race_type == 2));
    if (!arrows_on) {
        none(r);
        return;
    }
    if (r->item != 0) r->item = 0, r->changed = 1;
    arrows(r, 0x155, 0x16a, 0x25d, 0x26f, 0x91, 0xa8);
}

void central_follow(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const int16_t race = *st->race_type;
    if (in(r, 0x153, 0x271, 0x61, 0xda) &&
        (race == 1 || race == 2 || (st->lost != 0 && race == 3 && *st->net_self == 0))) {
        race_type_box(r);
    } else if (in(r, 0x13, 0x14a, 0x70, 0x11a) && st->view_only == 0) {
        set_item(r, CC_CAR);
        arrows(r, 0x15, 0x28, 0x136, 0x148, 0xba, 0xd1);
    } else if (in(r, 0x153, 0x225, 0xfc, 0x17f)) {
        if (st->lost != 0 && st->e7b == 0) return;
        set_item(r, CC_TRACK);
        arrows(r, 0x155, 0x168, 0x20f, 0x223, 0x131, 0x14e);
    } else if (in(r, 0x5a, 0x14c, 0x13a, 0x180)) {
        if (st->lost != 0 && st->e7b == 0) return;
        set_item(r, CC_RACE_TYPE);
        arrows(r, 0x5c, 0x6b, 0x136, 0x145, 0x154, 0x16d);
    } else if (in(r, 0xa5, 0x125, 0x1ba, 0x1d1)) {
        set_item(r, CC_OPTIONS);
    } else if (in(r, 0x1f8, 0x27f, 0x1ba, 0x1d1)) {
        set_item(r, CC_CANCEL);
    } else if (in(r, 0x16b, 0x1b5, 0x1b8, 0x1d6)) {
        if (st->lost != 0) set_item(r, CC_CHAT);
    } else if (in(r, 0, 0x70, 0x1ba, 0x1d1)) {
        if (st->lost != 0 && (st->view_only != 0 || st->players <= 1 || st->e7b == 0)) return;
        set_item(r, CC_RACE);
    } else if (in(r, 0x151, 0x20b, 0xdb, 0xf6) && (race == 3 || race == 2) && st->lost != 0) {
        set_item(r, CC_PLAYERS);
    } else if (in(r, 0, 0x280, 0, 0x38) && st->lost != 0) {
        players(r);
    } else {
        none(r);
    }
}

/* 00433ad9..00433cc6: the link game's limits, a new track or race type, the blink */
void central_after_follow(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    if (st->lost != 0) {
        if (*st->race_type == 1 && st->players > 2) *st->race_type = 2;
        if (st->players >= 8 && *st->net_self != 0) *st->net_self = 0, st->was[WAS_OPPONENT] = -1;
        if (*st->net_player + 1 >= st->players) *st->net_player = (int16_t)(st->players - 1);
    }
    if (r->track_was != *st->track) {
        r->track_was = *st->track;
        r->changed = 1;
        if (*st->race_type == 3) {
            *st->player_car = CENTRAL_TOURNAMENT_CAR[r->track_was];
            o->car_flags(o->ctx);
            st->was[WAS_OPPONENT] = 0x14;
            if (*st->net_self != 0) *st->opponent = 10;
        }
        central_laps(r);
    }
    if (r->race_was != *st->race_type) {
        o->type_changed(o->ctx);
        r->race_was = *st->race_type;
        r->changed = 1;
    }
    if (o->ticks(o->ctx) != 0) r->blink_ticks++;
    if ((int16_t)r->blink_ticks > 0x1e) {
        r->blink_ticks = 0;
        r->blink = 1 - r->blink;
        if ((int16_t)o->chat_waiting(o->ctx) != 0) r->redraw = 1;
    }
}
