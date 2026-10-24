#include "game/frontend/controls_menu.h"
#include "game/frontend/controls_plus.h"
#include "game/sys/key_codes.h"

#include <stdio.h>
#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { STEERING, THROTTLE, SHIFT, CAMERA, HANDBRAKE, HORN, CALIBRATE, DONE, CANCEL, ITEMS };
enum { FIELDS = 6, DEVICES = 6, KEYBOARD = 0 };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x2ee0, CLASH_WAIT = 200, REST = 0x19, WIDE = 0x280, ALL = 0x100 };
enum { FIELD_X = 400, PICTURE_W = 0xf0, PICTURE_H = 0x28, TEXT = 80, NAME = 40 };

/* 00413aa0 / 00413ab2: where the pointer goes per item */
static const int16_t POINTER_X[ITEMS] = { 200, 200, 200, 200, 200, 200, 200, 50, 565 };
static const int16_t POINTER_Y[ITEMS] = { 118, 157, 196, 237, 277, 317, 356, 450, 450 };
/* the bands 00463f80 is asked about, in turn (Calibrate's only while offered) */
static const int16_t BAND_LEFT[ITEMS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0x145 };
static const int16_t BAND_RIGHT[ITEMS] = { WIDE, WIDE, WIDE, WIDE, WIDE, WIDE, WIDE, 0x140, WIDE };
static const int16_t BAND_TOP[ITEMS] = { 0, 0x8a, 0xb2, 0xda, 0x102, 0x12c, 0x152, 0x195, 0x195 };
static const int16_t BAND_BOTTOM[ITEMS] = { 0x86, 0xae, 0xd6, 0xfe, 0x127, 0x14e, 0x18b, 0x1e0, 0x1df };
/* the fields' rows, each with its picture of the background under it */
static const int16_t FIELD_Y[FIELDS] = { 0x68, 0x90, 0xb8, 0xe0, 0x108, 0x130 };
static const char *const LIT[ITEMS] = { "strl", "accl", "shfl", "caml", "hndl", "hrnl", "call", "donl", "canl" };
static const char *const DARK[ITEMS] = { "strd", "accd", "shfd", "camd", "hndd", "hrnd", "cald", "dond", "cand" };
/* the key items' second labels, lit and dark */
static const char *const KEY_LIT[3] = { "cakl", "hbkl", "hnkl" };
static const char *const KEY_DARK[3] = { "cakd", "hbkd", "hnkd" };
/* 00413ac4..00413b3c, copied to the stack: the device items' devices, dark and lit */
static const char *const DEVICE_DARK[3][DEVICES] = {
    { "stkd", "stjd", "stjd", "stjd", "stmd", "stmd" },
    { "ackd", "acjd", "acjd", "acjd", "acmd", "acmd" },
    { "shkd", "shjd", "shjd", "shjd", "shmd", "shmd" } };
static const char *const DEVICE_LIT[3][DEVICES] = {
    { "stkl", "stjl", "stjl", "stjl", "stml", "stml" },
    { "ackl", "acjl", "acjl", "acjl", "acml", "acml" },
    { "shkl", "shjl", "shjl", "shjl", "shml", "shml" } };
/* 00413b54 / 00413bcc: the devices' names (00413cb0 sprintf's the German over the English) */
static const char *const DEVICE_NAME[2][DEVICES] = {
    { "Keyboard", "Left/Right", "Up/Down", "Button1/Button2", "Left/Right", "Button1/Button2" },
    { "Keyboard", "Links/Rechts", "Hoch/Runter", "Taste1/Taste2", "Links/Rechts", "Taste1/Taste2" } };
/* the keyboard's prompts for the pairs, and the key items' */
static const char *const PROMPT[3][2] = { { "ensl", "ensr" }, { "enac", "enbr" }, { "enup", "endn" } };
static const char *const KEY_PROMPT = "enhb";
/* the controls' shorts: the device items' devices, their pairs of keys, the key items' keys */
static const int PAIR[3][2] = { { 3, 4 }, { 5, 6 }, { 7, 8 } };
static const int KEY[3] = { 10, 9, 11 };

