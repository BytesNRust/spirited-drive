/* The in-race menu (race_menu.h, 00447564). */
#include "game/race/race_menu.h"

#include "game/sys/shape_bank.h"

#include <stdio.h>
#include <string.h>

/* The calls with no arguments, by address */
enum {
    CALL_487D38 = 0x487d38, CALL_43CBCC = 0x43cbcc, CALL_421400 = 0x421400, CALL_464534 = 0x464534,
    CALL_464568 = 0x464568, CALL_443C34 = 0x443c34, CALL_474558 = 0x474558, CALL_4735BC = 0x4735bc,
    CALL_464A10 = 0x464a10, CALL_4648DC = 0x4648dc, CALL_4640D0 = 0x4640d0, CALL_441858 = 0x441858,
    CALL_461F54 = 0x461f54, CALL_4104D0 = 0x4104d0, CALL_4C618C = 0x4c618c, CALL_473604 = 0x473604,
    CALL_4733EC = 0x4733ec, CALL_441798 = 0x441798, CALL_46376C = 0x46376c, CALL_464088 = 0x464088,
    CALL_441814 = 0x441814, CALL_43CBF4 = 0x43cbf4, CALL_40A5D8 = 0x40a5d8, CALL_46417C = 0x46417c,
    CALL_4C6194 = 0x4c6194, CALL_446284 = 0x446284, CALL_446B48 = 0x446b48, CALL_464A30 = 0x464a30,
    CALL_4644F0 = 0x4644f0, CALL_4649C8 = 0x4649c8, CALL_41E364 = 0x41e364, CALL_443820 = 0x443820,
    CALL_444394 = 0x444394, CALL_44435C = 0x44435c, CALL_456244 = 0x456244, CALL_421790 = 0x421790,
    CALL_4217A8 = 0x4217a8, CALL_42179C = 0x42179c, CALL_47463C = 0x47463c, CALL_446944 = 0x446944,
    CALL_446AF0 = 0x446af0, CALL_4557E8 = 0x4557e8, CALL_43CBE0 = 0x43cbe0, CALL_464A68 = 0x464a68, CALL_464060 = 0x464060
};
enum {
    NUMBER_441530 = 0x441530, NUMBER_46492C = 0x46492c, NUMBER_44F534 = 0x44f534, NUMBER_4015DC = 0x4015dc,
    NUMBER_423D28 = 0x423d28
};
enum { FILL_463DF0 = 0x463df0, FILL_4766B8 = 0x4766b8, PALETTE_476460 = 0x476460, PALETTE_463E7C = 0x463e7c };
enum {
    ITEM_CONTINUE, ITEM_REPLAY, ITEM_AUDIO, ITEM_GRAPHICS, ITEM_CHAT, ITEM_QUIT,
    KEY_ENTER = 0x0d, KEY_ESC = 0x1b, KEY_SPACE = 0x20, KEY_UP = 0x4800, KEY_DOWN = 0x5000,
    SOUND_MOVE = 0x47, SOUND_PICK = 0x48, REPLAY_START = 1, REPLAY_ON = 7,
    BLINK_TICKS = 0x1e, KEY_WAIT_TICKS = 0x19, END_QUIT = 2,
    HIGH_W = 0x280, HIGH_H = 0x1e0, LOW_W = 0x140, LOW_H = 200, BITS = 8, NOTICE_HELD = -1, NOTICE_TIME = 300
};

static const char *const LIT[RACE_MENU_ITEMS] = { "conl", "repl", "audl", "gral", "chal", "quil" };
static const char *const DIM[RACE_MENU_ITEMS] = { "cond", "repd", "audd", "grad", "chad", "quid" };
/* 00463f80's bands: top and bottom of each item */
static const int16_t BAND[RACE_MENU_ITEMS][2] = {
    { 0, 0x8c }, { 0x98, 0xbe }, { 0xc5, 0xec }, { 0xf7, 0x11b }, { 0x125, 0x149 }, { 0x153, 0x1e0 } };

typedef struct {
    RaceMenuWorld *w;
    const RaceMenuOps *ops;
    uint8_t *art, *map;                    /* the frame's esi (check/pause.fsh) and 0x13d8 (the track's map) */
    char name[RACE_MENU_NAME];
    uint8_t palette[RACE_MENU_PALETTE];    /* 0xc60: the art's */
    uint8_t race_palette[RACE_MENU_PALETTE];  /* 0x960: 004766b8's, put back at the end */
    uint8_t screen_palette[RACE_MENU_PALETTE];  /* 0xf60: 00463df0's */
    int16_t map_x[RACE_MENU_CARS], map_y[RACE_MENU_CARS];
    int16_t leader, cur, prev, fade_in, redraw, key_ready, mouse_up, esc_up, done, result, changed, replay, blink, chat;
    int32_t waited;
} Menu;

