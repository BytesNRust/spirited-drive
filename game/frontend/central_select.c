/* Control Central (00432ff4): Enter, space or the left button on an item (00435120..004360bc). */
#include "game/frontend/central_run.h"
#include "game/frontend/car_list.h"
#include "game/frontend/track_list.h"
#include "game/frontend/showcase.h"

#include <stddef.h>

static int in(CentralRun *r, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    return r->ops->in_rect(r->ops->ctx, x0, x1, y0, y1) != 0;
}

static int warrior_rival(const ControlCentralState *st) { return st->warrior_rival != NULL && *st->warrior_rival != 0; }

const Edition *central_edition_of(const ControlCentralState *st) { return st->edition != NULL && st->edition->cars > 0 ? st->edition : NULL; }

/* port (dep59): an edition's cars are the arrows', outside a link game */
static int added_on(const ControlCentralState *st) { return central_edition_of(st) != NULL && st->lost == 0; }

/* port (dep59): `*car` stepped round the edition's cars (car_list.h) */
static void list_step(const ControlCentralState *st, int16_t *car, int step)
{
    CarList l;
    car_list_build(&l, central_edition_of(st), 0);
    *car = car_list_step(&l, *car, step);
}

/* the opponent (in a link game the player picked, or a single race's car) one on */
void central_opponent_up(CentralRun *r, int mouse)
{
    ControlCentralState *st = r->st;
    (void)mouse;
    if (st->lost != 0 && *st->net_self == 0) {
        *st->net_player = *st->net_player + 1 < st->players ? (int16_t)(*st->net_player + 1) : 0;
    } else if (*st->race_type == 2 && st->lost == 0) {
        if (*st->opponent == 11) *st->opponent = 0;
        else if (++*st->opponent > 8) *st->opponent = 0;
    } else if (*st->race_type == 2) {
        if (*st->net_self == 0) return;
        st->was[WAS_OPPONENT] = CENTRAL_NO_ART;
        *st->net_car = (int16_t)((*st->net_car + 1) % 9);
    } else if (*st->race_type == 1 && added_on(st)) {
        list_step(st, st->opponent, 1);   /* port (dep59): the head to head rival from the edition's cars */
    } else if (*st->opponent == 11) {
        *st->opponent = 0;
    } else if (++*st->opponent > 7) {
        *st->opponent = warrior_rival(st) ? 11 : 0;  /* port: the extra */
    }
    r->redraw = 1;
}

void central_opponent_down(CentralRun *r, int mouse)
{
    ControlCentralState *st = r->st;
    if (st->lost != 0 && *st->net_self == 0) {
        if (mouse) *st->net_player_was = -1;
        const int back = mouse ? *st->net_player > 0 : *st->net_player != 0;
        *st->net_player = back ? (int16_t)(*st->net_player - 1) : (int16_t)(st->players - 1);
    } else if (*st->race_type == 2 && st->lost == 0) {
        *st->opponent = *st->opponent == 11 ? 8 : (int16_t)(*st->opponent - 1);
        if (*st->opponent < 0) *st->opponent = 8;
    } else if (*st->race_type == 2) {
        if (*st->net_self == 0) return;
        st->was[WAS_OPPONENT] = CENTRAL_NO_ART;
        if (--*st->net_car < 0) *st->net_car = 8;
    } else if (*st->race_type == 1 && added_on(st)) {
        list_step(st, st->opponent, -1);
    } else {
        *st->opponent = *st->opponent == 11 ? 7 : (int16_t)(*st->opponent - 1);
        if (*st->opponent < 0) *st->opponent = warrior_rival(st) ? 11 : 7;  /* port: the extra */
    }
    r->redraw = 1;
}

/* the screen's start sends the Warrior (11) back to car 0; port: not when the extra keeps it and it is on offer. port (dep59): an added
 * car (the standard edition offers none) back to car 0 too, the player's and the rival's. port (dep59 step 4): in an edition, a car it
 * does not list becomes its first, the player's and the head to head rival */
void central_car_start(ControlCentralState *st)
{
    const Edition *e = central_edition_of(st);
    if (e != NULL) {
        CarList l;
        car_list_build(&l, e, 0);
        const int16_t first = car_list_step(&l, -1, 1);   /* (no car is -1: the list's first, or 0 for none) */
        if (!car_list_has(&l, *st->player_car)) *st->player_car = first;
        if (st->opponent != NULL && st->race_type != NULL && *st->race_type == 1 && !car_list_has(&l, *st->opponent)) *st->opponent = first;
        return;
    }
    const int keep = st->warrior_keep != NULL && *st->warrior_keep != 0 && *st->warrior != 0;
    if (*st->player_car == 11 && !keep) *st->player_car = 0;
    if (*st->player_car >= CAR_ADDED_FIRST) *st->player_car = 0;
    if (st->opponent != NULL && *st->opponent >= CAR_ADDED_FIRST) *st->opponent = 0;
}

