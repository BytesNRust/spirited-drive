#include "game/frontend/game_mode.h"

#include <stdio.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, DOWN = 0x5000, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { SINGLE, MODEM, NETWORK, EXIT, OPTIONS };
enum { RACE = 0, LEAVE = 1, TIMED_OUT = 2 };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x1194, REST = 0x19, TOLD = 0x12c, WIDE = 0x280, ALL = 0x100 };

/* 00432134 / 0043213c: the options' centres */
static const int16_t CENTRE_X[OPTIONS] = { 0x140, 0x140, 0x140, 0x140 };
static const int16_t CENTRE_Y[OPTIONS] = { 0x91, 0xda, 0x122, 0x16f };
/* the bands 00463f80 is asked about, in turn */
static const int16_t BAND_TOP[OPTIONS] = { 0, 0xb2, 0xfe, 0x14a };
static const int16_t BAND_BOTTOM[OPTIONS] = { 0xb1, 0xfd, 0x149, 0x1e0 };
static const char *const LIT[OPTIONS] = { "sinl", "modl", "netl", "exil" };   /* 004afaf8.. */
static const char *const DARK[OPTIONS] = { "sind", "modd", "netd", "exid" };  /* 004afb18.. */

typedef struct Menu {
    int32_t bank;
    int16_t selected;  /* esp + 0x384 */
    int16_t previous;  /* esp + 0x374 */
    int changed;       /* esp + 0x378 */
    int redraw;        /* esp + 0x370 */
    int fade;          /* esp + 0x368: the palette faded in on the next draw */
    int armed;         /* esp + 0x36c: the keys taken */
    int32_t rest;      /* esp + 0x364: the ticks summed while a key is held */
    int released;      /* esp + 0x360: the buttons seen up */
    int done;          /* esp + 0x37c */
    int16_t result;    /* esp + 0x380 */
    uint8_t rgb[GAME_MODE_PALETTE];
} Menu;

static void palette_load(Menu *m, const GameModeOps *o)
{
    const uint8_t *pal = o->shape(o->ctx, m->bank, "!pal");
    o->palette_set(o->ctx, m->rgb, 0, ALL, pal + 0x10, (int16_t)pal[0]);
}

static void move_to(const Menu *m, const GameModeOps *o)
{
    o->pointer_move_to(o->ctx, CENTRE_X[m->selected], CENTRE_Y[m->selected]);
}

/* the first band the pointer is in becomes the selection */
static void follow(Menu *m, const GameModeOps *o)
{
    for (int16_t i = 0; i < OPTIONS; i++) {
        if (!o->pointer_in_rect(o->ctx, 0, WIDE, BAND_TOP[i], BAND_BOTTOM[i])) continue;
        if (m->selected != i) {
            m->previous = m->selected;
            m->selected = i;
            m->changed = 1;
        }
        return;
    }
}

/* port: with the extras op, the Modem item's words covered with the clean background and "Customize game" written
 * there, lit while it is the selection. The cover is the item's art (modd / modl: 253 x 32 at (189, 207)). */
static void extras_label(const Menu *m, const GameModeOps *o)
{
    enum { TEXT_HALF_HEIGHT = 8, MODEM_X = 189, MODEM_Y = 207, MODEM_W = 253, MODEM_H = 32 };
    if (o->extras == NULL || o->plus_text == NULL || o->plus_text_width == NULL) return;
    if (o->extras_cover != NULL) o->extras_cover(o->ctx, MODEM_X, MODEM_Y, MODEM_W, MODEM_H);
    const char *const text = "Customize game";
    o->plus_text(o->ctx, m->selected == MODEM, text, CENTRE_X[MODEM] - o->plus_text_width(o->ctx, text) / 2,
                 CENTRE_Y[MODEM] - TEXT_HALF_HEIGHT);
}

static void draw(Menu *m, GameModeState *st, int german, const GameModeOps *o)
{
    o->select(o->ctx, st->screen);
    o->blit_own(o->ctx, o->shape(o->ctx, m->bank, "bgnd"));
    if ((uint16_t)m->selected < OPTIONS) o->blit_own(o->ctx, o->shape(o->ctx, m->bank, LIT[m->selected]));
    extras_label(m, o);
    o->select_default(o->ctx);
    o->pointer_hide(o->ctx);
    o->lock(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, st->screen));
    o->unlock(o->ctx);
    o->flip(o->ctx);
    o->pointer_show(o->ctx);
    if (m->fade) {
        o->music_feed(o->ctx);
        if (st->pointer_active) o->pointer_buttons(o->ctx);
        o->fade_in(o->ctx, 0, ALL, m->rgb);
        o->inverse_build(o->ctx);
        m->fade = 0;
        if (st->lost && st->lost_shown) {
            st->lost_shown = 0, st->lost = 0;
            o->banner_wait(o->ctx, german ? "Verbindung abgebrochen" : "Connection Terminated", TOLD);
        }
    }
    o->music_feed(o->ctx);
    o->deadline_set(o->ctx, WAIT);
    m->changed = 0;
    m->redraw = 0;
}

static void redraw_options(Menu *m, GameModeState *st, const GameModeOps *o)
{
    o->select(o->ctx, st->screen);
    m->redraw = 1;
    if (m->previous != m->selected) o->sound(o->ctx, MOVED);
    if ((uint16_t)m->previous < OPTIONS) o->blit_own(o->ctx, o->shape(o->ctx, m->bank, DARK[m->previous]));
    if ((uint16_t)m->selected < OPTIONS) o->blit_own(o->ctx, o->shape(o->ctx, m->bank, LIT[m->selected]));
    if (m->previous == MODEM || m->selected == MODEM) extras_label(m, o);
    m->changed = 0;
}