typedef struct Menu {
    int32_t bank;            /* ebp */
    int32_t pictures[FIELDS];  /* esp + 0x93c: the background under each field */
    int32_t scratch;         /* esp + 0xa0c: a field drawn before it is put on the screen */
    const char *const *names;  /* esp + 0x858 */
    int16_t selected;        /* esp + 0xa34 */
    uint16_t previous;       /* esp + 0xa10 */
    int changed;             /* esp + 0xa14 */
    int redraw;              /* esp + 0xa18 */
    int fade;                /* esp + 0xa28: the palette faded in on the next draw */
    int armed;               /* esp + 0xa1c: the keys taken */
    int32_t rest;            /* esp + 0xa30: the ticks summed while a key is held */
    int released;            /* esp + 0xa2c: the buttons seen up */
    int16_t leave;           /* esp + 0xa20 */
    PortControls kept_port;  /* port: controls.cfg's bindings on entry */
    int cancelled;           /* port: Cancel or Escape: nothing is saved */
    int nav_lit;             /* port: the page indicator's triangle the pointer is on (controls_plus.h), 0 none */
    int16_t kept[CONTROLS_SHORTS];  /* esp + 0x920: the controls on entry */
    char text[FIELDS][TEXT]; /* esp + 0x600: the fields' texts */
    char first[NAME];        /* esp + 0x8f8: a key's name */
    char second[NAME];       /* esp + 0x8d0 */
    uint8_t black[CONTROLS_PALETTE];  /* esp + 0x300 */
    uint8_t rgb[CONTROLS_PALETTE];    /* esp + 0 */
} Menu;

static void move_to(int16_t item, const ControlsMenuOps *o)
{
    o->pointer_move_to(o->ctx, POINTER_X[item], POINTER_Y[item]);
}

static void blit(const Menu *m, const char *tag, const ControlsMenuOps *o)
{
    o->blit_own(o->ctx, o->shape(o->ctx, m->bank, tag));
}

static void palette(Menu *m, const ControlsMenuOps *o)
{
    const uint8_t *pal = o->shape(o->ctx, m->bank, "!pal");
    o->palette_set(o->ctx, m->rgb, 0, ALL, pal + 0x10, pal[0]);
}

static void show(const ControlsMenuState *st, const ControlsMenuOps *o)
{
    o->select_default(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, *st->screen));
    o->flip(o->ctx);
}

/* a field's background put back on the screen, with its text over it when given */
static void field(const Menu *m, int i, const char *text, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    o->select(o->ctx, m->scratch);
    o->blit_at(o->ctx, o->picture_shape(o->ctx, m->pictures[i]), 0, 0);
    if (text) o->text_shadowed(o->ctx, text, 0, 0);
    o->select(o->ctx, *st->screen);
    o->blit_at(o->ctx, o->picture_shape(o->ctx, m->scratch), FIELD_X, FIELD_Y[i]);
}

/* the first band the pointer is in becomes the selection; over: the pointer on the fields */
static int follow(Menu *m, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    const int over = o->pointer_in_rect(o->ctx, 0x186, WIDE, 100, 0x14a) != 0;
    if (st->plus) {   /* port: the indicator's triangle the pointer is on lights */
        const int lit = controls_plus_nav_lit(o, 0);
        if (lit != m->nav_lit) m->nav_lit = lit, m->redraw = 1;
    }
    if (st->plus && controls_plus_nav_hit(o)) return over;   /* port: over the page indicator, no item */
    for (int16_t i = 0; i < ITEMS; i++) {
        if (i == CALIBRATE && *st->calibration == 0) continue;
        if (!o->pointer_in_rect(o->ctx, BAND_LEFT[i], BAND_RIGHT[i], BAND_TOP[i], BAND_BOTTOM[i])) continue;
        if (m->selected != i) {
            m->previous = (uint16_t)m->selected;
            m->selected = i;
            m->changed = 1;
        }
        break;
    }
    return over;
}