static int16_t get16(const uint8_t *b, int at) { return (int16_t)(b[at] | b[at + 1] << 8); }
static void put16(uint8_t *b, int at, int16_t v) { b[at] = (uint8_t)v, b[at + 1] = (uint8_t)((uint16_t)v >> 8); }

static int16_t track_of(const RaceMenuWorld *w) { return get16(w->settings, RACE_MENU_TRACK); }
static int16_t variant_of(const RaceMenuWorld *w) { return get16(w->settings, RACE_MENU_VARIANT); }

static const uint8_t *shape(const uint8_t *bank, const char *name)
{
    return shape_bank_find((uint8_t *)bank, (const uint8_t *)name);
}

static void blit(Menu *m, const uint8_t *bank, const char *name) { m->ops->blit_own(m->ops->ctx, shape(bank, name)); }
static void call(Menu *m, uint32_t address) { m->ops->call(m->ops->ctx, address); }

/* The map's point `index` (the frame's 0x0: x / divisor, -y / divisor); tracks past 8 have none */
static void map_point(int16_t track, int16_t variant, int16_t index, int16_t *x, int16_t *y)
{
    *x = *y = 0;
    if (track < 0 || track >= RACE_MENU_TRACKS) return;
    const RaceMenuMap *map = &race_menu_maps[track][track < 3 ? (variant == 0 ? 0 : variant == 1 ? 1 : 2) : 0];
    if (index < 0 || index >= map->count) return;
    *x = (int16_t)(map->points[index][0] / map->divisor);
    *y = (int16_t)(-map->points[index][1] / map->divisor);
}

/* The cars' places: a car's node (+0x48) / 4, every car in race types 2 and 3, else the player's (and in 1 the other's) */
static void place_cars(Menu *m)
{
    const RaceMenuWorld *w = m->w;
    int16_t index[RACE_MENU_CARS] = { 0 };
    if (w->race_type == 2 || w->race_type == 3) {
        for (int k = 0; k < RACE_MENU_CARS; k++) index[k] = (int16_t)(w->car_node[k] / 4);
        m->leader = w->leader;
    } else {
        index[w->player] = (int16_t)(w->car_node[w->player] / 4);
        if (w->race_type == 1) index[1 - w->player] = (int16_t)(w->car_node[1 - w->player] / 4);
    }
    for (int k = 0; k < RACE_MENU_CARS; k++)
        map_point(track_of(w), variant_of(w), index[k], &m->map_x[k], &m->map_y[k]);
}

static void pointer_to_item(Menu *m, int16_t item)
{
    m->ops->pointer_to(m->ops->ctx, race_menu_item_x[item], race_menu_item_y[item]);
}

/* The art and the map loaded, the art's palette made (each return from an option screen) */
static void load_art(Menu *m)
{
    const RaceMenuOps *o = m->ops;
    m->art = o->load(o->ctx, "check/pause.fsh");
    m->map = o->load(o->ctx, m->name);
}

static void art_palette(Menu *m)
{
    const uint8_t *pal = shape(m->art, "!pal");
    m->ops->palette(m->ops->ctx, m->palette, pal + 0x10, (int16_t)(pal[0] & 0xff));
}

