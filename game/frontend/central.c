/* Control Central (00432ff4): its frame, its start and its loop's end. */
#include "game/frontend/central_run.h"

#include <string.h>

const int16_t CENTRAL_X[CC_ITEMS] = { 482, 175, 444, 211, 45, 228, 400, 570, 0, 372 };
const int16_t CENTRAL_Y[CC_ITEMS] = { 158, 197, 318, 349, 454, 454, 454, 454, 0, 225 };
const int16_t CENTRAL_TOURNAMENT_CAR[10] = { 0, 3, 6, 0, 3, 3, 6, 6, 7, 3 };

void central_move(CentralRun *r)
{
    r->ops->pointer_move_to(r->ops->ctx, CENTRAL_X[(uint16_t)r->item], CENTRAL_Y[(uint16_t)r->item]);
}

void central_palette(CentralRun *r)
{
    r->ops->palette_put(r->ops->ctx, r->palette, 0, 0x100, r->pal + 0x10, (int16_t)(r->pal[0] & 0xff));
}

/* 00435b93..00435c33's: the inverse, central.qfs (ani.qfs and the panel's picture in a link game), !pal */
void central_load(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    *st->inverse = o->load_inverse(o->ctx, "control/central.inv");
    o->music(o->ctx);
    st->art[ART_CENTRAL] = o->load_art(o->ctx, "control/central.qfs");
    o->music(o->ctx);
    if (st->lost != 0) {
        st->art[ART_ANI] = o->load_art(o->ctx, "control/ani.qfs");
        st->art[ART_PANEL] = o->picture_new(o->ctx, 0x280, 0x38, 0);
        r->panel++;
    }
}

void central_laps(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const int short_track = *st->track >= 4 && *st->track <= 7;
    if (st->difficulty == 0) *st->laps = short_track ? 2 : 4;
    else if (st->difficulty == 1) *st->laps = short_track ? 6 : 8;
    else *st->laps = short_track ? 12 : 16;
}

static void start(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    *st->net_player = 0;
    *st->net_self = 1;
    *st->net_hover = -1;
    o->wait(o->ctx, 0);
    o->palette_clear(o->ctx, r->palette);
    o->pointer_move_to(o->ctx, 0xaa, 0xd6);
    if (st->view_only != 0) r->item = CC_OPTIONS;
    central_move(r);
    if (st->lost != 0 && *st->race_type == 0) {
        *st->race_type = 1;
        if (*st->opponent == 9) *st->opponent = *st->player_car;
    }
    *st->lost_vegas = 0, *st->vegas_cheat = 0, *st->unlock_m = 0, *st->unlock_r = 0;
    *st->variant_m = 0, *st->variant_r = 0;
    *st->lost_vegas = *st->unlock_level;
    if (*st->unlock_level >= 1) *st->warrior = 1, *st->unlock_m = 1;
    if (*st->unlock_level >= 2) *st->unlock_r = 1;
    if (*st->lost_vegas == 0 && *st->track == 8) *st->track = 0;
    central_car_start(st);
    o->wait(o->ctx, 0);
    o->music(o->ctx);
    st->art[ART_CENTRAL] = o->load_art(o->ctx, "control/central.qfs");
    o->music(o->ctx);
    if (st->lost != 0) {
        st->art[ART_ANI] = o->load_art(o->ctx, "control/ani.qfs");
        st->art[ART_PANEL] = o->picture_new(o->ctx, 0x280, 0x38, 0);
    }
    o->wait(o->ctx, 0);
    o->sound_load(o->ctx);
    *st->inverse = o->load_inverse(o->ctx, "control/central.inv");
    o->music_feed(o->ctx);
    o->pointer_mark(o->ctx);
    r->pal = o->shape(o->ctx, st->art[ART_CENTRAL], "!pal");
    central_palette(r);
    r->track_was = -1;
    r->race_was = -1;
}