/* port: the name of the key the controls short `at` holds. With the player's own keys (controls.cfg, the keyboard page)
 * the race uses those, so those are shown: display only, this screen's own flow edits the shorts as it did. */
static void key_label(int at, char *out, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    const int key = st->plus != NULL ? port_controls_key_of_word(at) : -1;
    if (key >= 0 && st->plus->port->keys_custom) {
        const int code = st->plus->port->key[key];
        snprintf(out, NAME, "%s", "NONE");
        if (code != 0 && o->plus_key_name != NULL) o->plus_key_name(o->ctx, code, out, NAME);
        return;
    }
    o->key_name(o->ctx, st->controls[at], out);
}

/* a device item's text: the keyboard's two keys, or the device's name */
static void device_text(Menu *m, int i, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    const int16_t *c = st->controls;
    if (c[i] == KEYBOARD) {
        key_label(PAIR[i][0], m->first, st, o);
        key_label(PAIR[i][1], m->second, st, o);
        snprintf(m->text[i], TEXT, "%s/%s", m->first, m->second);
    } else {
        snprintf(m->text[i], TEXT, "%s", m->names[c[i]]);
    }
}

/* the selection lit in a whole draw: its labels, its text on the screen itself */
static void draw_lit(const Menu *m, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    const int16_t i = m->selected;
    if (i >= STEERING && i <= SHIFT) {
        blit(m, LIT[i], o);
        blit(m, DEVICE_LIT[i][st->controls[i]], o);
        o->text_shadowed(o->ctx, m->text[i], FIELD_X, FIELD_Y[i]);
    } else if (i >= CAMERA && i <= HORN) {
        blit(m, LIT[i], o);
        blit(m, KEY_LIT[i - CAMERA], o);
        o->text_shadowed(o->ctx, m->text[i], FIELD_X, FIELD_Y[i]);
    } else if (i >= CALIBRATE && i <= CANCEL) {
        blit(m, LIT[i], o);
    }
}

static void draw(Menu *m, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    const int16_t *c = st->controls;
    o->text_select(o->ctx, 0);
    o->select(o->ctx, *st->screen);
    blit(m, "bgnd", o);
    for (int i = 0; i < FIELDS; i++)
        o->blit_grab(o->ctx, o->picture_shape(o->ctx, m->pictures[i]), FIELD_X, FIELD_Y[i]);
    for (int i = STEERING; i <= SHIFT; i++) blit(m, DEVICE_DARK[i][c[i]], o);
    for (int i = STEERING; i <= SHIFT; i++) {
        device_text(m, i, st, o);
        o->text_shadowed(o->ctx, m->text[i], FIELD_X, FIELD_Y[i]);
    }
    for (int i = CAMERA; i <= HORN; i++) {
        key_label(KEY[i - CAMERA], m->first, st, o);
        snprintf(m->text[i], TEXT, "%s", m->first);
        o->text_shadowed(o->ctx, m->text[i], FIELD_X, FIELD_Y[i]);
    }
    o->text_select(o->ctx, 1);
    draw_lit(m, st, o);
    if (st->plus) controls_plus_nav(o, 0, m->nav_lit);   /* port */
    if (m->fade) o->palette_put(o->ctx, 0, ALL, m->black);
    if (!m->changed) {
        show(st, o);
        o->pointer_show(o->ctx);
    }
    if (m->fade) {
        o->fade_in(o->ctx, 0, ALL, m->rgb);
        o->text_styles(o->ctx);
        m->fade = 0;
    }
    o->deadline_set(o->ctx, WAIT);
    m->redraw = 0;
}

/* an item drawn lit or dark through its field's picture; the screen stays locked */
static void draw_item(const Menu *m, uint16_t i, int lit, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    if (i > CANCEL) return;
    blit(m, lit ? LIT[i] : DARK[i], o);
    if (i <= SHIFT) blit(m, (lit ? DEVICE_LIT : DEVICE_DARK)[i][st->controls[i]], o);
    else if (i <= HORN) blit(m, (lit ? KEY_LIT : KEY_DARK)[i - CAMERA], o);
    if (i <= HORN) field(m, i, m->text[i], st, o);
}