/* "Exit to system ?" (in German "Zurück zum system ?", ü as 0x81) */
static void ask_exit(Menu *m, int german, const GameModeOps *o)
{
    char text[0x50];
    const char *ask = "Exit to system ?";
    if (german) snprintf(text, sizeof text, "Zur%cck zum system ?", 0x81), ask = text;
    if ((int16_t)o->yes_no(o->ctx, ask)) m->done = 1, m->result = LEAVE;
    o->drain(o->ctx);
}

/* back from the modem's or the network's game: 1 the menu again, else to the race */
static void came_back(Menu *m, GameModeState *st, int16_t r, const GameModeOps *o)
{
    m->result = r;
    if (r == 1) {
        palette_load(m, o);
        m->redraw = 1;
        m->fade = 1;
        move_to(m, o);
    } else {
        m->done = 1;
        m->result = RACE;
        st->network_raced = 1;
        o->inverse_build(o->ctx);
    }
}

static void choose(Menu *m, GameModeState *st, const GameModeOps *o)
{
    switch ((uint16_t)m->selected) {
    case SINGLE:
        if (!(int16_t)o->name_box(o->ctx, st->name)) break;
        m->result = RACE;
        st->single = 0;
        st->network_raced = 0;
        m->done = 1;
        break;
    case MODEM:
        if (o->extras != NULL) {   /* port: the EXTRAS screen over this one, then this menu again */
            o->extras(o->ctx);
            palette_load(m, o);
            m->redraw = 1;
            m->fade = 1;
            move_to(m, o);
            break;
        }
        if (!(int16_t)o->name_box(o->ctx, st->name)) break;
        o->fade_out(o->ctx, 0, ALL);
        o->free_inverse(o->ctx);
        came_back(m, st, (int16_t)o->modem(o->ctx), o);
        break;
    case NETWORK:
        if (!o->network_found(o->ctx)) {
            m->redraw = 1;
            o->deadline_set(o->ctx, WAIT);
            break;
        }
        if (!(int16_t)o->name_box(o->ctx, st->name)) break;
        o->fade_out(o->ctx, 0, ALL);
        o->free_inverse(o->ctx);
        came_back(m, st, (int16_t)o->network_game(o->ctx), o);
        break;
    case EXIT:
        m->done = 1;
        m->result = LEAVE;
        break;
    }
}

/* a key other than the choosing ones: Escape asks, the arrows step */
static void step(Menu *m, int16_t k, int german, const GameModeOps *o)
{
    if (k == ESCAPE) {
        o->sound(o->ctx, CHOSEN);
        ask_exit(m, german, o);
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        m->previous = m->selected;
        m->selected = k == UP ? (int16_t)(m->selected - 1) : (int16_t)((m->selected + 1) % OPTIONS);
        if (k == UP && m->selected < 0) m->selected = OPTIONS - 1;
        move_to(m, o);
        m->changed = 1;
    }
    o->deadline_set(o->ctx, WAIT);
    o->ticks(o->ctx);
    m->armed = 0;
    m->rest = 0;
}

/* 004376ac */
int32_t game_mode_menu(int german, GameModeState *st, const GameModeOps *ops)
{
    Menu m = { 0 };
    m.redraw = m.fade = m.armed = m.released = 1;
    st->mode_menu = 1;
    st->intro_seen = 1;
    ops->palette_clear(ops->ctx, m.rgb);
    move_to(&m, ops);
    ops->sound_load(ops->ctx);
    m.bank = ops->load_art(ops->ctx, "control/gamemode.qfs");
    ops->music_feed(ops->ctx);
    palette_load(&m, ops);
    ops->pointer_mark(ops->ctx);
    do {
        follow(&m, ops);
        if (m.redraw) draw(&m, st, german, ops);
        if (m.changed) redraw_options(&m, st, ops);
        int16_t k = (int16_t)ops->key(ops->ctx);
        if (!m.armed && k != ESCAPE && k != SPACE && k != ENTER) {
            m.rest += ops->ticks(ops->ctx);
            if ((int16_t)m.rest > REST) m.armed = 1;
        }
        if (st->pointer_active) st->buttons = (int16_t)ops->pointer_buttons(ops->ctx);
        if (st->buttons == 0) m.released = 1;
        ops->pointer_activate_if_moved(ops->ctx);
        k = (int16_t)ops->key(ops->ctx);
        if (k == SPACE || k == ENTER || (st->buttons == LEFT_BUTTON && m.released)) {
            ops->pointer_click(ops->ctx);
            if (m.armed) {
                ops->sound(ops->ctx, CHOSEN);
                choose(&m, st, ops);
            }
            ops->ticks(ops->ctx);
            m.released = 0;
            m.armed = 0;
            m.rest = 0;
        } else if (k != 0 && m.armed) {
            step(&m, k, german, ops);
        } else if (k == 0) {
            m.armed = 1;
        }
        ops->music(ops->ctx);
        if (st->redraw) st->redraw = 0, m.redraw = 1;
        if (m.fade || m.done) continue;
        if (ops->deadline_passed(ops->ctx)) {
            ops->sound(ops->ctx, CHOSEN);
            m.done = 1;
            m.result = TIMED_OUT;
        } else if (st->buttons == RIGHT_BUTTON && m.released) {
            ask_exit(&m, german, ops);
            for (;;) {
                if (st->pointer_active) st->buttons = (int16_t)ops->pointer_buttons(ops->ctx);
                if (!ops->key(ops->ctx) && st->buttons == 0) break;
                ops->music(ops->ctx);
            }
        }
    } while (!m.done);
    ops->sound_free(ops->ctx);
    ops->fade_out(ops->ctx, 0, ALL);
    ops->free_inverse(ops->ctx);
    ops->free(ops->ctx, m.bank);
    return m.result;
}
