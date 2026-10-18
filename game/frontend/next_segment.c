#include "game/frontend/next_segment.h"

#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, DOWN = 0x5000, LEFT_BUTTON = 1 };
enum { CONTINUE, REPLAY, HIGHLIGHTS, OPTIONS, RESULTS, CHAT, QUIT, ITEMS };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x2ee0, REST = 0x19, BLINK = 0x1e, WIDE = 0x280, TALL = 0x1e0,
       ALL = 0x100, SETTLE = 0xc8 };

/* 0040b5c0 / 0040b5ce: where the pointer goes per item */
static const int16_t POINTER_X[ITEMS] = { 320, 320, 320, 320, 320, 320, 320 };
static const int16_t POINTER_Y[ITEMS] = { 128, 173, 219, 266, 312, 358, 404 };
/* the bands 00463f80 is asked about, in turn */
static const int16_t BAND_TOP[ITEMS] = { 0, 0x9c, 0xcb, 0xf7, 0x125, 0x153, 0x180 };
static const int16_t BAND_BOTTOM[ITEMS] = { 0x91, 0xbe, 0xec, 0x11b, 0x149, 0x177, 0x1e0 };
static const char *const LIT[ITEMS] = { "conl", "repl", "higl", "gmol", "gmsl", "chal", "quil" };
static const char *const DARK[ITEMS] = { "cond", "repd", "higd", "gmod", "gmsd", "chad", "quid" };

typedef struct Menu {
    int32_t bank;      /* esi */
    int16_t selected;  /* esp + 0xc70 */
    int16_t previous;  /* esp + 0xc78 */
    int changed;       /* esp + 0xc6c */
    int redraw;        /* esp + 0xc68 */
    int first;         /* esp + 0xc74: the palette faded in on the next draw */
    int armed;         /* esp + 0xc60: the keys taken */
    int32_t rest;      /* esp + 0xc54: the ticks summed while a key is held */
    int released;      /* esp + 0xc4c: the buttons seen up */
    int done;          /* esp + 0xc64 */
    int16_t music_on;  /* esp + 0xc48: the music faded out and ended on the way out, else fed */
    int32_t blink;     /* esp + 0xc50 */
    int16_t chatting;  /* esp + 0xc5c: a chat message was waiting */
    int16_t result;    /* esp + 0xc58 */
    uint8_t black[AFTER_RACE_PALETTE];  /* esp + 0x5b8: cleared, put after the replay */
    uint8_t rgb[AFTER_RACE_PALETTE];    /* esp + 0x8b8: the art's, faded in */
} Menu;

static void move_to(const Menu *m, const AfterRaceOps *o)
{
    o->pointer_move_to(o->ctx, POINTER_X[m->selected], POINTER_Y[m->selected]);
}

static void load_art(Menu *m, const AfterRaceState *st, const AfterRaceOps *o)
{
    m->bank = o->load_art(o->ctx, "check/chkmenu.qfs");
    *st->inverse = o->load_inverse(o->ctx, "check/chkmenu.inv");
}

static void blit(const Menu *m, const char *tag, const AfterRaceOps *o)
{
    o->blit_own(o->ctx, o->shape(o->ctx, m->bank, tag));
}

static void palette_set(Menu *m, const AfterRaceOps *o)
{
    const uint8_t *pal = o->shape(o->ctx, m->bank, "!pal");
    o->palette_set(o->ctx, m->rgb, 0, ALL, pal + 0x10, pal[0]);
}

/* the replay and the highlights only outside a link game, the chat only in one */
static int offered(const AfterRaceState *st, int16_t i)
{
    if (i == REPLAY || i == HIGHLIGHTS) return *st->link == 0;
    if (i == CHAT) return *st->link != 0;
    return 1;
}

/* the first band the pointer is in, of an item offered, becomes the selection */
static void follow(Menu *m, const AfterRaceState *st, const AfterRaceOps *o)
{
    for (int16_t i = 0; i < ITEMS; i++) {
        if (!o->pointer_in_rect(o->ctx, 0, WIDE, BAND_TOP[i], BAND_BOTTOM[i]) || !offered(st, i)) continue;
        if (m->selected != i) {
            m->previous = m->selected;
            m->selected = i;
            m->changed = 1;
        }
        return;
    }
}

