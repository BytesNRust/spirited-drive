#include "game/frontend/game_mode.h"

#include <stdio.h>
#include <string.h>
#include "game/sys/port_log.h"

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, DOWN = 0x5000, LEFT = 0x4b00, RIGHT = 0x4d00, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
/* port (dep59 step 4, L2b): the rows, in the order Up / Down walk them: Single Player and Multiplayer (the exe's Network) on top, the
 * selector of the modes and editions, "Manage content...", Exit at the bottom right; all drawn in the game's text over the art's words */
enum { SINGLE, NETWORK, CHOICE, MANAGE, EXIT, ROWS };
enum { RACE = 0, LEAVE = 1, TIMED_OUT = 2 };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x1194, REST = 0x19, TOLD = 0x12c, WIDE = 0x280, ALL = 0x100 };

/* a row: the rect the pointer selects it in (00463f80's x0 .. x1, y0 .. y1, x1 left out) and where Up / Down put the pointer */
typedef struct {
    int16_t x0, x1, y0, y1, cx, cy;
} Row;
/* port (dep59 step 4, the owner's L2b): Single Player and Multiplayer centred as a pair, 52 pixels apart, in the space between the title
 * band (y 88) and the selector's title (y 303), an empty gap under them (no row: a click there does nothing), the selector and Manage
 * content low, Exit at the foot; the text centres at y 170, 222, 318 (the selector's title; its lines 340 and 358) and 383 */
static const Row ROW[ROWS] = {
    { 0, 0x280, 144, 196, 0x140, 170 },      /* Single Player */
    { 0, 0x280, 197, 248, 0x140, 222 },      /* Multiplayer */
    { 0, 0x280, 296, 365, 0x140, 333 },      /* the selector: its title, its lines */
    { 160, 480, 366, 400, 0x140, 383 },      /* Manage content...: its small words */
    { 500, 0x280, 410, 0x1e0, 580, 449 }     /* Exit: the bottom right, clear of the foot line */
};
/* 0043213c: the art's four words, covered by the port's rows */
static const int16_t ART_Y[4] = { 0x91, 0xda, 0x122, 0x16f };
/* the selector's thirds for the pointer: left of CHOICE_LEFT the previous choice, from CHOICE_RIGHT on the next, between them its name */
enum { CHOICE_LEFT = 0xd5, CHOICE_RIGHT = 0x1ab };
enum { LABEL_X = 311, TEXT_HALF_HEIGHT = 21, EXIT_RIGHT = 640 - 14, EXIT_TOP = 428, SMALL_NUM = 3, SMALL_DEN = 5 };   /* LABEL_X: where the art's words centre, not the pointer's 0x140 */
static const char MANAGE_WORDS[] = "Manage content...";
static const char MANAGE_LATER[] = "Editions are managed in a later version";
enum { TEXT_DIM_STYLE = 4 };   /* text.h's TEXT_DIM: what the mode does not let the player use */

enum { FOOTER_CAP = 80, FOOTER_LINES = 2, FOOTER_X = 14, FOOTER_Y = 448, FOOTER_HEIGHT = 20, FOOTER_NUM = 3, FOOTER_DEN = 5 };   /* the small subtitle size (3/5) of the profile's lines, 14 pixels in from the left and the bottom */
/* port (dep59 step 4, the owner's): the update notice on the version's line, to its right after this much space at the line's scale */
static const char FOOTER_GAP[] = "  ";

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
    char footer[FOOTER_LINES][FOOTER_CAP];   /* port: the foot texts as last drawn: 0 the version, 1 the update notice */
    int notice_hot;    /* port: the notice was drawn lit, the pointer on it */
    int in_use, shown, count;   /* port (dep59 step 4): the selector's choice in use, the one it shows (not yet applied when they differ), how many */
    uint32_t pending_at;   /* port: the clock's ms at the last arrow, the choice applies once CHOICE_IDLE_MS pass without another */
    int quick;         /* port: the fade in after a mode's change of picture is the short one */
    uint8_t rgb[GAME_MODE_PALETTE];
} Menu;