/* 0x2b dwords from the saved block over the settings, and the screen's words kept in them */
static void open_menu(Menu *m)
{
    RaceMenuWorld *w = m->w;
    const RaceMenuOps *o = m->ops;
    w->in_menu = 1, w->loading = 1;
    call(m, CALL_487D38), call(m, CALL_43CBCC), call(m, CALL_421400);
    o->sounds(o->ctx, RACE_MENU_SOUNDS_OWN);
    w->unknown_4c5cca = 1;
    memcpy(w->settings, w->saved, RACE_MENU_SETTINGS);
    if (w->high == 0 && w->screen_kind > 1) w->screen_kind = 1;
    put16(w->settings, RACE_MENU_KEPT + 2, w->screen_kind);
    put16(w->settings, RACE_MENU_KEPT + 4, w->unknown_4fb785);
    put16(w->settings, RACE_MENU_KEPT + 6, w->unknown_4fb787);
    put16(w->settings, RACE_MENU_KEPT + 8, w->sky_off);
    put16(w->settings, RACE_MENU_KEPT, o->kept(o->ctx));
    o->fill(o->ctx, FILL_463DF0, m->screen_palette);
    place_cars(m);
    o->fill(o->ctx, FILL_4766B8, m->race_palette);
    if (get16(w->settings, RACE_MENU_MUSIC) != 0) call(m, CALL_464534), call(m, CALL_464568);
    call(m, CALL_443C34);
    o->fade(o->ctx, 0, 0x100);
    call(m, CALL_474558);
    if (w->high == 0)
        for (int k = 0; k < 2; k++) {
            call(m, CALL_4735BC);
            o->set_palette(o->ctx, PALETTE_476460, m->screen_palette);
            call(m, CALL_464A10);
            if (k == 0) o->mode(o->ctx, HIGH_W, HIGH_H, BITS);
        }
    o->clip(o->ctx, 0, 0, HIGH_W, HIGH_H);
    if (w->high != 0) {
        w->menu_surface = w->view_surface;
    } else {
        o->surface_free(o->ctx, w->view_surface);
        w->menu_surface = o->surface_new(o->ctx, HIGH_W, HIGH_H);
    }
    pointer_to_item(m, m->cur);
    call(m, CALL_4648DC);
    o->music(o->ctx, "pause.asf");
    const int16_t track = track_of(w);
    if (track < 3)
        snprintf(m->name, sizeof m->name, "check/%s%d.qfs", race_menu_track_names[track], variant_of(w) + 1);
    else
        snprintf(m->name, sizeof m->name, "check/%s.qfs", race_menu_track_names[track]);
    load_art(m);
    w->inventory = o->load_inventory(o->ctx, "check/pause.inv");
    call(m, CALL_4640D0);
    art_palette(m);
    o->number(o->ctx, NUMBER_441530, 0);
    call(m, CALL_441858);
    w->blink_clock = 0;
    call(m, CALL_461F54);
}

/* 00463f80 down the bands: the item under the pointer (1 only out of a replay, 4 only in one), else none */
static int16_t hovered(Menu *m)
{
    const RaceMenuOps *o = m->ops;
    for (int16_t k = 0; k < RACE_MENU_ITEMS; k++) {
        const int32_t in = o->inside(o->ctx, 0, HIGH_W, BAND[k][0], BAND[k][1]);
        if (in != 0 && !(k == ITEM_REPLAY && m->w->playback != 0) && !(k == ITEM_CHAT && m->w->playback == 0)) return k;
    }
    return RACE_MENU_NONE;
}

static void marker(Menu *m, const char *name, int car)
{
    const RaceMenuWorld *w = m->w;
    const int at = track_of(w) * 3 + variant_of(w);
    m->ops->blit_at(m->ops->ctx, shape(m->art, name), race_menu_x_offset[at] + m->map_x[car],
                    race_menu_y_offset[at] + m->map_y[car]);
}

static void draw_markers(Menu *m)
{
    const RaceMenuWorld *w = m->w;
    if (w->race_type == 2 || w->race_type == 3) {
        for (int k = 0; k < RACE_MENU_CARS; k++) marker(m, race_menu_marker_names[k], k);
        marker(m, "youl", w->player);
        marker(m, "lear", m->leader);
        return;
    }
    if (w->race_type == 1) marker(m, race_menu_marker_names[1 - w->player], 1 - w->player);
    marker(m, race_menu_marker_names[w->player], w->player);
    marker(m, "youl", w->player);
}

/* The whole screen drawn again (and faded in after an option screen) */
static void redraw(Menu *m)
{
    RaceMenuWorld *w = m->w;
    const RaceMenuOps *o = m->ops;
    o->select(o->ctx, w->menu_surface);
    blit(m, m->art, "bgnd");
    blit(m, m->map, "0000");
    if (w->playback == 0) blit(m, m->art, "cham");
    if (w->playback != 0) blit(m, m->art, "repm");
    if (o->chat(o->ctx) != 0 && m->blink != 0) blit(m, m->art, "chal");
    draw_markers(m);
    if (m->cur >= 0 && m->cur < RACE_MENU_ITEMS) blit(m, m->art, LIT[m->cur]);
    call(m, CALL_4C618C);
    call(m, CALL_473604);
    o->present(o->ctx, w->menu_surface);
    call(m, CALL_4733EC);
    call(m, CALL_441798);
    if (m->fade_in != 0) {
        call(m, CALL_441858);
        if (w->mouse_on != 0) o->buttons(o->ctx);
        call(m, CALL_46376C);
        o->set_palette(o->ctx, PALETTE_463E7C, m->palette);
        m->fade_in = 0, m->changed = 0;
    }
    m->redraw = 0;
}

