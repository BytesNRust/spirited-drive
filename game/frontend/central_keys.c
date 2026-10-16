/* Control Central (00432ff4): the letters typed (00434cdf..00434ff8) and the other keys (004360df..0043700e). */
#include "game/frontend/central_run.h"

/* the first of 'A'..'Z' held, or 0 */
static int32_t letter(CentralRun *r)
{
    for (int32_t c = 'A'; c < 0x5b; c++)
        if (r->st->keys[r->ops->scancode(r->ops->ctx, c)] != 0) return c;
    return 0;
}

static int cheat(CentralRun *r, int which)
{
    return r->ops->cheat_step(r->ops->ctx, which) != 0;
}

static void ding(CentralRun *r) { r->ops->sound(r->ops->ctx, 0x48); }

/* 00434d3d..00434ff8: each code stepped with the new letter, while its effect is off */
static void cheat_codes(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const int race3 = *st->race_type == 3;
    if (*st->unlock_m == 0 && (st->lost == 0 || st->e7b != 0) && cheat(r, CENTRAL_SKCART)) {
        *st->unlock_m = 1;
        ding(r);
        if (!race3) *st->variant_m = 1, *st->variant_r = 0, r->redraw = 1;
    }
    if (*st->unlock_r == 0 && (st->lost == 0 || st->e7b != 0) && cheat(r, CENTRAL_EARALLYE)) {
        *st->unlock_r = 1;
        ding(r);
        if (!race3) *st->variant_r = 1, *st->variant_m = 0, r->redraw = 1;
    }
    if (*st->explode == 0 && !race3 && cheat(r, CENTRAL_EXPLODE)) *st->explode = 1, ding(r);
    if (*st->nitro == 0 && *st->race_type != 3 && st->lost == 0 && cheat(r, CENTRAL_NITRO)) *st->nitro = 1, ding(r);
    if (*st->vegas_cheat == 0 && *st->race_type != 3 && cheat(r, CENTRAL_CASINOVILLE)) {
        *st->vegas_cheat = 1;
        *st->lost_vegas = 1;
        if (st->net_host != 0) *st->track = 8, r->redraw = 1;
    }
    if (*st->race_type != 3 && cheat(r, CENTRAL_CONCEPTCAR)) {
        *st->warrior = 1;
        r->redraw = 1;
        *st->player_car = 11;
        ding(r);
    }
    if (st->lost == 0 && *st->race_type != 3 && cheat(r, CENTRAL_TURBO)) *st->track_flags |= 8, ding(r);
    if (*st->race_type != 3 && cheat(r, CENTRAL_ROCKYROAD)) {
        *st->unlock_level = (int16_t)((*st->unlock_level == 1) + 1);
        if (*st->lost_vegas == 0) *st->lost_vegas = *st->unlock_level;
        if (*st->unlock_level >= 1) *st->warrior = 1, *st->unlock_m = 1;
        if (*st->unlock_level >= 2) *st->unlock_r = 1;
    }
}

void central_cheats(CentralRun *r)
{
    ControlCentralState *st = r->st;
    if (*st->letter_held != 0) {
        *st->letter_held = (int16_t)letter(r);
        return;
    }
    if (letter(r) == 0) return;
    ++*st->letter_held;
    cheat_codes(r);
}

/* 004361d9..00436395: Up */
static int16_t up(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const int16_t race = *st->race_type;
    const int host_many = st->net_host != 0 && st->players > 1;
    switch ((uint16_t)r->item) {
    case CC_OPPONENT: return st->lost == 0 || host_many ? CC_RACE : CC_OPTIONS;
    case CC_PLAYERS:
        if ((st->net_host != 0 && race != 3) || *st->net_self == 0) return CC_OPPONENT;
        return host_many ? CC_RACE : CC_OPTIONS;
    case CC_CAR:
        if (st->lost != 0) return race == 2 || race == 3 ? CC_PLAYERS : host_many ? CC_RACE : CC_OPTIONS;
        return race == 3 || race == 0 ? CC_RACE : CC_OPPONENT;
    case CC_TRACK: case CC_RACE_TYPE: case CC_RACE: return (int16_t)(r->item - 1);
    case CC_OPTIONS: case CC_CHAT: case CC_CANCEL:
        if (st->lost != 0 && st->view_only != 0) return race == 1 ? r->item : CC_PLAYERS;
        return st->lost == 0 || st->e7b != 0 ? CC_RACE_TYPE : CC_CAR;
    case CC_NONE: return CC_CAR;
    default: return r->item;
    }
}

/* 0043639a..00436562: Down */
static int16_t down(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const int16_t race = *st->race_type;
    switch ((uint16_t)r->item) {
    case CC_OPPONENT: return st->lost != 0 && (race == 2 || race == 3) ? CC_PLAYERS : CC_CAR;
    case CC_PLAYERS: return st->lost != 0 && st->view_only != 0 ? CC_OPTIONS : CC_CAR;
    case CC_CAR: return st->lost != 0 && st->e7b == 0 ? CC_OPTIONS : CC_TRACK;
    case CC_TRACK: return CC_RACE_TYPE;
    case CC_RACE_TYPE: return st->lost == 0 || (st->players > 1 && st->e7b != 0) ? CC_RACE : CC_OPTIONS;
    case CC_RACE: case CC_OPTIONS: case CC_CHAT: case CC_CANCEL:
        if (st->lost == 0) return race == 3 || race == 0 ? CC_CAR : CC_OPPONENT;
        if (st->view_only != 0) return *st->net_self != 0 ? CC_PLAYERS : CC_OPPONENT;
        if (race == 1) return CC_CAR;
        return *st->net_self != 0 && (st->net_host == 0 || race == 3) ? CC_PLAYERS : CC_OPPONENT;
    case CC_NONE: return CC_CAR;
    default: return r->item;
    }
}