/* 00437038..00437206: the redraw a link game asks for, the timer, the right button */
static void loop_end(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    o->music(o->ctx);
    if (*st->redraw_asked != 0) {
        if (st->lost != 0) {
            if (st->e7b == 0 && *st->race_type == 1 && *st->net_self != 0) {
                if (r->item == CC_PLAYERS) r->item = 1;
                *st->net_self = 0;
                st->was[WAS_OPPONENT] = 0x14;
            }
            if (*st->net_self != 0) st->was[WAS_OPPONENT] = 0x14;
            r->redraw = 5;
        } else {
            *st->redraw_asked = 0;
            r->redraw = 1;
        }
    }
    if (st->lost == 0) *st->net_self = 0;
    if ((int16_t)r->first != 0) return;
    if ((o->deadline_passed(o->ctx) != 0 && st->lost == 0 && *st->race_type != 3) || *st->link_lost != 0) {
        if (*st->link_lost != 0) *st->link_lost = 1, r->result = 0;
        else r->result = 2;
        o->sound(o->ctx, 0x48);
        r->done = 1;
    } else if (*st->buttons == 2 && (int16_t)r->buttons_armed != 0) {
        r->answer = o->yes_no(o->ctx, st->german != 0 ? "Spiel verlassen ?" : "Exit game ?");
        if ((int16_t)r->answer != 0) {
            if (st->lost != 0) *st->link_lost = 1;
            r->done = 1;
            r->result = 0;
        }
        o->drain(o->ctx);
        if (*st->pointer_active != 0) *st->buttons = (int16_t)o->pointer_buttons(o->ctx);
        while (o->key(o->ctx) != 0 || *st->buttons != 0) {
            o->music(o->ctx);
            *st->buttons = (int16_t)o->pointer_buttons(o->ctx);
        }
    }
}

/* 00434c91..00434cda: the keys arm again when none of Escape, space, Enter is held for 0x19 ticks */
static void arm_keys(CentralRun *r)
{
    const ControlCentralOps *o = r->ops;
    const int16_t key = (int16_t)o->key(o->ctx);
    if ((int16_t)r->keys_armed == 0 && key != 0x1b && key != 0x20 && key != 0xd) {
        r->tick_sum += o->ticks(o->ctx);
        if ((int16_t)r->tick_sum > 0x19) r->keys_armed = 1;
    }
    o->music(o->ctx);
}

/* 00434ff8..00435084: 'O' held in head to head is No Mercy */
static void no_mercy(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    if (*st->race_type == 1 && o->key_down(o->ctx, 0x4f) != 0) {
        if (*st->nomercy == 0) {
            *st->nomercy = 1;
            r->redraw = 1;
            st->was[WAS_RACE] = 0x14;
            *st->track_flags |= 4;
        }
    } else if (*st->nomercy != 0) {
        *st->nomercy = 0;
        st->was[WAS_RACE] = 0x14;
        *st->track_flags &= 0xfffb;
        r->redraw = 1;
    }
}

/* one turn of the loop at 00433301; 1 when the screen is left from inside (the frame's result is set) */
static int turn(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    o->net_service(o->ctx);
    *st->net_second = 0;
    central_follow(r);
    central_after_follow(r);
    if ((int16_t)r->redraw != 0) central_draw(r);
    const int32_t changed = r->changed;
    o->music(o->ctx);
    if ((int16_t)changed != 0) central_changed(r);
    arm_keys(r);
    central_cheats(r);
    no_mercy(r);
    if (*st->pointer_active != 0) *st->buttons = (int16_t)o->pointer_buttons(o->ctx);
    if (*st->buttons == 0) r->buttons_armed = 1;
    o->pointer_activate_if_moved(o->ctx);
    int16_t key = (int16_t)o->key(o->ctx);
    if (st->lost != 0 && st->net_host == 0 && (int16_t)o->net_start(o->ctx) != 0) {
        key = 0xd;
        r->item = CC_RACE;
        r->keys_armed = 1;
    }
    if (key == 0x20 || key == 0xd || (*st->buttons == 1 && (int16_t)r->buttons_armed != 0)) {
        if (central_select(r)) return 1;
        r->buttons_armed = 0;
        r->keys_armed = 0;
        o->ticks(o->ctx);
        r->tick_sum = 0;
    } else {
        central_keys(r, key);
    }
    loop_end(r);
    return 0;
}

int32_t control_central(ControlCentralState *st, const ControlCentralOps *ops)
{
    CentralRun r;
    memset(&r, 0, sizeof r);
    r.st = st, r.ops = ops;
    r.first = 1, r.redraw = 1, r.panel = 1, r.item = 1, r.keys_armed = 1, r.buttons_armed = 1;
    start(&r);
    while ((int16_t)r.done == 0)
        if (turn(&r)) return r.result;
    ops->sound_free(ops->ctx);
    ops->fade_out(ops->ctx, 0, 0x100);
    ops->art_free(ops->ctx);
    return r.result;
}
