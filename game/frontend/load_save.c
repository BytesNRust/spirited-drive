#include "game/frontend/load_save.h"

#include <stdio.h>
#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, DOWN = 0x5000, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { SAVE, LOAD, DELETE_GAME, SAVE_REPLAY, VIEW_REPLAY, VIEW_HIGHLIGHT, DELETE_REPLAY, DONE, ITEMS };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x2ee0, REST = 0x19, WIDE = 0x280, TALL = 0x1e0, ALL = 0x100 };
enum { LIST_REPLAY = 0, LIST_DELETE_REPLAY = 1, LIST_LOAD_GAME = 2, LIST_DELETE_GAME = 3 };

/* 004291f8.. (German) and 00429218.. (English): where the pointer goes per item */
static const int16_t POINTER_X[ITEMS] = { 155, 155, 155, 155, 155, 155, 155, 40 };
static const int16_t POINTER_Y[2][ITEMS] = { { 120, 160, 200, 240, 280, 320, 360, 450 },
                                             { 120, 160, 200, 251, 302, 340, 390, 450 } };
/* the bands 00463f80 is asked about, in turn (English, German) */
static const int16_t BAND_TOP[2][ITEMS] = { { 0, 0x8c, 0xb4, 0xdc, 0x104, 0x12c, 0x154, 0x19a },
                                            { 0, 0x8c, 0xb4, 0xdc, 0x11d, 0x145, 0x16d, 0x1ae } };
static const int16_t BAND_BOTTOM[2][ITEMS] = { { 0x87, 0xaf, 0xd7, 0xff, 0x127, 0x14f, 0x190, 0x1e0 },
                                               { 0x87, 0xaf, 0xd7, 0x118, 0x140, 0x168, 0x1a4, 0x1e0 } };
static const char *const LIT[ITEMS] = { "savl", "loal", "dlgl", "sarl", "vwrl", "vwhl", "dlrl", "donl" };
static const char *const DARK[ITEMS] = { "savd", "load", "dlgd", "sard", "vwrd", "vwhd", "dlrd", "dond" };

typedef struct Menu {
    int16_t mode;      /* esp + 0x728 */
    int32_t bank;      /* esi */
    int16_t selected;  /* esp + 0x720 */
    int16_t previous;  /* esp + 0x714 */
    int changed;       /* esp + 0x710 */
    int redraw;        /* esp + 0x72c */
    int fade;          /* esp + 0x718: the palette faded in on the next draw */
    int armed;         /* esp + 0x70c: the keys taken */
    int32_t rest;      /* esp + 0x724: the ticks summed while a key is held */
    int released;      /* esp + 0x71c: the buttons seen up */
    int done;          /* esp + 0x708 */
    uint8_t black[LOAD_SAVE_PALETTE];  /* esp + 0: cleared, put after the replay */
    uint8_t rgb[LOAD_SAVE_PALETTE];    /* esp + 0x300: the art's, faded in */
} Menu;

static void move_to(const Menu *m, const LoadSaveState *st, const LoadSaveOps *o)
{
    o->pointer_move_to(o->ctx, POINTER_X[m->selected], POINTER_Y[st->german != 0][m->selected]);
}

static void load_art(Menu *m, const LoadSaveOps *o, const LoadSaveState *st)
{
    m->bank = o->load_art(o->ctx, m->mode == 1 ? "check/loadsave.qfs" : "option/loadsave.qfs");
    *st->inverse = o->load_inverse(o->ctx, m->mode == 1 ? "check/loadsave.inv" : "option/loadsave.inv");
}

static void blit(const Menu *m, const char *tag, const LoadSaveOps *o)
{
    o->blit_own(o->ctx, o->shape(o->ctx, m->bank, tag));
}

/* an item the pointer may take: saving from 0 in a tournament, loading outside one, viewing unless the
 * connection was lost */
static int offered(const Menu *m, const LoadSaveState *st, int16_t i)
{
    if (i == SAVE) return st->tournament != 0 && m->mode == 0;
    if (i == LOAD) return st->tournament == 0;
    if (i == VIEW_REPLAY || i == VIEW_HIGHLIGHT) return st->lost == 0;
    return 1;
}

/* the first band the pointer is in, of an item offered, becomes the selection */
static void follow(Menu *m, const LoadSaveState *st, const LoadSaveOps *o)
{
    const int g = st->german != 0;
    for (int16_t i = 0; i < ITEMS; i++) {
        if (!o->pointer_in_rect(o->ctx, 0, WIDE, BAND_TOP[g][i], BAND_BOTTOM[g][i]) || !offered(m, st, i)) continue;
        if ((i == SAVE || i == LOAD) && st->lost) return;
        if (m->selected != i) {
            m->previous = m->selected;
            m->selected = i;
            m->changed = 1;
        }
        return;
    }
}