static void redraw_items(Menu *m, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    m->redraw = 1;
    o->select(o->ctx, *st->screen);
    o->sound(o->ctx, MOVED);
    o->lock(o->ctx);
    o->text_select(o->ctx, 0);
    draw_item(m, m->previous, 0, st, o);
    o->text_select(o->ctx, 1);
    draw_item(m, (uint16_t)m->selected, 1, st, o);
    m->changed = 0;
}

/* keys that clash: "conf" shown 200 ticks, then "conb"; 1 to ask again */
static int clashed(const Menu *m, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    if ((int16_t)o->clash(o->ctx) == 0) return 0;
    o->select(o->ctx, *st->screen);
    blit(m, "conf", o);
    show(st, o);
    o->deadline_set(o->ctx, CLASH_WAIT);
    while (!o->deadline_passed(o->ctx)) {
        o->music(o->ctx);
        if (*st->pointer_active) o->pointer_poll(o->ctx);
    }
    o->deadline_set(o->ctx, WAIT);
    o->select(o->ctx, *st->screen);
    blit(m, "conb", o);
    show(st, o);
    return 1;
}

/* a key asked for into one of the controls (left as it was for none), and named */
static void ask(int at, char *name, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    const int32_t k = o->key_capture(o->ctx, name);
    if ((int16_t)k == 0) return;
    st->controls[at] = (int16_t)k;
    const int key = st->plus != NULL ? port_controls_key_of_word(at) : -1;
    if (key >= 0 && st->plus->port->keys_custom) {   /* port: the player's own keys follow what this screen was asked */
        static const int none[PKEY_COUNT];
        port_controls_set_key(st->plus->port, none, (PortKey)key, key_code((uint16_t)key_layout(k, *st->german)));
    }
}

/* the keyboard's two keys for a device item, asked for until they clash no more */
static void ask_pair(Menu *m, int i, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    const int16_t y = FIELD_Y[i];
    o->text_select(o->ctx, 1);
    blit(m, DEVICE_LIT[i][KEYBOARD], o);
    int again;
    do {
        field(m, i, NULL, st, o);
        blit(m, PROMPT[i][0], o);
        o->text_boxed(o->ctx, "_", FIELD_X, y);
        ask(PAIR[i][0], m->first, st, o);
        o->select(o->ctx, *st->screen);
        key_label(PAIR[i][0], m->first, st, o);
        blit(m, PROMPT[i][1], o);
        snprintf(m->text[i], TEXT, "%s/_", m->first);
        o->text_boxed(o->ctx, m->text[i], FIELD_X, y);
        ask(PAIR[i][1], m->second, st, o);
        o->select(o->ctx, *st->screen);
        key_label(PAIR[i][1], m->second, st, o);
        snprintf(m->text[i], TEXT, "%s/%s", m->first, m->second);
        o->text_boxed(o->ctx, m->text[i], FIELD_X, y);
        again = clashed(m, st, o);
        o->pointer_mark(o->ctx);
    } while (again);
}

/* a key item's key, asked for until it clashes no more */
static void ask_key(Menu *m, int i, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    const int at = KEY[i - CAMERA];
    int again;
    do {
        field(m, i, NULL, st, o);
        blit(m, KEY_PROMPT, o);
        o->text_boxed(o->ctx, "_", FIELD_X, FIELD_Y[i]);
        ask(at, m->first, st, o);
        key_label(at, m->first, st, o);
        snprintf(m->text[i], TEXT, "%s", m->first);  /* sprintf(text, name): no key's name but '%''s holds a '%' */
        o->text_boxed(o->ctx, m->text[i], FIELD_X, FIELD_Y[i]);
        again = clashed(m, st, o);
        o->pointer_mark(o->ctx);
    } while (again);
}