/* The item left drawn dim and the new one lit */
static void highlight(Menu *m)
{
    const RaceMenuOps *o = m->ops;
    m->redraw = 1;
    o->select(o->ctx, m->w->menu_surface);
    if (m->cur != RACE_MENU_NONE) o->number(o->ctx, NUMBER_46492C, SOUND_MOVE);
    call(m, CALL_4C618C);
    if ((uint16_t)m->prev < RACE_MENU_ITEMS) blit(m, m->art, DIM[m->prev]);
    if (m->cur >= 0 && m->cur < RACE_MENU_ITEMS) blit(m, m->art, LIT[m->cur]);
    m->changed = 0;
}

/* Audio, graphics and chat: the art freed for the frontend's screen and loaded again after */
static void option_screen(Menu *m, int16_t item)
{
    const RaceMenuOps *o = m->ops;
    o->fade(o->ctx, 0, 0x100);
    call(m, CALL_4640D0);
    call(m, CALL_474558);
    o->unload(o->ctx, m->map);
    o->unload(o->ctx, m->art);
    if (item == ITEM_AUDIO) o->number(o->ctx, NUMBER_4015DC, 2);
    else if (item == ITEM_GRAPHICS) o->number(o->ctx, NUMBER_423D28, 1);
    else call(m, CALL_40A5D8);
    load_art(m);
    m->w->inventory = o->load_inventory(o->ctx, "check/pause.inv");
    art_palette(m);
    call(m, CALL_4640D0);
    pointer_to_item(m, m->cur);
    m->redraw = 1, m->fade_in = 1;
}

static void pick(Menu *m)
{
    RaceMenuWorld *w = m->w;
    const RaceMenuOps *o = m->ops;
    o->number(o->ctx, NUMBER_46492C, SOUND_PICK);
    switch (m->cur) {
    case ITEM_CONTINUE:
        m->done = m->result = 1;
        break;
    case ITEM_REPLAY:
        o->number(o->ctx, NUMBER_44F534, REPLAY_START);
        m->replay = m->done = m->result = 1;
        call(m, CALL_43CBF4);
        break;
    case ITEM_AUDIO:
    case ITEM_GRAPHICS:
    case ITEM_CHAT:
        option_screen(m, m->cur);
        break;
    case ITEM_QUIT: {
        o->unload(o->ctx, m->map);
        o->unload(o->ctx, m->art);
        const int16_t yes = o->ask(o->ctx, w->german != 0 ? "Spiel beenden ?" : "End game ?");
        load_art(m);
        if (yes != 0) {
            w->end_state = END_QUIT;
            m->done = 1;
            if (w->playback != 0) w->dropped = 1;
        }
        break;
    }
    default:
        break;
    }
}

/* Up and down skip the item the race has not got (replay in a replay, chat out of one) */
static void step(Menu *m, uint16_t key)
{
    const int16_t playback = m->w->playback;
    call(m, CALL_464060);
    m->prev = m->cur;
    if (key == KEY_UP) {
        if (--m->cur < 0) m->cur = ITEM_QUIT;
        if (playback == 0 && m->cur == ITEM_CHAT) m->cur = ITEM_GRAPHICS;
        if (playback != 0 && m->cur == ITEM_REPLAY) m->cur = ITEM_CONTINUE;
    } else {
        m->cur = (int16_t)((m->cur + 1) % RACE_MENU_ITEMS);
        if (playback == 0 && m->cur == ITEM_CHAT) m->cur = ITEM_QUIT;
        if (playback != 0 && m->cur == ITEM_REPLAY) m->cur = ITEM_AUDIO;
    }
    pointer_to_item(m, m->cur);
    m->changed = 1;
}

