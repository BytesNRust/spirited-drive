#include "game/frontend/options_menu.h"

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { TRANSMISSION, ABS, TRACTION, AUDIO, GRAPHICS, CONTROLS, LOAD_SAVE, CREDITS, DONE, CANCEL, ITEMS };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x2ee0, REST = 0x19, WIDE = 0x280, TALL = 0x1e0, ALL = 0x100 };
enum { NOT_OFFERED = 2, TOURNAMENT = 3, CIRCUIT = 2, TIME_TRIAL = 0, TRACKS = 9 };

/* 00444ad0 / 00444ae4: where the pointer goes per item */
static const int16_t POINTER_X[ITEMS] = { 240, 240, 240, 240, 240, 240, 240, 240, 40, 565 };
static const int16_t POINTER_Y[ITEMS] = { 118, 151, 185, 220, 253, 287, 324, 363, 450, 450 };
/* the bands 00463f80 is asked about, in turn: the eight rows across the screen, then Done and Cancel side by side */
static const int16_t BAND_LEFT[ITEMS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x143 };
static const int16_t BAND_RIGHT[ITEMS] = { WIDE, WIDE, WIDE, WIDE, WIDE, WIDE, WIDE, WIDE, 0x141, WIDE };
static const int16_t BAND_TOP[ITEMS] = { 0, 0x86, 0xa8, 0xcc, 0xed, 0x10f, 0x133, 0x156, 0x185, 0x185 };
static const int16_t BAND_BOTTOM[ITEMS] = { 0x85, 0xa7, 0xcb, 0xec, 0x10e, 0x132, 0x155, 0x184, TALL, 0x1df };
static const char *const LIT[ITEMS] = { "Tral", "Absl", "Trcl", "Audl", "Gral", "Conl", "Loal", "Crel", "Donl", "Canl" };
static const char *const DARK[ITEMS] = { "Trad", "Absd", "Trcd", "Audd", "Grad", "Cond", "Load", "Cred", "Dond", "Cand" };
/* 00444af8..00444b34, copied to the stack: the settings' values, lit and dark */
static const char *const GEAR_LIT[2] = { "Autl", "Manl" };
static const char *const GEAR_DARK[2] = { "Autd", "Mand" };
static const char *const ABS_LIT[3] = { "On1l", "Of1l", "N/1l" };
static const char *const ABS_DARK[3] = { "On1d", "Of1d", "N/1d" };
static const char *const TRACTION_LIT[3] = { "On2l", "Of2l", "N/2l" };
static const char *const TRACTION_DARK[3] = { "On2d", "Of2d", "N/2d" };
/* 004baef8: a tournament's car per track */
static const int16_t TOURNAMENT_CAR[TRACKS] = { 0, 3, 6, 0, 3, 3, 6, 6, 7 };

typedef struct Menu {
    int32_t bank;           /* edi */
    const uint8_t *pal;     /* ebp: the bank's "!pal" */
    int16_t selected;       /* esi */
    int16_t previous;       /* esp + 0x388 */
    int changed;            /* esp + 0x390 */
    int redraw;             /* esp + 0x384 */
    int fade;               /* esp + 0x38c: the palette faded in on the next draw */
    int armed;              /* esp + 0x380: the keys taken */
    int32_t rest;           /* esp + 0x394: the ticks summed while a key is held */
    int released;           /* esp + 0x398: the buttons seen up */
    int done;               /* esp + 0x39c */
    int loaded;             /* esp + 0x374: back from the load / save menu */
    int16_t kept[4];        /* esp + 0x378, 0x370, 0x368, 0x36c: 00524e87, 00524e7d, 00524e89, 00524e7f */
    uint8_t rgb[OPTIONS_MENU_PALETTE];  /* esp + 0 */
} Menu;

static void move_to(int16_t item, const OptionsMenuOps *o)
{
    o->pointer_move_to(o->ctx, POINTER_X[item], POINTER_Y[item]);
}