static void palette_load(Menu *m, const GameModeOps *o)
{
    const uint8_t *pal = o->shape(o->ctx, m->bank, "!pal");
    o->palette_set(o->ctx, m->rgb, 0, ALL, pal + 0x10, (int16_t)pal[0]);
}

static void move_to(const Menu *m, const GameModeOps *o)
{
    o->pointer_move_to(o->ctx, ROW[m->selected].cx, ROW[m->selected].cy);
}

static int on_row(const GameModeOps *o, int16_t i)
{
    return o->pointer_in_rect(o->ctx, ROW[i].x0, ROW[i].x1, ROW[i].y0, ROW[i].y1) != 0;
}

/* 1 when the pointer is on a row (a click elsewhere, the foot line or the art's margins, chooses nothing) */
static int on_any_row(const GameModeOps *o)
{
    for (int16_t i = 0; i < ROWS; i++)
        if (on_row(o, i)) return 1;
    return 0;
}

/* the first row the pointer is in becomes the selection */
static void follow(Menu *m, const GameModeOps *o)
{
    for (int16_t i = 0; i < ROWS; i++) {
        if (!on_row(o, i)) continue;
        if (m->selected != i) {
            m->previous = m->selected;
            m->selected = i;
            m->changed = 1;
        }
        return;
    }
}

/* the art's four words put back to the clean background */
static void cover_art(const GameModeOps *o)
{
    enum { COVER_X = 96, COVER_W = 448, COVER_HALF = 36 };
    if (o->extras_cover == NULL) return;
    for (int i = 0; i < 4; i++) o->extras_cover(o->ctx, COVER_X, ART_Y[i] - COVER_HALF, COVER_W, COVER_HALF * 2);
}

/* 1 when the mode in use is Restomod's (its standard edition, 2, or an edition after it): Manage content is open there */
static int restomod_in_use(const Menu *m) { return m->in_use >= 2; }

/* "<  name  >" (lit while it is the selection) and the lines about the choice it shows under it */
static void choice_row(const Menu *m, const GameModeOps *o)
{
    const int cy = ROW[CHOICE].cy, title_y = cy - 35, line1_y = cy + 1, line2_y = cy + 19;
    const char *text[3] = { "", "", "" };
    if (o->choice == NULL || o->plus_text_scaled == NULL || o->plus_text_width_scaled == NULL) return;
    if (o->choice(o->ctx, m->shown != m->in_use ? GAME_MODE_CHOICE_PEEK : GAME_MODE_CHOICE_READ, m->shown, text) < 0) return;
    char title[96];
    snprintf(title, sizeof title, "<  %s  >", text[0]);
    o->plus_text(o->ctx, m->selected == CHOICE, title, LABEL_X - o->plus_text_width(o->ctx, title) / 2, title_y);
    if (text[1][0] != 0) o->plus_text_scaled(o->ctx, 0, text[1], LABEL_X - o->plus_text_width_scaled(o->ctx, text[1], SMALL_NUM, SMALL_DEN) / 2, line1_y, SMALL_NUM, SMALL_DEN);
    if (text[2][0] != 0) o->plus_text_scaled(o->ctx, 0, text[2], LABEL_X - o->plus_text_width_scaled(o->ctx, text[2], SMALL_NUM, SMALL_DEN) / 2, line2_y, SMALL_NUM, SMALL_DEN);
}

/* "Manage content...", small under the selector: lit while it is the selection, dimmed where the mode has no editions */
static void manage_row(const Menu *m, const GameModeOps *o)
{
    if (o->choice == NULL || o->plus_text_scaled == NULL || o->plus_text_width_scaled == NULL) return;
    const int32_t style = !restomod_in_use(m) ? TEXT_DIM_STYLE : m->selected == MANAGE;
    o->plus_text_scaled(o->ctx, style, MANAGE_WORDS, LABEL_X - o->plus_text_width_scaled(o->ctx, MANAGE_WORDS, SMALL_NUM, SMALL_DEN) / 2,
                        ROW[MANAGE].cy - 6, SMALL_NUM, SMALL_DEN);
}