static void keys(Menu *m)
{
    RaceMenuWorld *w = m->w;
    const RaceMenuOps *o = m->ops;
    if (m->key_ready == 0) {
        m->waited += o->ticks(o->ctx);
        if ((int16_t)m->waited > KEY_WAIT_TICKS) m->key_ready = 1;
    }
    if (w->mouse_on != 0) w->mouse_buttons = o->buttons(o->ctx);
    if (w->mouse_buttons == 0) m->mouse_up = 1;
    call(m, CALL_464088);
    uint16_t key = o->key(o->ctx);
    if (w->mouse_buttons == 2 && m->mouse_up != 0) m->cur = ITEM_QUIT, key = KEY_ENTER, m->key_ready = 1;
    if (key == KEY_SPACE || key == KEY_ENTER || (w->mouse_buttons == 1 && m->mouse_up != 0)) {
        call(m, CALL_441814);
        if (m->key_ready != 0) pick(m);
        m->mouse_up = m->key_ready = 0, m->waited = 0;
        o->ticks(o->ctx);
        return;
    }
    if (key == 0 || m->key_ready == 0) {
        if (key == 0) m->key_ready = 1;
        return;
    }
    if (key == KEY_ESC && m->esc_up != 0) {
        while (w->esc_down != 0) {
            call(m, CALL_46417C);
            if (w->mouse_on != 0) call(m, CALL_4C6194);
        }
        if (w->playback == 0) m->done = m->result = 1;
    } else if (key == KEY_UP || key == KEY_DOWN) {
        step(m, key);
    }
    o->ticks(o->ctx);
    m->key_ready = 0, m->waited = 0;
}

static void run(Menu *m)
{
    RaceMenuWorld *w = m->w;
    const RaceMenuOps *o = m->ops;
    while (m->done == 0) {
        call(m, CALL_4104D0);
        if (w->esc_down == 0) m->esc_up = 1;
        const int16_t item = hovered(m);
        if (item != m->cur) m->prev = m->cur, m->cur = item, m->changed = 1;
        if (w->blink_clock > BLINK_TICKS) {
            m->blink = (int16_t)(1 - m->blink);
            w->blink_clock = 0;
            const int16_t chat = o->chat(o->ctx);
            if (chat != 0) m->chat = 1, m->redraw = 1;
            else if (m->chat != 0) m->chat = chat, m->redraw = 1;
        }
        if (m->redraw != 0) redraw(m);
        if (m->changed != 0) highlight(m);
        keys(m);
        if (m->done == 0) {
            call(m, CALL_46417C);
            if (w->playback == 0 && m->cur == ITEM_CHAT) {
                m->cur = ITEM_QUIT;
                pointer_to_item(m, ITEM_QUIT);
                m->prev = ITEM_QUIT, m->redraw = 1;
            }
        }
        if (w->dropped != 0) m->done = 1, m->result = 0, w->end_state = END_QUIT;
        else if (w->playback != 0 && w->unknown_50a5b6 == 1) m->done = m->result = 1;
    }
}

static void lost(Menu *m)
{
    RaceMenuWorld *w = m->w;
    m->ops->notice(m->ops->ctx, w->german != 0 ? "Verbindung abgebrochen" : "Connection Aborted", NOTICE_TIME);
    w->dropped = 1, w->end_state = END_QUIT, m->result = 0;
}

/* A network game (004fb7d4 1): the others told, or the game dropped */
static void net_end(Menu *m)
{
    RaceMenuWorld *w = m->w;
    const RaceMenuOps *o = m->ops;
    if (w->dropped != 0) {
        o->notice(o->ctx, w->german != 0 ? "Verbindung abgebrochen" : "Connection Terminated", NOTICE_HELD);
        m->result = 0, w->end_state = END_QUIT;
    } else if (m->result == 0) {
        w->dropped = 1;
        o->notice(o->ctx, w->german != 0 ? "Verbindung abgebrochen" : "Connection Aborted", NOTICE_TIME);
        w->end_state = END_QUIT;
    } else if (w->unknown_4fbb0c != 0) {
        call(m, CALL_446284);
        if (w->dropped != 0) lost(m);
    } else {
        o->notice(o->ctx, w->german != 0 ? "Warte auf andere Spieler..." : "Waiting for other players...", NOTICE_HELD);
        o->net_send(o->ctx, -1);
        if (o->net_wait(o->ctx, 1) != 0) w->unknown_50a5b6 = 0;
        else lost(m);
    }
}