static void blit(const Menu *m, const char *tag, const OptionsMenuOps *o)
{
    o->blit_own(o->ctx, o->shape(o->ctx, m->bank, tag));
}

static void palette(Menu *m, const OptionsMenuOps *o)
{
    m->pal = o->shape(o->ctx, m->bank, "!pal");
    o->palette_set(o->ctx, m->rgb, 0, ALL, m->pal + 0x10, m->pal[0]);
}

/* the first band the pointer is in becomes the selection, Load / save only with the connection up */
static void follow(Menu *m, const OptionsMenuState *st, const OptionsMenuOps *o)
{
    for (int16_t i = 0; i < ITEMS; i++) {
        if (!o->pointer_in_rect(o->ctx, BAND_LEFT[i], BAND_RIGHT[i], BAND_TOP[i], BAND_BOTTOM[i])) continue;
        if (m->selected != i && (i != LOAD_SAVE || *st->lost == 0)) {
            m->previous = m->selected;
            m->selected = i;
            m->changed = 1;
        }
        return;
    }
}

/* the three settings: the label lit with its value lit when selected, else the value dark alone */
static void draw_setting(const Menu *m, int16_t item, const char *const *lit, const char *const *dark, int16_t value,
                         const OptionsMenuOps *o)
{
    if (m->selected == item) {
        blit(m, LIT[item], o);
        blit(m, lit[value], o);
    } else {
        blit(m, dark[value], o);
    }
}

static void draw(Menu *m, const OptionsMenuState *st, const OptionsMenuOps *o)
{
    o->select(o->ctx, *st->screen);
    blit(m, "bgnd", o);
    draw_setting(m, TRANSMISSION, GEAR_LIT, GEAR_DARK, *st->transmission, o);
    draw_setting(m, ABS, ABS_LIT, ABS_DARK, *st->abs, o);
    draw_setting(m, TRACTION, TRACTION_LIT, TRACTION_DARK, *st->traction, o);
    for (int16_t i = AUDIO; i < ITEMS; i++)
        if (m->selected == i) blit(m, LIT[i], o);
    if (*st->lost) blit(m, "Loam", o);
    o->select_default(o->ctx);
    o->pointer_hide(o->ctx);
    o->lock(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, *st->screen));
    o->unlock(o->ctx);
    o->flip(o->ctx);
    o->pointer_show(o->ctx);
    if (m->fade) {
        o->pointer_mark(o->ctx);
        if (*st->pointer_active) o->pointer_buttons(o->ctx);
        o->palette_set(o->ctx, m->rgb, 0, ALL, m->pal + 0x10, m->pal[0]);
        o->fade_in(o->ctx, 0, ALL, m->rgb);
        o->text_styles(o->ctx);
        m->fade = 0;
    }
    o->deadline_set(o->ctx, WAIT);
    m->changed = 0;
    m->redraw = 0;
}

/* the item left goes dark (a setting's label and value), the new one lit; the screen stays locked */
static void redraw_items(Menu *m, const OptionsMenuState *st, const OptionsMenuOps *o)
{
    o->select(o->ctx, *st->screen);
    m->redraw = 1;
    o->sound(o->ctx, MOVED);
    o->pointer_hide(o->ctx);
    o->lock(o->ctx);
    if ((uint16_t)m->previous < ITEMS) {
        blit(m, DARK[m->previous], o);
        if (m->previous == TRANSMISSION) blit(m, GEAR_DARK[*st->transmission], o);
        else if (m->previous == ABS) blit(m, ABS_DARK[*st->abs], o);
        else if (m->previous == TRACTION) blit(m, TRACTION_DARK[*st->traction], o);
    }
    if ((uint16_t)m->selected < ITEMS) {
        blit(m, LIT[m->selected], o);
        if (m->selected == TRANSMISSION) blit(m, GEAR_LIT[*st->transmission], o);
        else if (m->selected == ABS) blit(m, ABS_LIT[*st->abs], o);
        else if (m->selected == TRACTION) blit(m, TRACTION_LIT[*st->traction], o);
    }
    m->changed = 0;
}

