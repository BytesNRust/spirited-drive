#include "game/frontend/graphics_menu.h"
#include "game/frontend/graphics_plus.h"

#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, F1 = 0x3b00, F2 = 0x3c00, F3 = 0x3d00, F4 = 0x3e00, UP = 0x4800,
       LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000, PAGE_DOWN = 0x5100, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { RESOLUTION, WINDOW, IMAGE, VIEW, CARS, HORIZON, DONE, CANCEL, ITEMS, SETTINGS = DONE };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x2ee0, REST = 0x19, WIDE = 0x280, ALL = 0x100, LOW = 1 };

/* 00423c10 / 00423c20: where the pointer goes per item */
static const int16_t POINTER_X[ITEMS] = { 230, 230, 230, 230, 230, 230, 60, 565 };
static const int16_t POINTER_Y[ITEMS] = { 116, 169, 220, 270, 325, 377, 445, 445 };
/* the bands 00463f80 is asked about, in turn (the resolution's only with mode 0) */
static const int16_t BAND_LEFT[ITEMS] = { 0, 0, 0, 0, 0, 0, 0, 0x145 };
static const int16_t BAND_RIGHT[ITEMS] = { WIDE, WIDE, WIDE, WIDE, WIDE, WIDE, 0x140, WIDE };
static const int16_t BAND_TOP[ITEMS] = { 0, 0x91, 0xc6, 0xf7, 0x12c, 0x161, 0x1a4, 0x1a4 };
static const int16_t BAND_BOTTOM[ITEMS] = { 0x8c, 0xc1, 0xf2, 0x127, 0x15c, 0x190, 0x1e0, 0x1df };
static const char *const LIT[ITEMS] = { "scrl", "winl", "imal", "vanl", "carl", "horl", "donl", "canl" };
static const char *const DARK[ITEMS] = { "scrd", "wind", "imad", "vand", "card", "hord", "dond", "cand" };
/* 00423c30..00423cc0, copied to the stack: each setting's values lit and dark, and the resolution's muted */
static const int SETTING[SETTINGS] = { GRAPHICS_RESOLUTION, GRAPHICS_WINDOW, GRAPHICS_IMAGE, GRAPHICS_VIEW,
                                       GRAPHICS_CARS, GRAPHICS_HORIZON };
static const char *const VALUE_LIT[SETTINGS][6] = {
    { "640l", "320l" }, { "full", "wmdl", "smal" }, { "ihil", "imdl", "ilwl", "intl", "atml", "atll" },
    { "farl", "vmdl", "clsl" }, { "chil", "clwl" }, { "honl", "offl" } };
static const char *const VALUE_DARK[SETTINGS][6] = {
    { "640d", "320d" }, { "fuld", "wmdd", "smad" }, { "ihid", "imdd", "ilwd", "intd", "atmd", "atld" },
    { "fard", "vmdd", "clsd" }, { "chid", "clwd" }, { "hond", "offd" } };
static const char *const RESOLUTION_MUTED[2] = { "640m", "320m" };
/* F1..F4: the setting they step */
static const int16_t F_ITEM[4] = { WINDOW, IMAGE, VIEW, HORIZON };

typedef struct Menu {
    int32_t bank;              /* ebp */
    int16_t selected;          /* esp + 0x470 */
    int16_t previous;          /* esp + 0x478 */
    int changed;               /* esp + 0x46c */
    int redraw;                /* esp + 0x464 */
    int fade;                  /* esp + 0x468: the palette faded in on the next draw */
    int armed;                 /* esp + 0x474: the keys taken */
    int32_t rest;              /* esp + 0x488: the ticks summed while a key is held */
    int released;              /* esp + 0x484: the buttons seen up */
    int16_t leave;             /* esp + 0x480 */
    int cancelled;             /* port: Cancel or Escape put the settings back */
    PortSettings kept_port;    /* port: the port's settings on entry, put back with them */
    uint8_t kept[GRAPHICS_KEPT];  /* esp + 0x300 */
    uint8_t rgb[GRAPHICS_PALETTE];  /* esp + 0 */
} Menu;

