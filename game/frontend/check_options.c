#include "game/frontend/check_options.h"

#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { AUDIO, GRAPHICS, CONTROLS, LOAD_SAVE, DONE, CANCEL, ITEMS, STEPS = 5 };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 12000, REST = 0x19, WIDE = 0x280, TALL = 0x1e0, ALL = 0x100 };
enum { KEPT_AT = 9, MUSIC_VOLUME_AT = 0x77 };  /* 00524df9 and 00524e67 in config.dat's image */

/* 00403700 / 0040370c: where the pointer goes per item */
static const int16_t POINTER_X[ITEMS] = { 100, 100, 100, 100, 40, 565 };
static const int16_t POINTER_Y[ITEMS] = { 119, 196, 271, 345, 450, 450 };
/* the bands 00463f80 is asked about, in turn: four rows across the screen, then Done and Cancel side by side */
static const int16_t BAND_LEFT[ITEMS] = { 0, 0, 0, 0, 0, 0x143 };
static const int16_t BAND_RIGHT[ITEMS] = { WIDE, WIDE, WIDE, WIDE, 0x141, WIDE };
static const int16_t BAND_TOP[ITEMS] = { 0, 0x9d, 0xeb, 0x138, 0x1a6, 0x1a6 };
static const int16_t BAND_BOTTOM[ITEMS] = { 0x9b, 0xe9, 0x136, 0x1a4, TALL, 0x1df };
static const char *const LIT[ITEMS] = { "audl", "gral", "conl", "loal", "donl", "canl" };
static const char *const DARK[ITEMS] = { "audd", "grad", "cond", "load", "dond", "cand" };
static const char ART[] = "check/chkopts.qfs", INVERSE[] = "check/chkopts.inv";

typedef struct Menu {
    int32_t bank;           /* edi */
    int16_t selected;       /* esp + 0x3b0 */
    uint32_t previous;      /* esp + 0x398 */
    int changed;            /* esp + 0x3ac */
    int redraw;             /* esp + 0x390 */
    int fade;               /* esp + 0x394: the palette faded in on the next draw */
    int armed;              /* esp + 0x3a0: the keys taken */
    int32_t rest;           /* esp + 0x38c: the ticks summed while a key is held */
    int released;           /* esp + 0x3a4: the buttons seen up */
    int16_t done;           /* esp + 0x3a8 */
    uint8_t kept[CHECK_OPTIONS_KEPT];  /* esp + 0x300: 00524df9.. as on entry */
    uint8_t rgb[CHECK_OPTIONS_PALETTE];  /* esp + 0 */
} Menu;

static void move_to(int16_t item, const CheckOptionsOps *o)
{
    o->pointer_move_to(o->ctx, POINTER_X[item], POINTER_Y[item]);
}

static void blit(const Menu *m, const char *tag, const CheckOptionsOps *o)
{
    o->blit_own(o->ctx, o->shape(o->ctx, m->bank, tag));
}

static void palette(Menu *m, const CheckOptionsOps *o)
{
    const uint8_t *const pal = o->shape(o->ctx, m->bank, "!pal");
    o->palette_set(o->ctx, m->rgb, 0, ALL, pal + 0x10, pal[0]);
}

/* the first band the pointer is in becomes the selection; Load / save's band not asked while the connection is lost */
static void follow(Menu *m, const CheckOptionsState *st, const CheckOptionsOps *o)
{
    for (int16_t i = 0; i < ITEMS; i++) {
        if (i == LOAD_SAVE && *st->lost != 0) continue;
        if (!o->pointer_in_rect(o->ctx, BAND_LEFT[i], BAND_RIGHT[i], BAND_TOP[i], BAND_BOTTOM[i])) continue;
        if (m->selected != i) {
            m->previous = (uint16_t)m->selected;
            m->selected = i;
            m->changed = 1;
        }
        return;
    }
}

static void draw(Menu *m, const CheckOptionsState *st, const CheckOptionsOps *o)
{
    o->select(o->ctx, *st->screen);
    blit(m, "bgnd", o);
    if (*st->language == 0 && *st->lost != 0) blit(m, "loam", o);
    if ((uint16_t)m->selected < ITEMS) blit(m, LIT[m->selected], o);
    o->select_default(o->ctx);
    o->pointer_hide(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, *st->screen));
    o->flip(o->ctx);
    o->pointer_show(o->ctx);
    if (m->fade) {
        o->pointer_mark(o->ctx);
        if (*st->pointer_active) o->pointer_buttons(o->ctx);
        o->fade_in(o->ctx, 0, ALL, m->rgb);
        m->fade = 0;
    }
    o->deadline_set(o->ctx, WAIT);
    m->changed = 0;
    m->redraw = 0;
}

/* the item left goes dark, the new one lit; the screen stays locked */
static void redraw_items(Menu *m, const CheckOptionsState *st, const CheckOptionsOps *o)
{
    o->select(o->ctx, *st->screen);
    o->sound(o->ctx, MOVED);
    o->pointer_hide(o->ctx);
    o->lock(o->ctx);
    m->redraw = 1;
    if ((uint16_t)m->previous < ITEMS) blit(m, DARK[(uint16_t)m->previous], o);
    if ((uint16_t)m->selected < ITEMS) blit(m, LIT[m->selected], o);
    m->changed = 0;
}

/* back from a sub-screen: the pointer on its item unless the pointer is in use, the inverse table loaded again */
static void back(Menu *m, const CheckOptionsState *st, const CheckOptionsOps *o)
{
    if (*st->pointer_active == 0) move_to(m->selected, o);
    *st->inverse = o->load_inverse(o->ctx, INVERSE);
}