/* a setting toggled: its new value lit over the old, the pointer put back on it */
static void toggled(Menu *m, const char *tag, const OptionsMenuState *st, const OptionsMenuOps *o)
{
    o->pointer_hide(o->ctx);
    blit(m, tag, o);
    if (*st->pointer_active == 0) move_to(m->selected, o);
    m->changed = 1;
}

/* back from a sub-screen: the pointer on its item, the inverse table loaded again, all drawn and faded in */
static void back(Menu *m, const OptionsMenuState *st, const OptionsMenuOps *o)
{
    move_to(m->selected, o);
    *st->inverse = o->load_inverse(o->ctx, "option/options.inv");
    m->redraw = 1;
    m->fade = 1;
}

/* after a load / save the race's settings are set again for its type */
static void settle(const Menu *m, const OptionsMenuState *st, const OptionsMenuOps *o)
{
    if (!m->loaded) return;
    if (*st->race_type == TOURNAMENT) {
        *st->player_car = TOURNAMENT_CAR[*st->track];
        o->car_flags(o->ctx);
        *st->opponent = 10;
        *st->difficulty = 1;
        *st->laps = *st->track >= 4 && *st->track <= 7 ? 6 : 8;
    } else if (*st->race_type == CIRCUIT) {
        if (*st->lost) *st->net_car = 8;
        else *st->opponent = 8;
    } else if (*st->race_type == TIME_TRIAL) {
        *st->opponent = 9;
    } else if (*st->opponent > 7) {
        *st->opponent = *st->player_car;
    }
}

/* Cancel and Escape: the three settings as they were */
static void put_back(Menu *m, const OptionsMenuState *st, const OptionsMenuOps *o)
{
    *st->transmission = m->kept[0];
    *st->automatic = m->kept[1];
    *st->abs = m->kept[2];
    *st->traction = m->kept[3];
    m->done = 1;
    o->music_fade_to(o->ctx, *st->music_volume);
}

static void choose(Menu *m, const OptionsMenuState *st, const OptionsMenuOps *o)
{
    switch ((uint16_t)m->selected) {
    case TRANSMISSION:
        *st->transmission ^= 1;
        *st->automatic = *st->transmission == 0;
        toggled(m, GEAR_LIT[*st->transmission], st, o);
        break;
    case ABS:
        if (*st->abs == NOT_OFFERED) break;
        *st->abs ^= 1;
        toggled(m, ABS_LIT[*st->abs], st, o);
        break;
    case TRACTION:
        if (*st->traction == NOT_OFFERED) break;
        *st->traction ^= 1;
        toggled(m, TRACTION_LIT[*st->traction], st, o);
        break;
    case AUDIO:
    case GRAPHICS:
    case CONTROLS:
        o->fade_out(o->ctx, 0, ALL);
        o->free_inverse(o->ctx);
        if (m->selected == AUDIO) o->audio(o->ctx, 0);
        else if (m->selected == GRAPHICS) o->graphics(o->ctx, 0);
        else o->controls(o->ctx, 0);
        back(m, st, o);
        break;
    case LOAD_SAVE:
        o->fade_out(o->ctx, 0, ALL);
        o->free_inverse(o->ctx);
        o->free(o->ctx, m->bank);
        o->load_save(o->ctx, 0);
        move_to(m->selected, o);
        m->bank = o->load_art(o->ctx, "option/options.qfs");
        m->loaded = 1;
        palette(m, o);
        *st->inverse = o->load_inverse(o->ctx, "option/options.inv");
        o->music_feed(o->ctx);
        m->redraw = 1;
        m->fade = 1;
        break;
    case CREDITS:
        o->fade_out(o->ctx, 0, ALL);
        o->free_inverse(o->ctx);
        o->music_fade_out(o->ctx);
        o->music_end(o->ctx);
        o->music_start(o->ctx, "credits.asf");
        o->credits(o->ctx, 0);
        o->music_fade_out(o->ctx);
        o->music_end(o->ctx);
        o->music_pick(o->ctx);
        back(m, st, o);
        break;
    case DONE:
        settle(m, st, o);
        m->done = 1;
        break;
    case CANCEL:
        put_back(m, st, o);
        break;
    default:
        break;
    }
}