static int16_t get(const GraphicsMenuState *st, int at)
{
    int16_t v;
    memcpy(&v, st->config + at, sizeof v);
    return v;
}

static void put(const GraphicsMenuState *st, int at, int16_t v)
{
    memcpy(st->config + at, &v, sizeof v);
}

static void move_to(int16_t item, const GraphicsMenuOps *o)
{
    o->pointer_move_to(o->ctx, POINTER_X[item], POINTER_Y[item]);
}

static void blit(const Menu *m, const char *tag, const GraphicsMenuOps *o)
{
    o->blit_own(o->ctx, o->shape(o->ctx, m->bank, tag));
}

static const char *value(int i, int lit, const GraphicsMenuState *st)
{
    return (lit ? VALUE_LIT : VALUE_DARK)[i][get(st, SETTING[i])];
}

/* a setting's value art, and over the view's "far" the port's "ultra" when that is set */
static void put_value(const Menu *m, int i, int lit, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    blit(m, value(i, lit, st), o);
    if (i == VIEW && st->plus && st->plus->port->view_ultra && get(st, GRAPHICS_VIEW) == 0)
        graphics_plus_ultra(st, o, m->rgb, lit);
}

/* the first band the pointer is in becomes the selection */
static void follow(Menu *m, int32_t mode, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    if (st->plus && graphics_plus_nav_hit(o)) return;   /* port: over the page indicator, no item */
    for (int16_t i = 0; i < ITEMS; i++) {
        if (!o->pointer_in_rect(o->ctx, BAND_LEFT[i], BAND_RIGHT[i], BAND_TOP[i], BAND_BOTTOM[i])) continue;
        if (i == RESOLUTION && (int16_t)mode != 0) continue;
        if (m->selected != i) {
            m->previous = m->selected;
            m->selected = i;
            m->changed = 1;
        }
        return;
    }
}

static void draw(Menu *m, int32_t mode, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    const int fade_drawn = m->fade;
    o->select(o->ctx, *st->screen);
    blit(m, "bgnd", o);
    for (int i = 0; i < SETTINGS; i++) {
        if (m->selected == i) blit(m, LIT[i], o);
        put_value(m, i, m->selected == i, st, o);
    }
    if (m->selected == DONE) blit(m, LIT[DONE], o);
    if (m->selected == CANCEL) blit(m, LIT[CANCEL], o);
    if ((int16_t)mode != 0) {
        blit(m, "scrm", o);
        blit(m, RESOLUTION_MUTED[get(st, GRAPHICS_RESOLUTION)], o);
    }
    if (st->plus) graphics_plus_nav(st, o, 0);
    o->select_default(o->ctx);
    o->pointer_hide(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, *st->screen));
    o->unlock(o->ctx);
    o->flip(o->ctx);
    o->pointer_show(o->ctx);
    if (m->fade) {
        o->pointer_mark(o->ctx);
        if (*st->pointer_active) o->pointer_buttons(o->ctx);
        o->text_styles(o->ctx);
        o->fade_in(o->ctx, 0, ALL, m->rgb);
        m->fade = 0;
        /* port: the text's colours are matched to the palette, which is only the screen's once it has faded in; the
         * picture is drawn once more with them */
        if (st->plus) o->text_styles(o->ctx);
    }
    o->deadline_set(o->ctx, WAIT);
    m->redraw = st->plus && !m->fade && fade_drawn;
}

/* an item drawn lit or dark over the screen as it is: its label, then its setting's value */
static void draw_item(const Menu *m, uint16_t i, int lit, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    if (i > CANCEL) return;
    blit(m, (lit ? LIT : DARK)[i], o);
    if (i < SETTINGS) put_value(m, i, lit, st, o);
}

/* the two items again; the previous one is left as it was */
static void redraw_items(Menu *m, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    m->redraw = 1;
    o->select(o->ctx, *st->screen);
    o->sound(o->ctx, MOVED);
    o->pointer_hide(o->ctx);
    o->lock(o->ctx);
    draw_item(m, (uint16_t)m->previous, 0, st, o);
    draw_item(m, (uint16_t)m->selected, 1, st, o);
    m->changed = 0;
}