/* port (dep59 step 4): why Multiplayer is closed, NULL when open */
static const char *network_closed(const GameModeOps *o) { return o->network_closed != NULL ? o->network_closed(o->ctx) : NULL; }

/* the rows over the clean background; Multiplayer dimmed while it is closed */
static void rows_draw(const Menu *m, const GameModeOps *o)
{
    static const char *const WORDS[2] = { "Single Player", "Multiplayer" };
    cover_art(o);
    for (int16_t i = SINGLE; i <= NETWORK; i++) {
        const int32_t style = i == NETWORK && network_closed(o) != NULL ? TEXT_DIM_STYLE : m->selected == i;
        o->plus_text(o->ctx, style, WORDS[i], LABEL_X - o->plus_text_width(o->ctx, WORDS[i]) / 2, ROW[i].cy - TEXT_HALF_HEIGHT);
    }
    choice_row(m, o);
    manage_row(m, o);
    o->plus_text(o->ctx, m->selected == EXIT, "Exit", EXIT_RIGHT - o->plus_text_width(o->ctx, "Exit"), EXIT_TOP);
}

static int32_t footer_width(const GameModeOps *o, const char *text)
{
    return o->plus_text_width_scaled != NULL ? o->plus_text_width_scaled(o->ctx, text, FOOTER_NUM, FOOTER_DEN) : o->plus_text_width != NULL ? o->plus_text_width(o->ctx, text) : 0;
}

/* port: where foot text i goes and how wide it is: the version at the line's start, the notice after it and FOOTER_GAP (at the start
 * when there is no version) */
static void footer_place(const Menu *m, const GameModeOps *o, int32_t i, int32_t *x, int32_t *y, int32_t *w)
{
    *w = footer_width(o, m->footer[i]);
    *x = FOOTER_X, *y = FOOTER_Y;
    if (i == 1 && m->footer[0][0] != 0) *x += footer_width(o, m->footer[0]) + footer_width(o, FOOTER_GAP);
}

/* port: the foot line at the bottom left in the plain style: the version, then the notice, lit while the pointer is on it */
static void footer_draw(Menu *m, const GameModeOps *o)
{
    if (o->footer == NULL || o->plus_text == NULL) return;
    for (int32_t i = 0; i < FOOTER_LINES; i++)
        if (!o->footer(o->ctx, i, m->footer[i], sizeof m->footer[i])) m->footer[i][0] = 0;
    for (int32_t i = 0; i < FOOTER_LINES; i++) {
        if (m->footer[i][0] == 0) continue;
        int32_t x, y, w;
        footer_place(m, o, i, &x, &y, &w);
        const int32_t lit = i == 1 && m->notice_hot;
        if (o->plus_text_scaled != NULL) o->plus_text_scaled(o->ctx, lit, m->footer[i], x, y, FOOTER_NUM, FOOTER_DEN);
        else o->plus_text(o->ctx, lit, m->footer[i], x, y);
    }
}

/* port: 1 when a foot line is not the one drawn */
static int footer_stale(const Menu *m, const GameModeOps *o)
{
    if (o->footer == NULL) return 0;
    for (int32_t i = 0; i < FOOTER_LINES; i++) {
        char now[FOOTER_CAP] = "";
        if (!o->footer(o->ctx, i, now, sizeof now)) now[0] = 0;
        if (strcmp(now, m->footer[i]) != 0) return 1;
    }
    return 0;
}

/* port: 1 when the notice is drawn and the pointer is on its own text (x .. x + w, the line's height); the version is not part of it */
static int on_notice(const Menu *m, const GameModeOps *o)
{
    if (o->open_notice == NULL || o->footer == NULL || m->footer[1][0] == 0) return 0;
    int32_t x, y, w;
    footer_place(m, o, 1, &x, &y, &w);
    return w > 0 && o->pointer_in_rect(o->ctx, x, x + w, y, y + FOOTER_HEIGHT) != 0;
}