static void draw(Menu *m, const LoadSaveState *st, const LoadSaveOps *o)
{
    o->select(o->ctx, *st->screen);
    blit(m, "bgnd", o);
    if ((uint16_t)m->selected < ITEMS) blit(m, LIT[m->selected], o);
    if (m->mode == 0 && st->tournament == 0) {
        blit(m, "savm", o);
        if (st->lost) blit(m, "loam", o);
    } else {
        if (m->mode == 1) blit(m, "savm", o);
        blit(m, "loam", o);
    }
    if (st->lost) blit(m, "vwrm", o), blit(m, "vwhm", o);
    o->select_default(o->ctx);
    o->pointer_hide(o->ctx);
    o->lock(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, *st->screen));
    o->unlock(o->ctx);
    o->flip(o->ctx);
    o->pointer_show(o->ctx);
    if (m->fade) {
        if (st->pointer_active) o->pointer_buttons(o->ctx);
        o->fade_in(o->ctx, 0, ALL, m->rgb);
        o->text_styles(o->ctx);
        m->fade = 0;
    }
    o->deadline_set(o->ctx, WAIT);
    m->redraw = 0;
}

/* the item left dark (Load only outside a tournament), the new one lit; the screen stays locked */
static void redraw_items(Menu *m, const LoadSaveState *st, const LoadSaveOps *o)
{
    o->sound(o->ctx, MOVED);
    m->redraw = 1;
    o->select(o->ctx, *st->screen);
    o->pointer_hide(o->ctx);
    o->lock(o->ctx);
    if ((uint16_t)m->previous < ITEMS && (m->previous != LOAD || st->tournament == 0)) blit(m, DARK[m->previous], o);
    if ((uint16_t)m->selected < ITEMS) blit(m, LIT[m->selected], o);
    m->changed = 0;
}

/* a replay or a highlight picked in 0042ae9c plays, the menu's art freed around it */
static void view(Menu *m, LoadSaveState *st, const LoadSaveOps *o)
{
    char path[LOAD_SAVE_PATH];  /* esp + 0x650: left as it was in the original; emptied here */
    uint8_t race[LOAD_SAVE_RACE];
    o->free_inverse(o->ctx);
    o->free(o->ctx, m->bank);
    o->music_fade_out(o->ctx);
    o->fade_out(o->ctx, 0, ALL);
    o->music_end(o->ctx);
    o->picture_free(o->ctx, *st->screen);
    o->sound_free(o->ctx);
    o->clear_one(o->ctx);
    o->window(o->ctx, 0x20, 0xc8, 0xc0, 0x120);
    *st->replay_flag = (int16_t)((m->selected != VIEW_REPLAY) + 1);
    memcpy(race, st->race, sizeof race);
    if (*st->music_silent) {
        st->volumes[2] = st->kept[2];
        st->volumes[1] = st->kept[1];
        *st->music_silent = 0;
        st->volumes[0] = st->kept[0];
    }
    path[0] = 0;
    o->replay(o->ctx, 2, path);
    memcpy(st->race, race, sizeof race);
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
    *st->screen = o->picture_new(o->ctx, WIDE, TALL, 0);
    o->sound_load(o->ctx);
    o->music_pick(o->ctx);
    load_art(m, o, st);
    m->fade = 1;
}

static void choose(Menu *m, LoadSaveState *st, const LoadSaveOps *o)
{
    char path[LOAD_SAVE_PATH];
    switch ((uint16_t)m->selected) {
    case SAVE:
        snprintf(path, sizeof path, "%s", st->name);  /* sprintf(path, name): the case's names hold no '%' */
        o->save_box(o->ctx, path);
        break;
    case LOAD:
        o->game_list(o->ctx, LIST_LOAD_GAME);
        *st->variant = 0;
        m->selected = DONE;
        break;
    case DELETE_GAME:
        o->game_list(o->ctx, LIST_DELETE_GAME);
        break;
    case SAVE_REPLAY:
        snprintf(path, sizeof path, "%s", st->replay_name);
        o->replay_box(o->ctx, path);
        break;
    case VIEW_REPLAY:
    case VIEW_HIGHLIGHT:
        *st->highlight = m->selected != VIEW_REPLAY;
        if ((int16_t)o->game_list(o->ctx, LIST_REPLAY)) view(m, st, o);
        break;
    case DELETE_REPLAY:
        o->game_list(o->ctx, LIST_DELETE_REPLAY);
        break;
    case DONE:
        m->done = 1;
        return;
    default:
        return;
    }
    move_to(m, st, o);
    o->deadline_set(o->ctx, WAIT);
    m->redraw = 1;
}