/* Cancel and Escape: config.dat's settings as they were, the music at their volume */
static void put_back(Menu *m, const CheckOptionsState *st, const CheckOptionsOps *o)
{
    int16_t volume;
    memcpy(st->config + KEPT_AT, m->kept, sizeof m->kept);
    m->done = 1;
    memcpy(&volume, st->config + MUSIC_VOLUME_AT, sizeof volume);
    o->music_fade_to(o->ctx, volume);
}

static void choose(Menu *m, int32_t mode, const CheckOptionsState *st, const CheckOptionsOps *o)
{
    switch (m->selected) {
    case AUDIO:
    case GRAPHICS:
    case CONTROLS:
        o->fade_out(o->ctx, 0, ALL);
        o->free_inverse(o->ctx);
        if (m->selected == AUDIO) o->audio(o->ctx, 1);
        else if (m->selected == GRAPHICS) o->graphics(o->ctx, 0);
        else o->controls(o->ctx, 1);
        back(m, st, o);
        if (m->selected == CONTROLS) palette(m, o);
        m->redraw = 1;
        m->fade = 1;
        break;
    case LOAD_SAVE:
        if (*st->lost != 0) break;
        o->fade_out(o->ctx, 0, ALL);
        o->free(o->ctx, m->bank);
        o->free_inverse(o->ctx);
        if ((int16_t)mode == 0) o->load_save(o->ctx, 1);
        else o->load_save2(o->ctx, 1);
        if (*st->pointer_active == 0) move_to(m->selected, o);
        m->bank = o->load_art(o->ctx, ART);
        *st->inverse = o->load_inverse(o->ctx, INVERSE);
        palette(m, o);
        o->music_feed(o->ctx);
        m->redraw = 1;
        m->fade = 1;
        break;
    case DONE:
        m->done = 1;
        break;
    case CANCEL:
        put_back(m, st, o);
        break;
    default:
        break;
    }
}

/* a key other than the choosing ones: Escape puts back and leaves, the arrows step */
static void step(Menu *m, const CheckOptionsState *st, uint16_t k, const CheckOptionsOps *o)
{
    if (k == ESCAPE) {
        o->sound(o->ctx, CHOSEN);
        put_back(m, st, o);
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        const int16_t was = m->selected;
        m->previous = (uint16_t)was;
        if (k == UP) {
            m->selected = (int16_t)(was - 1);
            if (*st->lost != 0 && m->selected == LOAD_SAVE) m->selected = (int16_t)(was - 2);
            if (m->selected < 0) m->selected = DONE;
        } else {
            m->selected = (int16_t)((was + 1) % STEPS);
            if (*st->lost != 0 && m->selected == LOAD_SAVE) m->selected = DONE;
        }
        move_to(m->selected, o);
        m->changed = 1;
    } else if ((k == LEFT || k == RIGHT) && (m->selected == DONE || m->selected == CANCEL)) {
        const int16_t to = m->selected == DONE ? CANCEL : DONE;
        o->pointer_off(o->ctx);
        m->previous = (uint16_t)m->selected;
        move_to(to, o);
        m->selected = to;
        m->changed = 1;
    }
    m->armed = 0;
    m->rest = 0;
    o->ticks(o->ctx);
}

/* 00403760 */
int32_t check_options(int32_t mode, const CheckOptionsState *st, const CheckOptionsOps *ops)
{
    static Menu zero;
    Menu m = zero;
    memcpy(m.kept, st->config + KEPT_AT, sizeof m.kept);
    move_to(AUDIO, ops);
    m.bank = ops->load_art(ops->ctx, ART);
    m.fade = 1;
    *st->inverse = ops->load_inverse(ops->ctx, INVERSE);
    m.redraw = m.armed = m.released = 1;
    palette(&m, ops);
    if (*st->pointer_active) ops->pointer_poll(ops->ctx);
    ops->pointer_mark(ops->ctx);
    while (!m.done) {
        follow(&m, st, ops);
        if (m.redraw) draw(&m, st, ops);
        if (m.changed) redraw_items(&m, st, ops);
        if (!m.armed) {
            m.rest += ops->ticks(ops->ctx);
            if ((int16_t)m.rest > REST) m.armed = 1;
        }
        if (*st->pointer_active) *st->buttons = (int16_t)ops->pointer_buttons(ops->ctx);
        if (*st->buttons == 0) m.released = 1;
        ops->pointer_activate_if_moved(ops->ctx);
        const uint16_t k = (uint16_t)ops->key(ops->ctx);
        if (k == SPACE || k == ENTER || (*st->buttons == LEFT_BUTTON && m.released)) {
            ops->pointer_click(ops->ctx);
            if (m.armed) {
                ops->sound(ops->ctx, CHOSEN);
                choose(&m, mode, st, ops);
            }
            ops->ticks(ops->ctx);
            m.released = 0;
            m.armed = 0;
            m.rest = 0;
        } else if (k == 0 || !m.armed) {
            if (k == 0) m.armed = 1;
        } else {
            step(&m, st, k, ops);
        }
        ops->music(ops->ctx);
        if (!m.fade && (ops->deadline_passed(ops->ctx) || (*st->buttons == RIGHT_BUTTON && m.released) || *st->quit == 1)) {
            ops->sound(ops->ctx, CHOSEN);
            ops->wait(ops->ctx, 0);
            m.done = 1;
        }
    }
    ops->fade_out(ops->ctx, 0, ALL);
    ops->music_feed(ops->ctx);
    ops->free(ops->ctx, m.bank);
    ops->free_inverse(ops->ctx);
    ops->deadline_set(ops->ctx, WAIT);
    ops->config_save(ops->ctx);
    ops->pointer_mark(ops->ctx);
    return m.done;
}
