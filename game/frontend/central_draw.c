/* Control Central (00432ff4): the screen drawn again (00433cd5..00434b54) and an item's change (00434b69..). */
#include "game/frontend/central_banner.h"
#include "game/frontend/central_run.h"

#include "game/frontend/variant_rule.h"
#include "game/race/lunar_springs.h"
#include "game/sys/port_extras.h"
#include "game/sys/resource.h"

#include <stdio.h>
#include <string.h>

/* the frame's name tables (00432050.., copied to esp+400.. and read past their ends as the stack has them):
 * the tracks, their "m" and "r" art */
static const char *const TRACK_NAMES[10 + 9 + 9] = {
    "city", "coast", "alpine", "springs", "avalley", "bsienna", "vridge", "tpolis", "lvegas", "moon",
    "citym", "coastm", "alpinem", "springsm", "avalleym", "bsiennam", "vridgem", "tpolism", "lvegasm",
    "cityr", "coastr", "alpiner", "springsr", "avalleyr", "bsiennar", "vridger", "tpolisr", "lvegasr" };
static const char *const CAR_NAMES[12] = {
    "rx71", "nsx1", "sup1", "9111", "vet1", "vip1", "5121", "dia1", "war1", "war1", "war1", "war1" };
static const char *const OPPONENT_NAMES[12] = {
    "rx72", "nsx2", "sup2", "9112", "vet2", "vip2", "5122", "dia2", "slew", "watch2", "tourny", "war2" };
static const char *const RACE_NAMES[5] = { "watch1", "headhead", "single", "tourny", "nomercy" };
/* the players' colours (00432030) and boxes on the panel (004baf24, 004baf34) */
static const int32_t PLAYER_COLOUR[8] = { 0x09, 0x69, 0x7c, 0x20, 0xfe, 0xe0, 0xbe, 0x9f };
static const int16_t BOX_X[4] = { 0, 161, 321, 482 };
static const int32_t BOX_W[4] = { 158, 157, 158, 157 };

static const uint8_t *shape(CentralRun *r, int slot, const char *tag)
{
    return r->ops->shape(r->ops->ctx, r->st->art[slot], tag);
}

static void blit(CentralRun *r, const char *tag) { r->ops->blit(r->ops->ctx, shape(r, ART_CENTRAL, tag)); }
static void masked(CentralRun *r, const char *tag) { r->ops->blit_masked(r->ops->ctx, shape(r, ART_CENTRAL, tag)); }

/* the players' names in their boxes; style 1 the one picked (when chosen) */
static void player_boxes(CentralRun *r, int pick)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    for (int16_t i = 0; i < st->players; i++) {
        char name[0x20];
        strcpy(name, st->names + 9 * i);
        if (o->text_width(o->ctx, name) > 0x9b)
            for (char *c = name; *c != 0; c++)
                if (*c >= 'A' && *c <= 'Z') *c = (char)(*c + 0x20);
        const int16_t w = (int16_t)o->text_width(o->ctx, name);
        const int16_t x0 = (int16_t)((i % 4) * 0xa0), y = (int16_t)((i / 4) * 0x1d);
        const int style = pick && *st->net_self == 0 && i == *st->net_player && *st->race_type != 1;
        o->text_select(o->ctx, style);
        o->text_shadowed(o->ctx, name, x0 + 0x50 - w / 2, y + 1);
        const int k = i % 4;
        o->frame_rect(o->ctx, BOX_X[k], y, BOX_X[k] + BOX_W[k], y + 0x1a, PLAYER_COLOUR[i]);
        o->frame_rect(o->ctx, BOX_X[k] + 1, y + 1, BOX_X[k] + BOX_W[k] - 1, y + 0x19, PLAYER_COLOUR[i]);
        o->music(o->ctx);
    }
}

static void panel(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    if (r->players_was != st->players || *st->net_player != *st->net_player_was) {
        r->players_was = st->players;
        r->panel++;
        *st->net_player_was = *st->net_player;
    }
    if ((int16_t)r->panel != 0) {
        o->select(o->ctx, st->art[ART_PANEL]);
        r->panel = 0;
        o->blit(o->ctx, shape(r, ART_ANI, "R$T9"));
        player_boxes(r, 1);
    }
    o->select(o->ctx, *st->screen);
    o->blit_word(o->ctx, o->picture_shape(o->ctx, st->art[ART_PANEL]));
}

static void reload(CentralRun *r, int slot, const char *path)
{
    const ControlCentralOps *o = r->ops;
    if (r->st->art[slot] != 0) o->free(o->ctx, r->st->art[slot]);
    r->st->art[slot] = o->load_bank(o->ctx, path, 0);
}