static void race_screen_back(Menu *m)
{
    RaceMenuWorld *w = m->w;
    const RaceMenuOps *o = m->ops;
    o->fade(o->ctx, 0, 0x100);
    call(m, CALL_446B48);
    if (w->mouse_on != 0) call(m, CALL_4C618C);
    call(m, CALL_464A30);
    o->unload(o->ctx, m->map);
    o->unload(o->ctx, m->art);
    call(m, CALL_474558), call(m, CALL_4644F0), call(m, CALL_4649C8), call(m, CALL_464568);
    if (w->high == 0) {
        o->surface_free(o->ctx, w->menu_surface);
        w->view_surface = o->surface_new(o->ctx, LOW_W, LOW_H);
    }
    memcpy(w->saved, w->settings, RACE_MENU_SETTINGS);
    if (w->high == 0)
        for (int k = 0; k < 2; k++) {
            call(m, CALL_4735BC);
            o->set_palette(o->ctx, PALETTE_476460, m->screen_palette);
            call(m, CALL_464A10);
            if (k == 0) o->mode(o->ctx, LOW_W, LOW_H, BITS);
        }
    if (w->unknown_4c5cbe != 0) {
        put16(w->settings, RACE_MENU_UNKNOWN_E69, w->unknown_52b328);
        put16(w->settings, RACE_MENU_UNKNOWN_E67, w->unknown_52b326);
        w->unknown_4c5cbe = 0;
        put16(w->settings, RACE_MENU_MUSIC_VOLUME, w->unknown_52b322);
    }
}

static void close_menu(Menu *m)
{
    RaceMenuWorld *w = m->w;
    const RaceMenuOps *o = m->ops;
    race_screen_back(m);
    const int p = w->player;
    w->unknown_4fb817[p] = w->end_state == END_QUIT ? 0 : get16(w->settings, RACE_MENU_UNKNOWN_E69);
    w->unknown_4fb81b[p] = w->end_state == END_QUIT ? 0 : get16(w->settings, RACE_MENU_MUSIC_VOLUME);
    w->unknown_4fb784 = w->settings[RACE_MENU_KEPT];
    w->screen_kind = w->settings[RACE_MENU_KEPT + 2];
    w->unknown_4fb785 = w->settings[RACE_MENU_KEPT + 4];
    w->unknown_4fb787 = w->settings[RACE_MENU_KEPT + 6];
    memcpy(w->saved, w->settings, RACE_MENU_SETTINGS);
    w->sky_off = w->settings[RACE_MENU_KEPT + 8];
    if (w->unknown_4f9b7c != 0) {
        w->unknown_4b6bb0 = 1;
    } else {
        w->unknown_4b6bb0 = 0;
        call(m, CALL_41E364);
    }
    o->take(o->ctx, w->follow);
    o->view_screen(o->ctx, w->unknown_4fb784, w->screen_kind > 2 ? 2 : w->screen_kind);
    if (m->replay == 0 && w->phase != 1) o->number(o->ctx, NUMBER_44F534, REPLAY_ON);
    if (w->end_state != END_QUIT) {
        call(m, CALL_443820);
        call(m, w->player_unknown_512 == 2 || w->player_unknown_512 == 1 ? CALL_444394 : CALL_44435C);
    }
    w->unknown_4c5cca = 0;
    if (m->result != 0 && get16(w->settings, RACE_MENU_MUSIC) != 0) {
        w->unknown_4c5cca = 1;
        call(m, CALL_456244), call(m, CALL_4640D0);
    }
    call(m, CALL_474558);
    o->palette(o->ctx, m->palette, m->race_palette, 0x24);
    o->set_palette(o->ctx, PALETTE_476460, m->palette);
    const int32_t kept = w->unknown_509e04;
    call(m, CALL_421790), call(m, CALL_4217A8), call(m, CALL_42179C);
    w->unknown_509e04 = kept;
    call(m, CALL_47463C), call(m, CALL_446944);
    o->number(o->ctx, NUMBER_441530, 1);
    call(m, CALL_446AF0);
    w->unknown_513e50 = 0;
    call(m, CALL_4557E8);
    o->sounds(o->ctx, RACE_MENU_SOUNDS_RACE);
    call(m, CALL_441858);
    call(m, m->result != 0 ? CALL_43CBE0 : CALL_43CBF4);
    if (w->playback != 0 && w->dropped == 0) {
        o->net_send(o->ctx, -1);
        if (o->net_wait(o->ctx, 8) == 0) w->dropped = 1;
    }
    if (w->dropped != 0) w->end_state = END_QUIT;
    w->in_menu = 0, w->loading = 0;
    if (w->unknown_4bafe0 != 0) call(m, CALL_464A68);
}

void race_menu(RaceMenuWorld *w, const RaceMenuOps *ops)
{
    Menu m = { .w = w, .ops = ops, .fade_in = 1, .redraw = 1, .key_ready = 1, .mouse_up = 1 };
    open_menu(&m);
    run(&m);
    if (w->net_game == 1) net_end(&m);
    close_menu(&m);
}