/* a setting the next of its values: at 320 the image at most 1 (the resolution) or back to 0 (the image) */
static void step_setting(int i, const GraphicsMenuState *st)
{
    if (i == VIEW && st->plus) {   /* port: far, medium, close, ultra */
        int16_t view = get(st, GRAPHICS_VIEW);
        graphics_plus_view_step(&view, &st->plus->port->view_ultra);
        put(st, GRAPHICS_VIEW, view);
        return;
    }
    switch (i) {
    case RESOLUTION:
    case CARS:
    case HORIZON:
        put(st, SETTING[i], get(st, SETTING[i]) ^ 1);
        break;
    case IMAGE:
        put(st, GRAPHICS_IMAGE, (int16_t)((get(st, GRAPHICS_IMAGE) + 1) % 4));
        if (get(st, GRAPHICS_RESOLUTION) == LOW && get(st, GRAPHICS_IMAGE) > LOW) put(st, GRAPHICS_IMAGE, 0);
        break;
    default:
        put(st, SETTING[i], (int16_t)((get(st, SETTING[i]) + 1) % 3));
        break;
    }
}

/* a setting chosen: the next value lit; the resolution to 320 dims the image down to 1 */
static void choose_setting(Menu *m, int i, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    step_setting(i, st);
    if (i == RESOLUTION && get(st, GRAPHICS_RESOLUTION) == LOW && get(st, GRAPHICS_IMAGE) > LOW) {
        put(st, GRAPHICS_IMAGE, LOW);
        o->pointer_hide(o->ctx);
        blit(m, value(IMAGE, 0, st), o);
    }
    o->pointer_hide(o->ctx);
    put_value(m, i, 1, st, o);
    if (*st->pointer_active == 0) move_to(m->selected, o);
    m->changed = 1;
}

static void put_back(Menu *m, const GraphicsMenuState *st)
{
    memcpy(st->config, m->kept, sizeof m->kept);
    if (st->plus) *st->plus->port = m->kept_port;
    m->cancelled = 1;
    m->leave = 1;
}

/* port: the second page; Done leaves the screen, Cancel puts everything back, back returns to page 1 drawn again */
static void second_page(Menu *m, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    const PlusPageResult r = graphics_plus_page(st, o, m->bank, m->rgb);
    if (r == PLUS_PAGE_CANCEL) put_back(m, st);
    else if (r == PLUS_PAGE_DONE) m->leave = 1;
    m->redraw = 1;
    m->released = m->armed = 0;
    m->rest = 0;
}

/* Space, Enter or the left button: the item chosen when the keys are taken */
static void chosen(Menu *m, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    o->pointer_click(o->ctx);
    o->select(o->ctx, *st->screen);
    if (m->armed) {
        o->sound(o->ctx, CHOSEN);
        const uint16_t i = (uint16_t)m->selected;
        if (i < SETTINGS) choose_setting(m, i, st, o);
        else if (i == DONE) m->leave = 1;
        else if (i == CANCEL) put_back(m, st);
    }
    o->ticks(o->ctx);
    m->released = m->armed = 0;
    m->rest = 0;
}

/* F1..F4: a setting the next of its values at once, the pointer to it */
static void function_key(Menu *m, int i, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    o->sound(o->ctx, CHOSEN);
    step_setting(i, st);
    put_value(m, i, 1, st, o);
    m->changed = 1;
    m->released = m->armed = 0;
    m->rest = 0;
    move_to((int16_t)i, o);
    o->ticks(o->ctx);
}