static void track_art(CentralRun *r)
{
    ControlCentralState *st = r->st;
    char path[0x50];
    if (st->was[WAS_TRACK] != *st->track || (int16_t)st->was[WAS_M] != *st->variant_m ||
        (int16_t)st->was[WAS_R] != *st->variant_r) {
        /* the records and the picture follow the rally when both variants are on (variant_rule.h) */
        const int base = central_track_art_base(*st->variant_m, *st->variant_r, *st->variant_m != 0 && *st->variant_r != 0);
        if (st->art[ART_TRACK] != 0) r->ops->free(r->ops->ctx, st->art[ART_TRACK]);
        /* "r"'s past the tenth track is the colours' first, 9, read as a name: the original faults there */
        const int at = base + *st->track;
        sprintf(path, "%scontrol/%s", st->paths, at < 28 ? TRACK_NAMES[at] : "");
        strcat(path, ".qfs");
        /* port: Lunar Springs (lunar_springs.h) shows the moon's picture for the rally on Rusty Springs, from the German
         * art directory whatever the language */
        char moon[512];
        const int lunar = lunar_springs_picture(at, port_extras_load().lunar_springs, resource_root(), moon, sizeof moon);
        st->art[ART_TRACK] = r->ops->load_bank(r->ops->ctx, lunar ? moon : path, 0);
        st->was[WAS_TRACK] = *st->track;
        st->was[WAS_R] = (int16_t)*st->variant_r;
        st->was[WAS_M] = (int16_t)*st->variant_m;
    }
    r->ops->blit(r->ops->ctx, shape(r, ART_TRACK, "0000"));
    if (*st->race_type == 3 && st->races[*st->track] != 0) masked(r, "flad");
}

static void car_art(CentralRun *r)
{
    ControlCentralState *st = r->st;
    char path[0x50];
    if (st->was[WAS_CAR] != *st->player_car) {
        sprintf(path, "%scontrol/%s.qfs", st->paths, CAR_NAMES[*st->player_car]);
        reload(r, ART_CAR, path);
        st->was[WAS_CAR] = *st->player_car;
    }
    r->ops->blit(r->ops->ctx, shape(r, ART_CAR, "0000"));
}

static void opponent_art(CentralRun *r)
{
    ControlCentralState *st = r->st;
    char path[0x50];
    if (st->lost != 0 && *st->net_self == 0) *st->opponent = st->other_car[*st->net_player];
    if (st->lost != 0 && *st->race_type == 1) *st->opponent = st->other_car[1 - st->net_side];
    if (st->was[WAS_OPPONENT] != *st->opponent) {
        if (st->art[ART_OPPONENT] != 0) r->ops->free(r->ops->ctx, st->art[ART_OPPONENT]);
        sprintf(path, "%scontrol/%s", st->paths, OPPONENT_NAMES[*st->opponent]);
        if (*st->race_type == 3 && (*st->net_self != 0 || st->lost == 0))
            sprintf(path, *st->player_car < 3 ? "%scontrol/tourny3" : *st->player_car < 6 ? "%scontrol/tourny2"
                                                                                           : "%scontrol/tourny1",
                    st->paths);
        else if (*st->race_type == 2 && st->lost != 0 && *st->net_self != 0)
            sprintf(path, "%scontrol/%s", st->paths, OPPONENT_NAMES[*st->net_car]);
        strcat(path, ".qfs");
        st->art[ART_OPPONENT] = r->ops->load_bank(r->ops->ctx, path, 0);
        st->was[WAS_OPPONENT] = *st->opponent;
    }
    r->ops->blit(r->ops->ctx, shape(r, ART_OPPONENT, "0000"));
}

static void race_art(CentralRun *r)
{
    ControlCentralState *st = r->st;
    char path[0x50];
    if (st->was[WAS_RACE] != *st->race_type) {
        if (st->art[ART_RACE] != 0) r->ops->free(r->ops->ctx, st->art[ART_RACE]);
        const char *name = *st->race_type == 1 && *st->nomercy != 0 ? RACE_NAMES[4] : RACE_NAMES[*st->race_type];
        sprintf(path, "%scontrol/%s.qfs", st->paths, name);
        st->art[ART_RACE] = r->ops->load_bank(r->ops->ctx, path, 0);
        st->was[WAS_RACE] = *st->race_type;
    }
    r->ops->blit(r->ops->ctx, shape(r, ART_RACE, "0000"));
}