/* every 0x1e clock ticks: the blink turns over, and a chat message waiting (or one no longer) redraws */
static void blink(Menu *m, const AfterRaceState *st, const AfterRaceOps *o)
{
    if (*st->clock <= BLINK) return;
    m->blink = 1 - m->blink;
    *st->clock = 0;
    if ((int16_t)o->chat_waiting(o->ctx)) {
        m->chatting = 1;
        m->redraw = 1;
    } else if (m->chatting) {
        m->chatting = 0;
        m->redraw = 1;
    }
}

static void draw(Menu *m, const AfterRaceState *st, const AfterRaceOps *o)
{
    o->select(o->ctx, *st->screen);
    blit(m, "bgnd", o);
    if (*st->link) blit(m, "chad", o), blit(m, "repm", o), blit(m, "higm", o);
    if ((int16_t)o->chat_waiting(o->ctx) && (int16_t)m->blink)
        o->blit_masked(o->ctx, o->shape(o->ctx, m->bank, "chal"));
    if ((uint16_t)m->selected < ITEMS) blit(m, LIT[m->selected], o);
    o->select_default(o->ctx);
    o->pointer_hide(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, *st->screen));
    o->flip(o->ctx);
    o->pointer_show(o->ctx);
    if (m->first) {
        o->pointer_mark(o->ctx);
        if (*st->pointer_active) o->pointer_buttons(o->ctx);
        palette_set(m, o);
        o->text_styles(o->ctx);
        o->fade_in(o->ctx, 0, ALL, m->rgb);
        m->first = 0;
    }
    o->deadline_set(o->ctx, WAIT);
    m->changed = 0;
    m->redraw = 0;
}

/* the item left dark, the new one lit; the screen stays locked and is drawn again next time */
static void redraw_items(Menu *m, const AfterRaceState *st, const AfterRaceOps *o)
{
    o->select(o->ctx, *st->screen);
    m->redraw = 1;
    o->sound(o->ctx, MOVED);
    o->pointer_hide(o->ctx);
    o->lock(o->ctx);
    if ((uint16_t)m->previous < ITEMS) blit(m, DARK[m->previous], o);
    if ((uint16_t)m->selected < ITEMS) blit(m, LIT[m->selected], o);
    m->changed = 0;
}

/* back from another screen: the pointer on the item, drawn again with the palette faded in */
static void back(Menu *m, const AfterRaceOps *o)
{
    o->music_feed(o->ctx);
    move_to(m, o);
    m->redraw = 1;
    m->first = 1;
}

/* the replay or the highlights: the menu freed, the game state and the cars' points kept around it */
static void replay(Menu *m, AfterRaceState *st, const AfterRaceOps *o)
{
    char path[0x50];  /* esp + 0xbb8: never set in the original; emptied here */
    uint8_t game[AFTER_RACE_SAVED];
    int16_t points[AFTER_RACE_CARS];
    o->music_fade_out(o->ctx);
    o->fade_out(o->ctx, 0, ALL);
    o->music_end(o->ctx);
    o->picture_free(o->ctx, *st->screen);
    o->free(o->ctx, m->bank);
    o->free_inverse(o->ctx);
    o->sound_free(o->ctx);
    memcpy(st->replay_name, "REPLAY.RPL", sizeof "REPLAY.RPL");
    if (*st->pointer_active) o->pointer_hide(o->ctx);
    o->clear_one(o->ctx);
    o->window(o->ctx, 0x20, 0xc8, 0xc0, 0x120);
    *st->replay_flag = (int16_t)((m->selected != REPLAY) + 1);
    if (*st->music_silent) {
        st->volumes[2] = st->kept[2];
        st->volumes[1] = st->kept[1];
        st->volumes[0] = st->kept[0];
        *st->music_silent = 0;
    }
    for (int i = 0; i < AFTER_RACE_CARS; i++) memcpy(&points[i], st->cars + AFTER_RACE_CAR * i, 2);
    memcpy(game, st->game_state, sizeof game);
    const int kept = st->game_state[AFTER_RACE_KEPT_AT] == 1 && st->game_state[AFTER_RACE_KEPT_AT + 1] == 0;
    path[0] = 0;
    o->replay(o->ctx, 2, path);
    if (kept) st->game_state[AFTER_RACE_KEPT_AT] = 1, st->game_state[AFTER_RACE_KEPT_AT + 1] = 0;
    memcpy(st->game_state, game, sizeof game);
    for (int i = 0; i < AFTER_RACE_CARS; i++) memcpy(st->cars + AFTER_RACE_CAR * i, &points[i], 2);
    *st->replay_flag = 0;
    o->window(o->ctx, 0, 0, WIDE, TALL);
    o->video_reset(o->ctx);
    o->palette_put(o->ctx, 0, ALL, m->black);
    o->clear_one(o->ctx);
    if (st->mode_switch) {
        o->mode_set(o->ctx, WIDE, TALL, 8);
        o->video_reset(o->ctx);
        o->palette_put(o->ctx, 0, ALL, m->black);
        o->clear_one(o->ctx);
    }
    o->pointer_reset(o->ctx, 0);
    *st->screen = o->picture_new(o->ctx, WIDE, TALL, 0);
    o->sound_load(o->ctx);
    o->music_pick(o->ctx);
    load_art(m, st, o);
    back(m, o);
}