/* Up: back one, over Load and the viewing ones when the connection is lost, and over Save or Load not offered */
static void step_up(Menu *m, const LoadSaveState *st)
{
    m->selected--;
    if (st->lost) {
        if (m->selected == LOAD) m->selected = DONE;
        if (m->selected == VIEW_HIGHLIGHT) m->selected = SAVE_REPLAY;
    }
    if (m->mode == 0 && st->tournament == 0) {
        if (m->selected == SAVE) m->selected = DONE;
    } else if (m->mode == 0) {
        if (m->selected == LOAD) m->selected = SAVE;
    } else if (m->selected == LOAD) {
        m->selected = DONE;
    }
    if (m->selected < 0) m->selected = DONE;
}

static void step_down(Menu *m, const LoadSaveState *st)
{
    m->selected = (int16_t)((m->selected + 1) % ITEMS);
    if (st->lost) {
        if (m->selected == SAVE) m->selected = DELETE_GAME;
        if (m->selected == VIEW_REPLAY) m->selected = DELETE_REPLAY;
    }
    if (st->tournament == 0 && m->selected == LOAD) m->selected = DELETE_GAME;
    if (m->mode == 0 && st->tournament == 0) {
        if (m->selected == SAVE) m->selected = LOAD;
    } else if (m->mode == 0) {
        if (m->selected == LOAD) m->selected = DELETE_GAME;
    } else if (m->selected == SAVE) {
        m->selected = DELETE_GAME;
    }
}

/* a key other than the choosing ones: Escape leaves, the arrows step */
static void step(Menu *m, const LoadSaveState *st, uint16_t k, const LoadSaveOps *o)
{
    if (k == ESCAPE) {
        o->sound(o->ctx, CHOSEN);
        m->done = 1;
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        m->previous = m->selected;
        if (k == UP) step_up(m, st);
        else step_down(m, st);
        move_to(m, st, o);
        m->changed = 1;
    }
    o->ticks(o->ctx);
    m->armed = 0;
    m->rest = 0;
}

/* 0042c498 */
int32_t load_save_menu(int32_t mode, LoadSaveState *st, const LoadSaveOps *ops)
{
    static Menu zero;
    Menu m = zero;
    m.mode = (int16_t)mode;
    m.fade = m.redraw = m.armed = m.released = 1;
    m.previous = m.selected = LOAD;
    if (st->lost || m.mode == 1) {
        m.previous = m.selected = SAVE_REPLAY;
    } else if (st->tournament && st->race_type == 3) {
        m.previous = m.selected = SAVE;
    }
    move_to(&m, st, ops);
    load_art(&m, ops, st);
    *st->check = m.mode == 1;
    ops->music_feed(ops->ctx);
    ops->palette_clear(ops->ctx, m.black);
    const uint8_t *pal = ops->shape(ops->ctx, m.bank, "!pal");
    ops->palette_set(ops->ctx, m.rgb, 0, ALL, pal + 0x10, (int16_t)pal[0]);
    if (st->pointer_active) ops->pointer_poll(ops->ctx);
    ops->pointer_mark(ops->ctx);
    do {
        follow(&m, st, ops);
        if (m.redraw) draw(&m, st, ops);
        if (m.changed) redraw_items(&m, st, ops);
        if (!m.armed) {
            m.rest += ops->ticks(ops->ctx);
            if ((int16_t)m.rest > REST) m.armed = 1;
        }
        if (st->pointer_active) *st->buttons = (int16_t)ops->pointer_buttons(ops->ctx);
        if (*st->buttons == 0) m.released = 1;
        ops->pointer_activate_if_moved(ops->ctx);
        const uint16_t k = (uint16_t)ops->key(ops->ctx);
        if (k == SPACE || k == ENTER || (*st->buttons == LEFT_BUTTON && m.released)) {
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
            step(&m, st, k, ops);
        } else if (k == 0) {
            m.armed = 1;
        }
        ops->music(ops->ctx);
        if (m.fade) continue;
        if (ops->deadline_passed(ops->ctx) || (*st->buttons == RIGHT_BUTTON && m.released)) {
            ops->sound(ops->ctx, CHOSEN);
            m.done = 1;
        }
    } while (!m.done);
    ops->fade_out(ops->ctx, 0, ALL);
    ops->music_feed(ops->ctx);
    ops->free_inverse(ops->ctx);
    ops->free(ops->ctx, m.bank);
    ops->deadline_set(ops->ctx, WAIT);
    ops->pointer_mark(ops->ctx);
    return 0;
}