/* port (dep59 step 4): the screen's start in an edition: a track it does not list becomes its first (track_list.h); the standard edition
 * leaves the track to the original's start */
void central_track_start(ControlCentralState *st)
{
    if (st->edition == NULL || st->edition->tracks == 0 || st->track == NULL) return;
    TrackList l;
    track_list_build(&l, st->edition, 0, TRACK_LIST_LUNAR_NEVER);
    if (l.count > 0 && !track_list_has(&l, *st->track)) *st->track = l.track[0];
}

/* the player's car one on: a tournament's class by track (0..2, 3..5, 6..7), else 0..7 and the Warrior */
void central_car_up(CentralRun *r)
{
    ControlCentralState *st = r->st;
    if (*st->race_type != 3 && added_on(st)) {
        list_step(st, st->player_car, 1);   /* port (dep59): the edition's cars */
    } else if (*st->race_type == 3) {
        const int16_t t = *st->track;
        ++*st->player_car;
        if (t == 0 || t == 3) {
            if (*st->player_car > 2) *st->player_car = 0;
        } else if (t == 1 || t == 4 || t == 5) {
            if (*st->player_car > 5) *st->player_car = 3;
        } else if (*st->player_car > 7) {
            *st->player_car = 6;
        }
    } else if (*st->warrior != 0 && *st->player_car == 7) {
        *st->player_car = 11;
    } else if (++*st->player_car > 7) {
        *st->player_car = 0;
    }
    r->ops->car_flags(r->ops->ctx);
    r->redraw = 1;
}

void central_car_down(CentralRun *r)
{
    ControlCentralState *st = r->st;
    if (*st->race_type != 3 && added_on(st)) {
        list_step(st, st->player_car, -1);
        r->ops->car_flags(r->ops->ctx);
        r->redraw = 1;
        return;
    }
    *st->player_car = *st->player_car == 11 ? 7 : (int16_t)(*st->player_car - 1);
    if (*st->race_type == 3) {
        const int16_t t = *st->track;
        if (t == 0 || t == 3) {
            if (*st->player_car < 0) *st->player_car = 2;
        } else if (t == 1 || t == 4 || t == 5) {
            if (*st->player_car < 3) *st->player_car = 5;
        } else if (*st->player_car < 6) {
            *st->player_car = 7;
        }
    } else if (*st->player_car < 0) {
        *st->player_car = *st->warrior != 0 ? 11 : 7;
    }
    r->ops->car_flags(r->ops->ctx);
    r->redraw = 1;
}

/* the palette again, the pointer on the item and the screen faded in again */
static void again(CentralRun *r)
{
    central_palette(r);
    central_move(r);
    r->first = 1;
    r->redraw = 1;
}

static void showcase_start(CentralRun *r)
{
    const ControlCentralOps *o = r->ops;
    o->music_fade_out(o->ctx);
    o->fade_out(o->ctx, 0, 0x100);
    o->music_end(o->ctx);
    o->music_start(o->ctx, "showcase.asf");
    r->st->was[WAS_OPPONENT] = CENTRAL_NO_ART;
}

/* port (owner 2026-11-17): the showcase's music ended after the pages that leave it playing (an added car's, the Warrior's, the carousel's) */
static void showcase_music_end(CentralRun *r)
{
    const ControlCentralOps *o = r->ops;
    o->music_fade_out(o->ctx);
    o->music_end(o->ctx);
}

/* port: the gallery has the 8 cars (showcase.c), the field (8) showing car 0; the original opened it on any rival and
 * read past its names with the Warrior (11, the port's Warrior-as-a-rival extra), which now leaves the card as the
 * player's own card does with the Warrior */
static int rival_shown(int16_t opponent) { return opponent >= 0 && opponent <= SHOWCASE_CARS; }

static void opponent_item(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    if (in(r, 0x25d, 0x270, 0x91, 0xa8)) {
        central_opponent_up(r, 1);
    } else if (in(r, 0x155, 0x16a, 0x91, 0xa8)) {
        central_opponent_down(r, 1);
    } else if (st->lost != 0) {
        if (st->net_host != 0 && *st->net_player != 0 && *st->net_self == 0) o->kick(o->ctx, *st->net_player);
    } else if (st->no_music == 0 && *st->player_car != 11 && rival_shown(*st->opponent)) {
        showcase_start(r);
        const int16_t car = *st->player_car;
        *st->player_car = *st->opponent == 8 ? 0 : *st->opponent;
        o->showcase(o->ctx);
        o->showcase_after(o->ctx);
        *st->opponent = *st->player_car;
        *st->player_car = car;
        again(r);
    }
}

