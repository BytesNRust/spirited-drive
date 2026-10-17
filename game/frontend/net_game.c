#include "game/frontend/net_game.h"

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, DOWN = 0x5000, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { SERVICE, SELECT, VIEW, CANCEL, ITEMS };
enum { RACE = 0, LEAVE = 1, TIMED_OUT = 2 };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 12000, REST = 0x19, WIDE = 0x280, ALL = 0x100 };
enum { NAME_X = 0x84, NAME_Y = 0x68, NAME_WIDTH = 0x208, NAME_WIDTH_LIT = 0x1f6 };

/* 00453e30 / 00453e38: the items' spots */
static const int16_t SPOT_X[ITEMS] = { 0x3c, 0x3c, 0x3c, 0x23a };
static const int16_t SPOT_Y[ITEMS] = { 0x76, 0xa5, 0xd2, 0x1c4 };
/* the bands 00463f80 is asked about, in turn */
static const int16_t BAND_LEFT[ITEMS] = { 0, 0, 0, 0x141 };
static const int16_t BAND_TOP[ITEMS] = { 0, 0x8d, 0xbf, 0x15f };
static const int16_t BAND_BOTTOM[ITEMS] = { 0x8c, 0xbe, 0x15e, 0x1e0 };
static const char *const LIT[ITEMS] = { "Serl", "Sell", "Viel", "Canl" };   /* 004b0ed0.. */
static const char *const DARK[ITEMS] = { "Serd", "Seld", "Vied", "Cand" };  /* 004b0f00.. */

typedef struct Menu {
    const char *service;  /* esi: 00442514's / 004425a0's entry */
    int16_t selected;     /* esp + 0x62c */
    int16_t previous;     /* esp + 0x628 */
    int changed;          /* esp + 0x624 */
    int redraw;           /* esp + 0x61c */
    int fade;             /* esp + 0x614: the palette faded in on the next draw */
    int armed;            /* esp + 0x620: the keys taken */
    int32_t rest;         /* esp + 0x618: the ticks summed while a key is held */
    int released;         /* esp + 0x610: the buttons seen up */
    int done;             /* esp + 0x630 */
    int16_t result;       /* esp + 0x634 */
    uint8_t rgb[NET_GAME_PALETTE];    /* esp: "!pal" */
    uint8_t black[NET_GAME_PALETTE];  /* esp + 0x300: 00463df0's */
} Menu;

static void blit(const NetGameState *st, const NetGameOps *o, const char *tag)
{
    o->blit_own(o->ctx, o->shape(o->ctx, st->bank, tag));
}

static void move_to(const Menu *m, const NetGameOps *o)
{
    o->pointer_move_to(o->ctx, SPOT_X[m->selected], SPOT_Y[m->selected]);
}

/* the first band the pointer is in becomes the selection */
static void follow(Menu *m, const NetGameOps *o)
{
    for (int16_t i = 0; i < ITEMS; i++) {
        if (!o->pointer_in_rect(o->ctx, BAND_LEFT[i], WIDE, BAND_TOP[i], BAND_BOTTOM[i])) continue;
        if (m->selected != i) {
            m->previous = m->selected;
            m->selected = i;
            m->changed = 1;
        }
        return;
    }
}

/* an item lit or dark (the service's name with it, at width) */
static void item(const Menu *m, const NetGameState *st, const NetGameOps *o, uint16_t which, int lit, int32_t width)
{
    if (which >= ITEMS) return;
    blit(st, o, lit ? LIT[which] : DARK[which]);
    if (which == SERVICE) o->text_fit(o->ctx, m->service, NAME_X, NAME_Y, width);
    if (which == VIEW) blit(st, o, st->view != 0 ? (lit ? "Yesl" : "Yesd") : (lit ? "Nol " : "Nod "));
}

static void draw(Menu *m, NetGameState *st, const NetGameOps *o)
{
    o->text_select(o->ctx, 0);
    o->select(o->ctx, st->screen);
    blit(st, o, "bgnd");
    if (m->selected != SERVICE) o->text_fit(o->ctx, m->service, NAME_X, NAME_Y, NAME_WIDTH);
    if (m->selected != VIEW) blit(st, o, st->view != 0 ? "Yesd" : "Nod ");
    o->text_select(o->ctx, 1);
    item(m, st, o, (uint16_t)m->selected, 1, NAME_WIDTH);
    if (m->fade) o->palette_show(o->ctx, 0, ALL, m->black);
    o->select_default(o->ctx);
    o->pointer_hide(o->ctx);
    o->lock(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, st->screen));
    o->unlock(o->ctx);
    o->flip(o->ctx);
    o->pointer_show(o->ctx);
    if (m->fade) {
        o->pointer_mark(o->ctx);
        if (st->pointer_active) o->pointer_poll(o->ctx);
        o->fade_in(o->ctx, 0, ALL, m->rgb);
        o->inverse_build(o->ctx);
        o->text_styles(o->ctx);
        m->fade = 0;
    }
    o->deadline_set(o->ctx, WAIT);
    m->redraw = 0;
}