/* a device back to the keyboard: 1 when the controls clash then */
static int to_keyboard(int i, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    st->controls[i] = KEYBOARD;
    return (int16_t)o->clash(o->ctx) != 0;
}

/* the next device offered for steering: the keyboard, the first joystick's (1), the second's (4) */
static int next_steering(const ControlsMenuState *st, const ControlsMenuOps *o)
{
    int16_t *c = st->controls;
    switch ((uint16_t)c[STEERING]) {
    case 0:
        if (*st->joystick_a) c[STEERING] = 1;
        else if (*st->joystick_b) c[STEERING] = 4;
        else return to_keyboard(STEERING, st, o);
        return 0;
    case 1:
        if (*st->joystick_b) c[STEERING] = 4;
        else return to_keyboard(STEERING, st, o);
        return 0;
    case 4:
        return to_keyboard(STEERING, st, o);
    default:
        return 0;
    }
}

/* throttle / brake: the keyboard, the first joystick's 2 and 3, the second's 5 (not 3 while shifting is on 3, not
 * 5 while shifting is on 5) */
static int next_throttle(const ControlsMenuState *st, const ControlsMenuOps *o)
{
    int16_t *c = st->controls;
    switch ((uint16_t)c[THROTTLE]) {
    case 0:
        if (*st->joystick_a) c[THROTTLE] = 2;
        else if (c[SHIFT] == 5) return 0;
        else if (*st->joystick_b) c[THROTTLE] = 5;
        else return to_keyboard(THROTTLE, st, o);
        return 0;
    case 2:
        if (c[SHIFT] == 3 || !*st->joystick_a) {
            if (*st->joystick_b) c[THROTTLE] = 5;
            else return to_keyboard(THROTTLE, st, o);
        } else {
            c[THROTTLE] = 3;
        }
        return 0;
    case 3:
        if (c[SHIFT] != 5 && *st->joystick_b) c[THROTTLE] = 5;
        else return to_keyboard(THROTTLE, st, o);
        return 0;
    case 5:
        return to_keyboard(THROTTLE, st, o);
    default:
        return 0;
    }
}

/* shifting: the keyboard, the first joystick's 3, the second's 5 (not where throttle / brake is) */
static int next_shift(const ControlsMenuState *st, const ControlsMenuOps *o)
{
    int16_t *c = st->controls;
    switch ((uint16_t)c[SHIFT]) {
    case 0:
        if (c[THROTTLE] == 3) {
            if (*st->joystick_b) c[SHIFT] = 5;
            else return to_keyboard(SHIFT, st, o);
        } else if (*st->joystick_a) {
            c[SHIFT] = 3;
        } else if (c[THROTTLE] == 5) {
            return 0;
        } else if (*st->joystick_b) {
            c[SHIFT] = 5;
        } else {
            return to_keyboard(SHIFT, st, o);
        }
        return 0;
    case 3:
        if (c[THROTTLE] != 5 && *st->joystick_b) c[SHIFT] = 5;
        else return to_keyboard(SHIFT, st, o);
        return 0;
    case 5:
        return to_keyboard(SHIFT, st, o);
    default:
        return 0;
    }
}

/* a device item chosen: the next device (not with the pointer on the fields, nor with Enter), then the keyboard's
 * keys asked for when it is on the keyboard and the pointer is on the fields, Enter was pressed or the keys clash */
static void choose_device(Menu *m, int i, int over, uint16_t k, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    int ask_keys = over;
    if (!over && k != ENTER) {
        if (i == STEERING) ask_keys = next_steering(st, o);
        else if (i == THROTTLE) ask_keys = next_throttle(st, o);
        else ask_keys = next_shift(st, o);
    }
    o->deadline_set(o->ctx, WAIT);
    if (st->controls[i] == KEYBOARD && (ask_keys || k == ENTER)) ask_pair(m, i, st, o);
    m->redraw = 1;
}