/* 00434577..004347fb: the item's light and arrow */
static void highlight(CentralRun *r)
{
    static const char *const LIGHT[4] = { "Trb3", "Tlb1", "Brb4", "Blb2" };
    static const char *const ARROW[4][2] = { { "Aro5", "Aro6" }, { "Aro1", "Aro2" }, { "Aro7", "Aro8" },
                                             { "Aro3", "Aro4" } };
    ControlCentralState *st = r->st;
    const uint16_t item = (uint16_t)r->item;
    if (item <= 3) {
        masked(r, LIGHT[item]);
        if ((int16_t)r->sub == 1) masked(r, ARROW[item][0]);
        else if ((int16_t)r->sub == 2) masked(r, ARROW[item][1]);
    } else if (item == CC_RACE) {
        blit(r, "Ra1l");
    } else if (item == CC_OPTIONS) {
        blit(r, "optl");
    } else if (item == CC_CHAT) {
        blit(r, "chal");
    } else if (item == CC_CANCEL) {
        blit(r, "canl");
    } else if (item == CC_PLAYERS) {
        if (*st->net_self != 0) blit(r, "Coml");
        else if (st->lost == 1) masked(r, "Netl");
        else masked(r, "Modl");
    }
}

/* 00434886..00434b17: a link game's first frames, the panel's art r&t1..7 */
static void intro(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    o->sound(o->ctx, 0x49);
    for (int16_t f = 1; f <= 8; f++) {
        o->wait(o->ctx, 0);
        o->select(o->ctx, *st->screen);
        o->blit(o->ctx, shape(r, ART_ANI, "R$T9"));
        player_boxes(r, 0);
        if (f != 8) {
            char tag[0x10];
            sprintf(tag, "r&t%d", f);
            o->blit_masked(o->ctx, shape(r, ART_ANI, tag));
        }
        o->deadline_set(o->ctx, 0xf);
        o->select_default(o->ctx);
        o->blit_word(o->ctx, o->picture_shape(o->ctx, *st->screen));
        o->flip(o->ctx);
        o->net_service(o->ctx);
        while (o->deadline_passed(o->ctx) == 0) {
            o->wait(o->ctx, 0);
            if (*st->pointer_active != 0) o->pointer_buttons(o->ctx);
        }
    }
}

void central_draw(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    o->select(o->ctx, *st->screen);
    r->redraw = 0;
    blit(r, "bgnd");
    if (st->lost != 0 && (st->view_only != 0 || st->players < 2 || st->e7b == 0)) masked(r, "Ra2m");
    if (st->lost == 0) masked(r, "cham");
    if ((int16_t)o->chat_waiting(o->ctx) != 0 && (int16_t)r->blink != 0) masked(r, "chal");
    if (st->lost != 0 && (*st->race_type == 2 || *st->race_type == 3))
        masked(r, *st->net_self != 0 ? "Comd" : st->lost == 1 ? "Netd" : "Modd");
    o->music(o->ctx);
    if ((int16_t)r->first == 0 && st->lost != 0) panel(r);
    track_art(r);
    car_art(r);
    if (st->view_only != 0 && (int16_t)r->first == 0) {
        o->text_select(o->ctx, 2);
        o->text_at(o->ctx, "view only", 0x20, 0x7a);
    }
    opponent_art(r);
    race_art(r);
    highlight(r);
    central_banner(r);   /* port: after the frame is blanked above, so a banner that goes leaves nothing */
    o->music(o->ctx);
    o->select_default(o->ctx);
    o->blit_word(o->ctx, o->picture_shape(o->ctx, *st->screen));
    o->flip(o->ctx);
    o->pointer_show(o->ctx);
    if ((int16_t)r->first != 0) {
        const int32_t redraw = r->redraw;
        o->music_feed(o->ctx);
        r->redraw = redraw + 1;
        if (*st->pointer_active != 0) o->pointer_buttons(o->ctx);
        o->fade_in(o->ctx, 0, 0x100, r->palette);
        o->inverse_build(o->ctx);
        o->text_styles(o->ctx);
        r->first = 0;
        if (st->lost != 0) intro(r);
        o->deadline_set(o->ctx, 0x2ee0);
    }
    o->music_feed(o->ctx);
    if (st->lost != 0 && (int16_t)r->redraw == 5) (*st->redraw_asked)--;
    r->changed = 0;
}

/* 00434b69..00434c88: the bottom row lit again for a new item */
void central_changed(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    o->select(o->ctx, *st->screen);
    r->redraw = 1;
    if (r->item != CC_NONE) o->sound(o->ctx, 0x47);
    blit(r, st->lost == 0 || (st->players > 1 && st->e7b != 0) ? "Ra3r" : "Ra2m");
    switch ((uint16_t)r->item) {
    case CC_RACE: blit(r, "Ra1l"); break;
    case CC_OPTIONS: blit(r, "optl"); break;
    case CC_CHAT: blit(r, "chal"); break;
    case CC_CANCEL: blit(r, "canl"); break;
    case CC_PLAYERS: blit(r, *st->net_self != 0 ? "Coml" : st->lost == 1 ? "Netl" : "Modl"); break;
    default: break;
    }
    r->changed = 0;
}