static void draw(Menu *m, GameModeState *st, int german, const GameModeOps *o)
{
    o->select(o->ctx, st->screen);
    o->blit_own(o->ctx, o->shape(o->ctx, m->bank, "bgnd"));
    rows_draw(m, o);
    footer_draw(m, o);
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
        if (m->quick && o->fade_in_quick != NULL) o->fade_in_quick(o->ctx, m->rgb);
        else o->fade_in(o->ctx, 0, ALL, m->rgb);
        m->quick = 0;
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

static void settle_choice(Menu *m, const GameModeOps *o, int quiet);

/* the selection moved: its sound, and the page drawn again; leaving the selector applies what it shows, or (a choice that cannot be
 * used) shows the one in use again */
static void redraw_options(Menu *m, const GameModeOps *o)
{
    if (m->previous != m->selected) o->sound(o->ctx, MOVED);
    if (m->previous == CHOICE && m->selected != CHOICE) {
        settle_choice(m, o, 0);
        m->shown = m->in_use;
    }
    m->redraw = 1;
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

static void choice_read(Menu *m, const GameModeOps *o);

/* port (dep59 step 4b, T7): the CONTENT screen on the choice in use; the choices read again (editions made or deleted there), the one it
 * closed on applied (the edition just edited is the one in use), and Game Mode drawn and faded in again as after the EXTRAS screen */
static void manage(Menu *m, const GameModeOps *o)
{
    const int32_t next = o->manage(o->ctx, m->in_use);
    choice_read(m, o);
    if (next >= 0 && next < m->count && next != m->in_use) {
        m->shown = next;
        settle_choice(m, o, 1);
    }
    o->deadline_set(o->ctx, WAIT);
    palette_load(m, o);
    m->redraw = 1;
    m->fade = 1;
    move_to(m, o);
}

/* port (dep59 step 4): the selector's state from the choice op: how many, the one in use, shown */
static void choice_read(Menu *m, const GameModeOps *o)
{
    const char *text[3];
    if (o->choice == NULL) return;
    m->count = o->choice(o->ctx, GAME_MODE_CHOICE_COUNT, 0, text);
    m->in_use = m->shown = o->choice(o->ctx, GAME_MODE_CHOICE_READ, 0, text);
}

/* port (modes, editions): the choice the selector shows becomes the one in use; nothing is lost, each mode keeps its own. One that cannot
 * be used is left shown, not applied. A change of the image's format or of its internal resolution is covered by a short fade (`quiet`:
 * not, the screen is closing) */
static void settle_choice(Menu *m, const GameModeOps *o, int quiet)
{
    const char *text[3];
    if (o->choice == NULL || m->shown == m->in_use || o->choice(o->ctx, GAME_MODE_CHOICE_PEEK, m->shown, text) != 1) return;
    const int fades = !quiet && o->fade_out_quick != NULL && o->fade_in_quick != NULL && o->choice(o->ctx, GAME_MODE_CHOICE_LOOK, m->shown, text) == 1;
    if (fades) o->fade_out_quick(o->ctx);
    o->choice(o->ctx, GAME_MODE_CHOICE_APPLY, m->shown, text);
    choice_read(m, o);
    m->redraw = 1;
    if (fades) m->fade = m->quick = 1;
}

/* port: the selector shows the next (+1) or previous (-1) choice round them all, at once; it applies once the arrows rest (no clock: at
 * once). Coming round to the one in use shows it as it is and applies nothing */
static void cycle_choice(Menu *m, const GameModeOps *o, int step)
{
    const char *text[3];
    if (o->choice == NULL || m->count <= 0) return;
    m->shown = ((m->shown + step) % m->count + m->count) % m->count;
    o->choice(o->ctx, GAME_MODE_CHOICE_PEEK, m->shown, text);
    m->redraw = 1;
    if (o->clock_ms == NULL) settle_choice(m, o, 0);
    else m->pending_at = o->clock_ms(o->ctx);
}

/* port: the arrows have rested: the choice on the selector applies */
static void choice_rests(Menu *m, const GameModeOps *o)
{
    enum { CHOICE_IDLE_MS = 400 };
    if (m->shown != m->in_use && o->clock_ms != NULL && o->clock_ms(o->ctx) - m->pending_at >= CHOICE_IDLE_MS) {
        m->pending_at = o->clock_ms(o->ctx);   /* one that cannot be used is asked again only after another rest */
        settle_choice(m, o, 0);
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
    case NETWORK:
        if (network_closed(o) != NULL) {   /* port (dep59 step 4): an edition of the player's own: why, and nothing opened */
            port_log("mark:game_mode=network_closed\n");
            o->banner_wait(o->ctx, network_closed(o), TOLD);
            o->deadline_set(o->ctx, WAIT);   /* the banner waited the shared deadline out: else the screen times out to the demo at once */
            m->redraw = 1;
            break;
        }
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
    case CHOICE:   /* port: a click on the row's left third shows the previous choice, on its right third the next; Return or a click on the name applies it */
        if (st->buttons == LEFT_BUTTON && o->pointer_in_rect(o->ctx, 0, CHOICE_LEFT, ROW[CHOICE].y0, ROW[CHOICE].y1)) cycle_choice(m, o, -1);
        else if (st->buttons == LEFT_BUTTON && o->pointer_in_rect(o->ctx, CHOICE_RIGHT, WIDE, ROW[CHOICE].y0, ROW[CHOICE].y1)) cycle_choice(m, o, 1);
        else settle_choice(m, o, 0);
        break;
    case MANAGE:   /* port (dep59 step 4b): the CONTENT screen (with no manage op, a later version's banner); in Stock and Restored, nothing */
        if (!restomod_in_use(m)) break;
        port_log("mark:game_mode=manage\n");
        if (o->manage != NULL) {
            manage(m, o);
            break;
        }
        o->banner_wait(o->ctx, MANAGE_LATER, TOLD);
        o->deadline_set(o->ctx, WAIT);   /* as Multiplayer's banner */
        m->redraw = 1;
        break;
    }
}

/* a key other than the choosing ones: Escape asks, the arrows step */
static void step(Menu *m, int16_t k, int german, const GameModeOps *o)
{
    if (k == ESCAPE) {
        o->sound(o->ctx, CHOSEN);
        ask_exit(m, german, o);
    } else if ((k == LEFT || k == RIGHT) && m->selected == CHOICE && o->choice != NULL) {   /* port: the selector's arrows */
        cycle_choice(m, o, k == LEFT ? -1 : 1);
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        m->previous = m->selected;
        m->selected = k == UP ? (int16_t)(m->selected - 1) : (int16_t)((m->selected + 1) % ROWS);
        if (k == UP && m->selected < 0) m->selected = (int16_t)(ROWS - 1);
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
    port_log("mark:screen=game_mode\n");   /* port: the headless walk (tools/menu_walk.sh) waits for it */
    Menu m = { 0 };
    m.redraw = m.fade = m.armed = m.released = 1;
    m.selected = SINGLE;
    choice_read(&m, ops);
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
        choice_rests(&m, ops);
        if (m.redraw) draw(&m, st, german, ops);
        if (m.changed) redraw_options(&m, ops);
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
            if (m.armed && st->buttons == LEFT_BUTTON && on_notice(&m, ops)) {
                ops->sound(ops->ctx, CHOSEN);
                ops->open_notice(ops->ctx);
            } else if (m.armed && (st->buttons != LEFT_BUTTON || on_any_row(ops))) {   /* port: a click off every row chooses nothing */
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
        if (footer_stale(&m, ops)) m.redraw = 1;   /* port: the update check answered */
        if (on_notice(&m, ops) != m.notice_hot) m.notice_hot = !m.notice_hot, m.redraw = 1;   /* port: the notice lights under the pointer */
        if (m.fade || m.done) continue;
        if (ops->deadline_passed(ops->ctx)) {
            ops->sound(ops->ctx, CHOSEN);
            settle_choice(&m, ops, 1);   /* port: the screen is closing, the choice on the selector still applies */
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