/* port (dep59 step 3): an added car's gallery; when its Previous / Next went to one of the eight, that car's gallery after it (the
 * Warrior has none: back here). The car it ends on is the player's. Nothing without the op. */
static void added_gallery(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    if (o->added_gallery == NULL) return;
    showcase_start(r);
    if (o->added_gallery(o->ctx, st->player_car, *st->warrior != 0, r->palette) && *st->player_car >= 0 && *st->player_car < SHOWCASE_CARS) {
        o->showcase(o->ctx);   /* the original's showcase ends the music itself */
    } else {
        showcase_music_end(r);   /* the page leaves it playing (owner 2026-11-17) */
    }
    o->showcase_after(o->ctx);
    o->car_flags(o->ctx);
    again(r);
}

/* port (dep59 section 8): a car whose page is the added_gallery op's: an added car's, the Warrior's */
static int has_page(int16_t car) { return car_registry_is_added(car) || car == 11; }

/* port (dep59 section 4): the owner's carousel, in Restomod (an edition in use) outside a tournament and not lost, with both pages' ops */
static int carousel_on(const ControlCentralState *st, const ControlCentralOps *o)
{
    return o->showcase_carousel != NULL && o->added_gallery != NULL && added_on(st) && *st->race_type != 3;
}

/* one carousel over the edition's lineup from the player's car: an original's showcase, which leaves on Next / Previous; an added car's
 * page or the Warrior's, which go round those pages themselves and hand back on an original; the next car in the edition's order after a
 * showcase. The car shown when Done is taken is the player's */
static void carousel(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    const int warrior = st->warrior != NULL && *st->warrior != 0;
    int16_t *const car = st->player_car;
    CarList l;
    car_list_build(&l, central_edition_of(st), warrior);
    showcase_start(r);
    for (;;) {
        if (has_page(*car) && !o->added_gallery(o->ctx, car, warrior, r->palette)) break;
        if (*car < 0 || *car >= SHOWCASE_CARS) break;   /* only an original's showcase from here */
        const int step = o->showcase_carousel(o->ctx);
        if (step == 0) break;
        *car = car_list_step_gallery(&l, *car, step);
    }
    showcase_music_end(r);   /* the pages leave the showcase's music playing from one to the next: it ends once, here (owner 2026-11-17) */
    o->showcase_after(o->ctx);
    o->car_flags(o->ctx);
    again(r);
}

static void car_item(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    if (in(r, 0x136, 0x149, 0xba, 0xd1)) {
        central_car_up(r);
    } else if (in(r, 0x15, 0x28, 0xba, 0xd1)) {
        central_car_down(r);
    } else if (carousel_on(st, o) && (has_page(*st->player_car) || (*st->player_car >= 0 && *st->player_car < SHOWCASE_CARS))) {
        carousel(r);   /* port (dep59 section 4) */
    } else if (st->lost == 0 && car_registry_is_added(*st->player_car)) {
        added_gallery(r);   /* port (dep59): an added car's own gallery */
    } else if (st->lost == 0 && *st->player_car >= 0 && *st->player_car < SHOWCASE_CARS) {   /* port (dep59): not 11, nor an added car */
        showcase_start(r);
        o->showcase(o->ctx);
        o->showcase_after(o->ctx);
        again(r);
    }
}

static void track_item(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    if (in(r, 0x20f, 0x224, 0x131, 0x14e)) {
        o->track_forward(o->ctx);
        r->redraw = 1;
    } else if (in(r, 0x155, 0x168, 0x131, 0x14e)) {
        o->track_backward(o->ctx);
        r->redraw = 1;
    } else if (st->no_music == 0 && *st->track != 9) {
        showcase_start(r);
        o->free_inverse(o->ctx);
        o->track_select(o->ctx);
        o->showcase_after(o->ctx);
        *st->inverse = o->load_inverse(o->ctx, "control/central.inv");
        again(r);
    }
}

static void race_type_item(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    const int32_t panel = r->panel + 1;
    if (in(r, 0x136, 0x145, 0x154, 0x16d)) {
        o->race_type_step(o->ctx, 0x4d00);
    } else if (in(r, 0x5c, 0x6b, 0x154, 0x16d)) {
        o->race_type_step(o->ctx, 0x4b00);
    } else {
        int16_t sum = 0;
        for (int i = 0; i < 9; i++) sum = (int16_t)(sum + st->races[i]);
        if (sum == 0) return;
        o->fade_out(o->ctx, 0, 0x100);
        o->free_inverse(o->ctx);
        o->standings(o->ctx, 0);
        *st->inverse = o->load_inverse(o->ctx, "control/central.inv");
        again(r);
        return;
    }
    r->panel = panel;
    st->was[WAS_OPPONENT] = CENTRAL_NO_ART;
    r->redraw = 1;
}