/* a key other than the choosing ones: Escape puts back and leaves, the arrows step */
static void step(Menu *m, int32_t mode, uint16_t k, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    const int from_race = (int16_t)mode != 0;
    if (k == ESCAPE) {
        put_back(m, st);
        o->sound(o->ctx, CHOSEN);
    } else if (st->plus && (k == PAGE_DOWN || (k == RIGHT && m->selected < DONE))) {
        second_page(m, st, o);
    } else if (k == UP) {
        o->pointer_off(o->ctx);
        m->previous = m->selected;
        if (m->selected == CANCEL) m->selected = DONE;
        m->selected--;
        if (m->selected < 0) m->selected = DONE;
        if (from_race && m->selected == RESOLUTION) m->selected = DONE;
        m->changed = 1;
        move_to(m->selected, o);
    } else if (k == DOWN) {
        o->pointer_off(o->ctx);
        m->previous = m->selected;
        m->selected = (int16_t)((m->selected + 1) % 7);
        if (m->previous == CANCEL) m->selected = RESOLUTION;
        if (from_race && m->selected == RESOLUTION) m->selected = WINDOW;
        m->changed = 1;
        move_to(m->selected, o);
    } else if ((k == LEFT || k == RIGHT) && (m->selected == DONE || m->selected == CANCEL)) {
        o->pointer_off(o->ctx);
        m->previous = m->selected;
        m->selected = m->selected == DONE ? CANCEL : DONE;
        m->changed = 1;
        move_to(m->selected, o);
    }
    o->ticks(o->ctx);
    m->armed = 0;
    m->rest = 0;
}

static void open_screen(Menu *m, int32_t mode, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    memcpy(m->kept, st->config, sizeof m->kept);
    *st->showing = 1;
    if ((int16_t)mode != 0) m->selected = m->previous = WINDOW;
    move_to(m->selected, o);
    m->bank = o->load_art(o->ctx, *st->net_off ? "check/graphics.fsh" : "option/graphics.qfs");
    *st->inverse = o->load_inverse(o->ctx, *st->net_off ? "check/graphics.inv" : "option/graphics.inv");
    o->music_feed(o->ctx);
    const uint8_t *pal = o->shape(o->ctx, m->bank, "!pal");
    o->palette_set(o->ctx, m->rgb, 0, ALL, pal + 0x10, pal[0]);
    if (st->plus) m->kept_port = *st->plus->port;
    if (*st->pointer_active) o->pointer_poll(o->ctx);
    o->pointer_mark(o->ctx);
}

static void close_screen(Menu *m, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    *st->showing = 0;
    o->fade_out(o->ctx, 0, ALL);
    o->music_feed(o->ctx);
    o->free_inverse(o->ctx);
    o->free(o->ctx, m->bank);
    o->pointer_mark(o->ctx);
}

/* 00423d28 */
int32_t graphics_menu(const GraphicsMenuState *st, int32_t mode, const GraphicsMenuOps *ops)
{
    static Menu zero;
    Menu m = zero;
    m.fade = m.redraw = m.armed = m.released = 1;
    open_screen(&m, mode, st, ops);
    do {
        follow(&m, mode, st, ops);
        if (m.redraw) draw(&m, mode, st, ops);
        if (m.changed) redraw_items(&m, st, ops);
        if (!m.armed) {
            m.rest += ops->ticks(ops->ctx);
            if ((int16_t)m.rest > REST) m.armed = 1;
        }
        if (*st->pointer_active) *st->buttons = (int16_t)ops->pointer_buttons(ops->ctx);
        if (*st->buttons == 0) m.released = 1;
        ops->pointer_activate_if_moved(ops->ctx);
        const uint16_t k = (uint16_t)ops->key(ops->ctx);
        if (st->plus && *st->buttons == LEFT_BUTTON && m.released && graphics_plus_nav_hit(ops)) {
            second_page(&m, st, ops);
        } else if (k == SPACE || k == ENTER || (*st->buttons == LEFT_BUTTON && m.released)) {
            chosen(&m, st, ops);
        } else if (m.armed && k >= F1 && k <= F4 && (k & 0xff) == 0) {
            function_key(&m, F_ITEM[(k - F1) >> 8], st, ops);
        } else if (k == 0) {
            m.armed = 1;
        } else if (m.armed) {
            step(&m, mode, k, st, ops);
        }
        ops->music(ops->ctx);
        if (m.fade) continue;
        if (*st->net_off == 0) ops->net_sync(ops->ctx);
        if (ops->deadline_passed(ops->ctx) || (*st->buttons == RIGHT_BUTTON && m.released) || *st->quit == 1) {
            ops->sound(ops->ctx, CHOSEN);
            m.leave = 1;
        }
    } while (!m.leave);
    close_screen(&m, st, ops);
    if (st->plus && !m.cancelled) ops->plus_save(ops->ctx, st->plus->port);
    return m.leave;
}