static void moved(CentralRun *r, int16_t item)
{
    r->item = item;
    central_move(r);
    r->changed = 1;
}

static void arrow_shown(CentralRun *r, const char *tag)
{
    central_move(r);
    r->ops->blit_masked(r->ops->ctx, r->ops->shape(r->ops->ctx, r->st->art[ART_CENTRAL], tag));
}

static void race_type_key(CentralRun *r, int32_t key)
{
    const ControlCentralOps *o = r->ops;
    ding(r);
    o->race_type_step(o->ctx, key);
    r->st->was[WAS_OPPONENT] = 0x14;
    r->redraw = 1;
    r->panel++;
    central_move(r);
}

/* 00436567..00436a07: Right */
static void right(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    o->pointer_off(o->ctx);
    switch ((uint16_t)r->item) {
    case CC_OPPONENT:
        ding(r), o->pointer_off(o->ctx);
        central_opponent_up(r, 0);
        arrow_shown(r, "Aro6");
        break;
    case CC_CAR:
        ding(r), o->pointer_off(o->ctx);
        central_car_up(r);
        arrow_shown(r, "Aro2");
        break;
    case CC_TRACK:
        ding(r), o->pointer_off(o->ctx);
        o->track_forward(o->ctx);
        arrow_shown(r, "Aro8");
        r->redraw = 1;
        break;
    case CC_RACE_TYPE: race_type_key(r, 0x4d00); break;
    case CC_RACE: case CC_CHAT: moved(r, (int16_t)(r->item + 1)); break;
    case CC_OPTIONS: moved(r, st->lost == 0 ? CC_CANCEL : (int16_t)(r->item + 1)); break;
    case CC_CANCEL: moved(r, st->lost == 0 || (st->players > 1 && st->e7b != 0) ? CC_RACE : CC_OPTIONS); break;
    case CC_NONE: r->item = CC_CANCEL; break;
    default: break;
    }
}

/* 00436a0c..00436fd0: Left */
static void left(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    o->pointer_off(o->ctx);
    switch ((uint16_t)r->item) {
    case CC_OPPONENT:
        ding(r), o->pointer_off(o->ctx);
        central_opponent_down(r, 0);
        arrow_shown(r, "Aro5");
        break;
    case CC_CAR:
        ding(r), o->pointer_off(o->ctx);
        central_car_down(r);
        arrow_shown(r, "Aro1");
        break;
    case CC_TRACK:
        ding(r), o->pointer_off(o->ctx);
        o->track_backward(o->ctx);
        if (*st->track < 0) {
            *st->track = 7;
            if (*st->lost_vegas != 0) *st->track = 8;
        }
        if (*st->race_type == 3) {
            *st->player_car = CENTRAL_TOURNAMENT_CAR[*st->track];
            o->car_flags(o->ctx);
            st->was[WAS_OPPONENT] = 0x14;
            *st->opponent = 10;
        }
        central_laps(r);
        arrow_shown(r, "Aro7");
        r->redraw = 1;
        break;
    case CC_RACE_TYPE: race_type_key(r, 0x4b00); break;
    case CC_RACE: moved(r, CC_CANCEL); break;
    case CC_OPTIONS:
        moved(r, st->lost == 0 || (st->view_only == 0 && st->players > 1 && st->e7b != 0) ? CC_RACE : CC_CANCEL);
        break;
    case CC_CHAT: moved(r, (int16_t)(r->item - 1)); break;
    case CC_CANCEL: moved(r, st->lost != 0 ? CC_CHAT : CC_OPTIONS); break;
    case CC_NONE: r->item = CC_RACE; break;
    default: break;
    }
}

static void escape(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    ding(r);
    if (*st->tournament != 0)
        r->answer = o->yes_no(o->ctx, st->german != 0 ? "Wettkampf beenden ?" : "Quit tournament ?");
    if ((int16_t)r->answer == 0 && *st->tournament != 0) return;
    r->done = 1;
    o->drain(o->ctx);
    r->result = 0;
    if (st->lost != 0) *st->link_lost = 1;
}

void central_keys(CentralRun *r, int16_t key)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    if (key != 0 && (int16_t)r->keys_armed != 0) {
        o->select(o->ctx, *st->screen);
        switch ((uint16_t)key) {
        case 0x1b: escape(r); break;
        case 0x4800: o->pointer_off(o->ctx), moved(r, up(r)); break;
        case 0x5000: o->pointer_off(o->ctx), moved(r, down(r)); break;
        case 0x4d00: right(r); break;
        case 0x4b00: left(r); break;
        default: break;
        }
        o->deadline_set(o->ctx, 0x2ee0);
        o->ticks(o->ctx);
        r->keys_armed = 0;
        r->tick_sum = 0;
    } else if (key == 0) {
        r->keys_armed = 1;
    }
    if (st->lost != 0 && *st->net_self == 0) *st->opponent = st->other_car[*st->net_player];
}