/* Calibrate: 0043bc00, then the screen's inverse table and palette again, all drawn and faded in */
static void calibrate(Menu *m, int32_t mode, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    o->fade_out(o->ctx, 0, ALL);
    o->free_inverse(o->ctx);
    o->calibrate(o->ctx, 1);
    if (*st->pointer_active == 0) move_to(m->selected, o);
    *st->inverse = o->load_inverse(o->ctx, (int16_t)mode ? "check/chconset.inv" : "option/cntrlset.inv");
    o->flush(o->ctx);
    palette(m, o);
    o->text_styles(o->ctx);
    o->palette_put(o->ctx, 0, ALL, m->rgb);
    m->fade = m->redraw = m->changed = 1;
}

static void put_back(Menu *m, const ControlsMenuState *st)
{
    memcpy(st->controls, m->kept, sizeof m->kept);
    if (st->plus) *st->plus->port = m->kept_port;
    m->cancelled = 1;
    m->leave = 1;
}

/* port: the added pages; Done leaves the screen, Cancel puts everything back, back returns to page 1 drawn again */
static void second_page(Menu *m, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    const ControlsPageResult r = controls_plus_run(st, o, m->bank);
    if (r == CONTROLS_PAGE_CANCEL) put_back(m, st);
    else if (r == CONTROLS_PAGE_DONE) m->leave = 1;
    /* back to page 1: its palette fades in again, since the pages put the clean background's on the display */
    if (r == CONTROLS_PAGE_BACK) m->fade = 1;
    m->redraw = 1;
    m->released = m->armed = 0;
    m->rest = 0;
    if (*st->pointer_active == 0) move_to(m->selected, o);
}

static void choose(Menu *m, int32_t mode, int over, uint16_t k, const ControlsMenuState *st,
                   const ControlsMenuOps *o)
{
    switch ((uint16_t)m->selected) {
    case STEERING:
    case THROTTLE:
    case SHIFT:
        choose_device(m, m->selected, over, k, st, o);
        break;
    case CAMERA:
    case HANDBRAKE:
    case HORN:
        o->text_select(o->ctx, 1);
        if (over || k == ENTER) {
            ask_key(m, m->selected, st, o);
            m->redraw = 1;
        }
        break;
    case CALIBRATE:
        calibrate(m, mode, st, o);
        break;
    case DONE:
        m->leave = 1;
        break;
    case CANCEL:
        put_back(m, st);
        break;
    default:
        break;
    }
}

/* Up: back one, Cancel to Calibrate (to Shift when Calibrate is not offered), Steering to Done */
static void step_up(Menu *m, const ControlsMenuState *st)
{
    if (m->selected == CANCEL) m->selected = DONE;
    m->selected--;
    if (*st->calibration == 0 && m->selected == CALIBRATE) m->selected--;
    if (m->selected < 0) m->selected = DONE;
}

/* Down: on one, Done and Cancel to Steering (Cancel by its own test), over Calibrate when not offered */
static void step_down(Menu *m, const ControlsMenuState *st)
{
    const int16_t was = m->selected;
    m->selected = (int16_t)((m->selected + 1) % 8);
    if (was == CANCEL) m->selected = STEERING;
    if (*st->calibration == 0 && m->selected == CALIBRATE) m->selected++;
}

/* a key other than the choosing ones: Escape puts back and leaves, the arrows step */
static void step(Menu *m, uint16_t k, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    if (k == ESCAPE) {
        put_back(m, st);
        o->sound(o->ctx, CHOSEN);
    } else if (st->plus && k == RIGHT && m->selected < DONE) {
        second_page(m, st, o);
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        m->previous = (uint16_t)m->selected;
        if (k == UP) step_up(m, st);
        else step_down(m, st);
        m->changed = 1;
        move_to(m->selected, o);
    } else if ((k == LEFT || k == RIGHT) && (m->selected == DONE || m->selected == CANCEL)) {
        const int16_t to = m->selected == DONE ? CANCEL : DONE;
        o->pointer_off(o->ctx);
        m->previous = (uint16_t)m->selected;
        m->selected = to;
        move_to(to, o);
        m->changed = 1;
    }
    m->armed = 0;
    m->rest = 0;
    o->ticks(o->ctx);
}