/* Options or chat: the art freed around it, loaded again */
static void menu_item(CentralRun *r, int options)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    o->fade_out(o->ctx, 0, 0x100);
    o->music_feed(o->ctx);
    o->art_free(o->ctx);
    if (options) {
        o->options(o->ctx);
        const int16_t vegas = (int16_t)*st->lost_vegas;
        if (r->race_was != *st->race_type) {
            o->type_changed(o->ctx);
            r->race_was = *st->race_type;
            r->changed = 1;
        }
        *st->lost_vegas = vegas;
    } else {
        o->chat(o->ctx);
    }
    central_load(r);
    r->pal = o->shape(o->ctx, st->art[ART_CENTRAL], "!pal");
    central_palette(r);
    if (!options) o->text_styles(o->ctx);
    o->music_feed(o->ctx);
    central_move(r);
    if (options) *st->net_self = 0;
    r->first = 1;
    r->redraw = 1;
}

static void cancel_item(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    if (*st->tournament != 0)
        r->answer = o->yes_no(o->ctx, st->german != 0 ? "Wettkampf beenden ?" : "Quit tournament ?");
    if ((int16_t)r->answer == 0 && *st->tournament != 0) return;
    o->drain(o->ctx);
    r->done = 1;
    r->result = 0;
    if (st->lost != 0) *st->link_lost = 1;
}

/* the race: 1 to leave Control Central with r->result */
static int race_item(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    if (*st->race_type == 3 && st->races[*st->track] != 0) {
        o->banner(o->ctx, st->german != 0 ? "Strecke bereits fertiggestellt" : "Track already completed");
        o->deadline_set(o->ctx, 0x64);
        while (o->deadline_passed(o->ctx) == 0) {
            o->music(o->ctx);
            o->pointer_buttons(o->ctx);
        }
        o->deadline_set(o->ctx, 0x2ee0);
        r->redraw = 1;
        return 0;
    }
    o->pre_race(o->ctx);
    *st->tournament = 1;
    if (*st->race_type == 3) {
        *st->explode = 0, *st->variant_r = 0, *st->warrior = 0;
        *st->track_flags &= 0xfff7;
    }
    o->art_free(o->ctx);
    o->music_fade_out(o->ctx);
    o->sound_free(o->ctx);
    o->fade_out(o->ctx, 0, 0x100);
    o->music_end(o->ctx);
    o->picture_free(o->ctx, *st->screen);
    o->flush(o->ctx);
    if (*st->music_silent != 0) {
        st->volumes[2] = st->kept[3], st->volumes[1] = st->kept[2];
        *st->music_silent = 0;
        st->volumes[0] = st->kept[0];
    }
    if (*st->link_lost != 0) {
        *st->pointer_active = 1;
        *st->screen = o->picture_new(o->ctx, 0x280, 0x1e0, 0);
        r->result = 2;
        return 1;
    }
    o->race(o->ctx);
    *st->nomercy = 0;
    st->was[WAS_RACE] = CENTRAL_NO_ART;
    *st->track_flags &= 0xfffb;
    *st->pointer_active = 1;
    *st->screen = o->picture_new(o->ctx, 0x280, 0x1e0, 0);
    if (*st->link_lost != 0) {
        r->result = 0;
        return 1;
    }
    o->music(o->ctx);
    central_load(r);
    o->sound_load(o->ctx);
    r->pal = o->shape(o->ctx, st->art[ART_CENTRAL], "!pal");
    central_palette(r);
    o->music(o->ctx);
    r->first = 1;
    r->redraw = 1;
    r->item = CC_CAR;
    central_move(r);
    o->deadline_set(o->ctx, 0x2ee0);
    return 0;
}

int central_select(CentralRun *r)
{
    ControlCentralState *st = r->st;
    const ControlCentralOps *o = r->ops;
    o->pointer_click(o->ctx);
    if ((int16_t)r->keys_armed == 0) return 0;
    o->sound(o->ctx, 0x48);
    if (*st->net_second != 0 && st->lost != 0 && st->net_host != 0) {
        o->kick(o->ctx, 1);
        return 0;
    }
    switch ((uint16_t)r->item) {
    case CC_PLAYERS:
        st->was[WAS_OPPONENT] = CENTRAL_NO_ART;
        *st->net_self ^= 1;
        central_move(r);
        r->panel++;
        if (st->lost != 0 && *st->net_self == 0) *st->opponent = st->other_car[*st->net_player];
        r->redraw = 1;
        break;
    case CC_OPPONENT: opponent_item(r); break;
    case CC_CAR: car_item(r); break;
    case CC_TRACK: track_item(r); break;
    case CC_RACE_TYPE: race_type_item(r); break;
    case CC_OPTIONS: menu_item(r, 1); break;
    case CC_CHAT: menu_item(r, 0); break;
    case CC_CANCEL: cancel_item(r); break;
    case CC_RACE: return race_item(r);
    default: break;
    }
    return 0;
}