/* Up: back one, Cancel to Credits, over Load / save while the connection is lost, Transmission to Done */
static void step_up(Menu *m, const OptionsMenuState *st)
{
    if (m->selected == CANCEL) m->selected--;
    m->selected--;
    if (m->selected < 0) m->selected = DONE;
    if (*st->lost && m->selected == LOAD_SAVE) m->selected--;
}

/* Down: Done and Cancel to Transmission, else on one */
static void step_down(Menu *m, const OptionsMenuState *st)
{
    if (m->selected == DONE || m->selected == CANCEL) {
        m->selected = TRANSMISSION;
        return;
    }
    m->selected = (int16_t)((m->selected + 1) % ITEMS);
    if (*st->lost && m->selected == LOAD_SAVE) m->selected = (int16_t)((m->selected + 1) % ITEMS);
}

/* a key other than the choosing ones: Escape puts back and leaves, the arrows step */
static void step(Menu *m, const OptionsMenuState *st, uint16_t k, const OptionsMenuOps *o)
{
    if (k == ESCAPE) {
        o->sound(o->ctx, CHOSEN);
        put_back(m, st, o);
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        m->previous = m->selected;
        if (k == UP) step_up(m, st);
        else step_down(m, st);
        move_to(m->selected, o);
        m->changed = 1;
    } else if ((k == LEFT || k == RIGHT) && (m->selected == DONE || m->selected == CANCEL)) {
        const int16_t to = m->selected == DONE ? CANCEL : DONE;
        o->pointer_off(o->ctx);
        move_to(to, o);
        m->previous = m->selected;
        m->selected = to;
        m->changed = 1;
    }
    m->armed = 0;
    m->rest = 0;
    o->ticks(o->ctx);
}

/* 00444bb0 */
int32_t options_menu(OptionsMenuState *st, const OptionsMenuOps *ops)
{
    static Menu zero;
    Menu m = zero;
    m.kept[0] = *st->transmission, m.kept[1] = *st->automatic, m.kept[2] = *st->abs, m.kept[3] = *st->traction;
    move_to(TRANSMISSION, ops);
    m.bank = ops->load_art(ops->ctx, "option/options.qfs");
    *st->inverse = ops->load_inverse(ops->ctx, "option/options.inv");
    ops->music_feed(ops->ctx);
    m.fade = m.redraw = m.armed = m.released = 1;
    palette(&m, ops);
    ops->text_styles(ops->ctx);
    if (*st->pointer_active) ops->pointer_poll(ops->ctx);
    ops->pointer_mark(ops->ctx);
    do {
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
                ops->select(ops->ctx, *st->screen);
                ops->sound(ops->ctx, CHOSEN);
                choose(&m, st, ops);
            }
            m.released = 0;
            m.armed = 0;
            m.rest = 0;
            ops->ticks(ops->ctx);
        } else if (k == 0) {
            m.armed = 1;
        } else if (m.armed) {
            step(&m, st, k, ops);
        }
        ops->music(ops->ctx);
        if (m.fade) continue;
        if (*st->net_off == 0) ops->net_sync(ops->ctx);
        if (ops->deadline_passed(ops->ctx) || (*st->buttons == RIGHT_BUTTON && m.released) || *st->quit == 1) {
            settle(&m, st, ops);
            ops->sound(ops->ctx, CHOSEN);
            m.done = 1;
        }
    } while (!m.done);
    ops->fade_out(ops->ctx, 0, ALL);
    ops->free_inverse(ops->ctx);
    ops->free(ops->ctx, m.bank);
    ops->music_feed(ops->ctx);
    ops->deadline_set(ops->ctx, WAIT);
    *st->redraw_asked = 1;
    return 0;
}