static void open_screen(Menu *m, int32_t mode, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    memcpy(m->kept, st->controls, sizeof m->kept);
    if (st->plus) m->kept_port = *st->plus->port;
    m->names = DEVICE_NAME[*st->german != 0];
    for (int i = 0; i < FIELDS; i++) m->pictures[i] = o->picture_new(o->ctx, PICTURE_W, PICTURE_H, 0);
    m->scratch = o->picture_new(o->ctx, PICTURE_W, PICTURE_H, 0);
    o->flush(o->ctx);
    o->palette_clear(o->ctx, m->black);
    move_to(m->selected, o);
    m->bank = o->load_art(o->ctx, (int16_t)mode ? "check/chconset.qfs" : "option/cntrlset.qfs");
    *st->inverse = o->load_inverse(o->ctx, (int16_t)mode ? "check/chconset.inv" : "option/cntrlset.inv");
    o->music_feed(o->ctx);
    palette(m, o);
    o->fade_in(o->ctx, 0, ALL, m->rgb);
    o->text_styles(o->ctx);
    if (*st->pointer_active) o->pointer_poll(o->ctx);
    o->pointer_mark(o->ctx);
}

static void close_screen(Menu *m, const ControlsMenuOps *o)
{
    o->fade_out(o->ctx, 0, ALL);
    o->music_feed(o->ctx);
    for (int i = 0; i < FIELDS; i++) o->picture_free(o->ctx, m->pictures[i]);
    o->picture_free(o->ctx, m->scratch);
    o->free(o->ctx, m->bank);
    o->free_inverse(o->ctx);
    o->pointer_mark(o->ctx);
}

/* Space, Enter or the left button: the item chosen when the keys are taken, then the keys drained */
static void chosen(Menu *m, int32_t mode, int over, uint16_t k, const ControlsMenuState *st,
                   const ControlsMenuOps *o)
{
    o->pointer_click(o->ctx);
    o->select(o->ctx, *st->screen);
    if (m->armed) {
        o->sound(o->ctx, CHOSEN);
        choose(m, mode, over, k, st, o);
        o->deadline_set(o->ctx, WAIT);
        while (o->key(o->ctx) != 0) o->music(o->ctx);
        if (*st->pointer_active == 0) move_to(m->selected, o);
    }
    m->released = 0;
    m->armed = 0;
    m->rest = 0;
    o->ticks(o->ctx);
}

/* 00413cb0 */
int32_t controls_menu(const ControlsMenuState *st, int32_t mode, const ControlsMenuOps *ops)
{
    static Menu zero;
    Menu m = zero;
    m.fade = m.redraw = m.armed = m.released = 1;
    open_screen(&m, mode, st, ops);
    do {
        const int over = follow(&m, st, ops);
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
        if (st->plus && *st->buttons == LEFT_BUTTON && m.released && controls_plus_nav_hit(ops)) {
            second_page(&m, st, ops);
        } else if (k == SPACE || k == ENTER || (*st->buttons == LEFT_BUTTON && m.released)) {
            chosen(&m, mode, over, k, st, ops);
        } else if (k == 0) {
            m.armed = 1;
        } else if (m.armed) {
            step(&m, k, st, ops);
        }
        ops->music(ops->ctx);
        if (m.fade) continue;
        if (*st->net_off == 0) ops->net_sync(ops->ctx);
        if (ops->deadline_passed(ops->ctx) || (*st->buttons == RIGHT_BUTTON && m.released) || *st->quit == 1) {
            ops->sound(ops->ctx, CHOSEN);
            m.leave = 1;
        }
    } while (!m.leave);
    close_screen(&m, ops);
    if (st->plus && !m.cancelled && memcmp(st->plus->port, &m.kept_port, sizeof m.kept_port) != 0)
        ops->plus_save(ops->ctx, st->plus->port);   /* port */
    return m.leave;
}