/* the results screens: the segments', then the standings for types 2 and 3; a link game shows the standings
 * alone for those */
static void results(const AfterRaceState *st, const AfterRaceOps *o)
{
    const int16_t type = st->race_type;
    if (type == 3 || type == 2) {
        if (!*st->link) o->segment_results(o->ctx, 0);
        o->standings(o->ctx, 1);
    } else {
        o->segment_results(o->ctx, 0);
    }
}

static void choose(Menu *m, NextSegmentState *ns, const NextSegmentOps *nops)
{
    AfterRaceState *st = &ns->menu;
    const AfterRaceOps *o = &nops->menu;
    switch ((uint16_t)m->selected) {
    case CONTINUE:
        m->result = 1;
        m->done = 1;
        break;
    case REPLAY:
    case HIGHLIGHTS:
        replay(m, st, o);
        break;
    case OPTIONS:
        o->fade_out(o->ctx, 0, ALL);
        o->music_feed(o->ctx);
        o->free_inverse(o->ctx);
        o->free(o->ctx, m->bank);
        o->options(o->ctx, 0);
        load_art(m, st, o);
        palette_set(m, o);
        back(m, o);
        break;
    case RESULTS:
    case CHAT:
        o->fade_out(o->ctx, 0, ALL);
        o->music_feed(o->ctx);
        o->free_inverse(o->ctx);
        if (m->selected == RESULTS) results(st, o);
        else o->chat(o->ctx);
        *st->inverse = o->load_inverse(o->ctx, "check/chkmenu.inv");
        back(m, o);
        break;
    case QUIT:
        if ((int16_t)nops->yes_no(o->ctx, ns->german ? "Spiel beenden ?" : "End game ?")) {
            m->result = 0;
            m->done = 1;
            m->music_on = 0;
        }
        break;
    }
    o->deadline_set(o->ctx, WAIT);
}

/* a key other than the choosing ones: Escape leaves outside a link game, the arrows step over the items not
 * offered */
static void step(Menu *m, const AfterRaceState *st, uint16_t k, const AfterRaceOps *o)
{
    if (k == ESCAPE) {
        if (*st->link == 0) {
            o->sound(o->ctx, CHOSEN);
            m->done = 1;
        }
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        m->previous = m->selected;
        if (k == UP) {
            if (--m->selected < 0) m->selected = QUIT;
            if (*st->link == 0 && m->selected == CHAT) m->selected = RESULTS;
            if (*st->link != 0 && m->selected == HIGHLIGHTS) m->selected = CONTINUE;
        } else {
            m->selected = (int16_t)((m->selected + 1) % ITEMS);
            if (*st->link == 0 && m->selected == CHAT) m->selected = QUIT;
            if (*st->link != 0 && m->selected == REPLAY) m->selected = OPTIONS;
        }
        move_to(m, o);
        m->changed = 1;
    }
    o->ticks(o->ctx);
    m->armed = 0;
    m->rest = 0;
}