static void redraw_items(Menu *m, NetGameState *st, const NetGameOps *o)
{
    m->redraw = 1;
    o->select(o->ctx, st->screen);
    o->sound(o->ctx, MOVED);
    o->pointer_hide(o->ctx);
    o->lock(o->ctx);
    o->text_select(o->ctx, 0);
    item(m, st, o, (uint16_t)m->previous, 0, NAME_WIDTH_LIT);
    o->text_select(o->ctx, 1);
    item(m, st, o, (uint16_t)m->selected, 1, NAME_WIDTH_LIT);
    m->changed = 0;
}

static void choose(Menu *m, NetGameState *st, const NetGameOps *o)
{
    switch ((uint16_t)m->selected) {
    case SERVICE:
        m->service = o->service_next(o->ctx);
        o->text_select(o->ctx, 0);
        o->text_fit(o->ctx, m->service, NAME_X, NAME_Y, NAME_WIDTH_LIT);
        o->select_default(o->ctx);
        o->lock(o->ctx);
        o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, st->screen));
        o->unlock(o->ctx);
        o->flip(o->ctx);
        o->pointer_show(o->ctx);
        if (!st->pointer_active) move_to(m, o);
        m->redraw = 1;
        break;
    case SELECT:
        if (o->host_join(o->ctx) != 0) m->done = 1, m->result = RACE;
        move_to(m, o);
        m->redraw = 1;
        break;
    case VIEW:
        st->view = st->view == 0;
        m->redraw = 1;
        break;
    case CANCEL:
        m->done = 1;
        m->result = LEAVE;
        break;
    }
    o->deadline_set(o->ctx, WAIT);
}

/* a key other than the choosing ones: Escape leaves, the arrows step */
static void step(Menu *m, uint16_t k, const NetGameOps *o)
{
    if (k == ESCAPE) {
        o->sound(o->ctx, CHOSEN);
        m->done = 1;
        m->result = LEAVE;
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        m->previous = m->selected;
        m->selected = (int16_t)(m->selected + (k == UP ? -1 : 1));
        if (m->selected < 0) m->selected = CANCEL;
        if (m->selected > CANCEL) m->selected = SERVICE;
        m->changed = 1;
        move_to(m, o);
    }
    o->deadline_set(o->ctx, WAIT);
    m->armed = 0;
    m->rest = 0;
    o->ticks(o->ctx);
}

/* 00454b78 */
int32_t net_game_menu(NetGameState *st, const NetGameOps *ops)
{
    Menu m = { 0 };
    move_to(&m, ops);
    st->view = 0;
    ops->flush(ops->ctx);
    ops->palette_clear(ops->ctx, m.black);
    move_to(&m, ops);
    m.fade = 1;
    st->bank = ops->load_art(ops->ctx, "option/netset.qfs");
    ops->music_feed(ops->ctx);
    m.redraw = m.armed = m.released = 1;
    const uint8_t *pal = ops->shape(ops->ctx, st->bank, "!pal");
    ops->palette_set(ops->ctx, m.rgb, 0, ALL, pal + 0x10, (int16_t)pal[0]);
    ops->palette_put(ops->ctx, 0, ALL, m.rgb);
    ops->inverse_build(ops->ctx);
    ops->text_styles(ops->ctx);
    if (st->pointer_active) ops->pointer_poll(ops->ctx);
    m.service = ops->service_first(ops->ctx);
    ops->pointer_mark(ops->ctx);
    while (!m.done) {
        follow(&m, ops);
        if (m.redraw) draw(&m, st, ops);
        if (m.changed) redraw_items(&m, st, ops);
        if (!m.armed) {
            m.rest += ops->ticks(ops->ctx);
            if ((int16_t)m.rest > REST) m.armed = 1;
        }
        if (st->pointer_active) st->buttons = (int16_t)ops->pointer_buttons(ops->ctx);
        if (st->buttons == 0) m.released = 1;
        ops->pointer_activate_if_moved(ops->ctx);
        uint16_t k = (uint16_t)ops->key(ops->ctx);
        if (st->buttons == RIGHT_BUTTON && m.released) m.selected = CANCEL, m.armed = 1, k = ENTER;
        if (k == SPACE || k == ENTER || (st->buttons == LEFT_BUTTON && m.released)) {
            ops->pointer_click(ops->ctx);
            if (m.armed) {
                ops->sound(ops->ctx, CHOSEN);
                choose(&m, st, ops);
            }
            m.released = 0;
            m.armed = 0;
            m.rest = 0;
            ops->ticks(ops->ctx);
        } else if (k != 0 && m.armed) {
            step(&m, k, ops);
        } else if (k == 0) {
            m.armed = 1;
        }
        const int fading = m.fade;
        ops->music(ops->ctx);
        if (!fading && ops->deadline_passed(ops->ctx)) {
            ops->sound(ops->ctx, CHOSEN);
            m.done = 1;
            m.result = TIMED_OUT;
        }
    }
    ops->fade_out(ops->ctx, 0, ALL);
    ops->music_feed(ops->ctx);
    ops->free(ops->ctx, st->bank);
    ops->free_inverse(ops->ctx);
    ops->pointer_mark(ops->ctx);
    if (m.result != 0) st->view = 0;
    return m.result;
}