static void keys(Menu *m, NextSegmentState *ns, const NextSegmentOps *nops)
{
    AfterRaceState *st = &ns->menu;
    const AfterRaceOps *o = &nops->menu;
    if (*st->pointer_active) *st->buttons = (int16_t)o->pointer_buttons(o->ctx);
    if (*st->buttons == 0) m->released = 1;
    o->pointer_activate_if_moved(o->ctx);
    const uint16_t k = (uint16_t)o->key(o->ctx);
    if (k == SPACE || k == ENTER || (*st->buttons == LEFT_BUTTON && m->released)) {
        o->pointer_click(o->ctx);
        if (m->armed) {
            o->sound(o->ctx, CHOSEN);
            choose(m, ns, nops);
        }
        m->released = 0;
        m->armed = 0;
        m->rest = 0;
        o->ticks(o->ctx);
    } else if (k != 0 && m->armed) {
        step(m, st, k, o);
    } else if (k == 0) {
        m->armed = 1;
    }
}

/* a link game going on: wait for the others; none coming, the game aborted. Then 0xc8 ticks of music */
static void link_end(Menu *m, NextSegmentState *ns, const NextSegmentOps *nops)
{
    const AfterRaceOps *o = &nops->menu;
    if (m->result == 0) {
        *ns->aborted = 1;
        return;
    }
    nops->banner(o->ctx, ns->german ? "Warte auf andere Spieler" : "Waiting for other players");
    nops->deadline(o->ctx, -1);
    if (nops->wait_others(o->ctx, 1)) {
        *ns->over = 0;
    } else {
        if (*ns->aborted) nops->banner(o->ctx, ns->german ? "Verbindung unterbrochen" : "Connection Lost");
        else nops->banner(o->ctx, ns->german ? "Benutzer bricht Spiel ab" : "User Aborting Game");
        m->music_on = 0;
        m->result = 0;
        *ns->aborted = 1;
    }
    o->deadline_set(o->ctx, SETTLE);
    while (!o->deadline_passed(o->ctx)) {
        if (*ns->menu.pointer_active) o->pointer_poll(o->ctx);
        o->music(o->ctx);
    }
}

/* 0040b648 */
int32_t next_segment_menu(NextSegmentState *ns, const NextSegmentOps *nops)
{
    AfterRaceState *st = &ns->menu;
    const AfterRaceOps *ops = &nops->menu;
    static Menu zero;
    Menu m = zero;
    m.redraw = m.first = m.armed = m.released = 1;
    m.music_on = 1;
    m.result = 1;
    move_to(&m, ops);
    load_art(&m, st, ops);
    ops->music_feed(ops->ctx);
    ops->palette_clear(ops->ctx, m.black);
    palette_set(&m, ops);
    if (*st->pointer_active) ops->pointer_poll(ops->ctx);
    ops->pointer_mark(ops->ctx);
    *st->clock = 0;
    do {
        ops->net_service(ops->ctx);
        follow(&m, st, ops);
        blink(&m, st, ops);
        if (m.redraw) draw(&m, st, ops);
        if (m.changed) redraw_items(&m, st, ops);
        if (!m.armed) {
            m.rest += ops->ticks(ops->ctx);
            if ((int16_t)m.rest > REST) m.armed = 1;
        }
        keys(&m, ns, nops);
        if (*st->link) {
            if (*ns->over == 1) m.result = 1, m.done = 1;
            else if (*ns->aborted) m.result = 0, m.done = 1;
        }
        ops->music(ops->ctx);
        if (*st->chat_closed) {
            if (m.selected == CHAT && *st->link == 0) {
                m.previous = m.selected = QUIT;
                move_to(&m, ops);
            }
            m.redraw = 1;
            *st->chat_closed = 0;
        }
    } while (!m.done);
    if (*st->link) link_end(&m, ns, nops);
    ops->free(ops->ctx, m.bank);
    ops->free_inverse(ops->ctx);
    if (m.music_on) ops->music_fade_out(ops->ctx);
    ops->sound_free(ops->ctx);
    ops->fade_out(ops->ctx, 0, ALL);
    if (m.music_on) ops->music_end(ops->ctx);
    else ops->music_feed(ops->ctx);
    ops->picture_free(ops->ctx, *st->screen);
    ops->pointer_mark(ops->ctx);
    return m.result;
}
