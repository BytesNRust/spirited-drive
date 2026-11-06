#include "game/frontend/game_mode.h"

#include <stdio.h>
#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, DOWN = 0x5000, LEFT = 0x4b00, RIGHT = 0x4d00, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
/* port (modes): the Profile row where the exe's Single Player stands, then Single Player, Multiplayer (the exe's Network) and Exit; all four drawn in the game's text, "Multiplayer" has no art */
enum { PROFILE, SINGLE, NETWORK, EXIT, OPTIONS };
enum { RACE = 0, LEAVE = 1, TIMED_OUT = 2 };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x1194, REST = 0x19, TOLD = 0x12c, WIDE = 0x280, ALL = 0x100 };

/* 00432134 / 0043213c: the options' centres, the art's four places */
static const int16_t CENTRE_X[OPTIONS] = { 0x140, 0x140, 0x140, 0x140 };
static const int16_t CENTRE_Y[OPTIONS] = { 0x91, 0xda, 0x122, 0x16f };
/* the bands 00463f80 is asked about, in turn */
static const int16_t BAND_TOP[OPTIONS] = { 0, 0xb2, 0xfe, 0x14a };
static const int16_t BAND_BOTTOM[OPTIONS] = { 0xb1, 0xfd, 0x149, 0x1e0 };
/* the row's thirds for the pointer: left of PROFILE_LEFT the previous mode, from PROFILE_RIGHT on the next, between them its name */
enum { PROFILE_LEFT = 0xd5, PROFILE_RIGHT = 0x1ab };
static const char *const WORDS[OPTIONS] = { NULL, "Single Player", "Multiplayer", "Exit" };

enum { FOOTER_CAP = 80, FOOTER_LINES = 2, FOOTER_X = 14, FOOTER_Y = 448, FOOTER_STEP = 20, FOOTER_NUM = 3, FOOTER_DEN = 5 };   /* the small subtitle size (3/5) of the profile's lines, 14 pixels in from the left and the bottom */

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
    char footer[FOOTER_LINES][FOOTER_CAP];   /* port: the foot lines as last drawn */
    int pending;       /* port (profiles): how many steps along the cycle the row shows, not yet applied (0: the profile in use) */
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

/* the row's second line while it shows a mode not yet applied */
static const char PENDING_LINE[] = "Return applies it.";

/* the art's words put back to the clean background over a row's place */
static void cover_row(const GameModeOps *o, int16_t row)
{
    enum { COVER_X = 96, COVER_W = 448, COVER_HALF = 36 };
    if (o->extras_cover != NULL) o->extras_cover(o->ctx, COVER_X, CENTRE_Y[row] - COVER_HALF, COVER_W, COVER_HALF * 2);
}

/* "<  name  >" (lit while it is the selection) and the two lines about the mode under it */
static void profile_row(const Menu *m, const GameModeOps *o)
{
    enum { TITLE_Y = 110, LINE1_Y = 146, LINE2_Y = 164, CENTRE = 311, SMALL_NUM = 3, SMALL_DEN = 5 };
    const char *text[3] = { "", "", "" };
    if (o->profile == NULL || o->plus_text_scaled == NULL || o->plus_text_width_scaled == NULL ||
        !o->profile(o->ctx, m->pending != 0 ? GAME_MODE_PROFILE_PEEK + m->pending : 0, text))
        return;
    if (m->pending != 0) text[2] = PENDING_LINE;   /* not applied yet: say how */
    char title[96];
    snprintf(title, sizeof title, "<  %s  >", text[0]);
    o->plus_text(o->ctx, m->selected == PROFILE, title, CENTRE - o->plus_text_width(o->ctx, title) / 2, TITLE_Y);
    o->plus_text_scaled(o->ctx, 0, text[1], CENTRE - o->plus_text_width_scaled(o->ctx, text[1], SMALL_NUM, SMALL_DEN) / 2, LINE1_Y, SMALL_NUM, SMALL_DEN);
    o->plus_text_scaled(o->ctx, 0, text[2], CENTRE - o->plus_text_width_scaled(o->ctx, text[2], SMALL_NUM, SMALL_DEN) / 2, LINE2_Y, SMALL_NUM, SMALL_DEN);
}

/* the four rows over the clean background */
static void rows_draw(const Menu *m, const GameModeOps *o)
{
    enum { TEXT_HALF_HEIGHT = 21, LABEL_X = 311 };   /* LABEL_X: where the art's words centre, not the pointer's 0x140 */
    for (int16_t i = 0; i < OPTIONS; i++) cover_row(o, i);
    profile_row(m, o);
    for (int16_t i = SINGLE; i < OPTIONS; i++)
        o->plus_text(o->ctx, m->selected == i, WORDS[i], LABEL_X - o->plus_text_width(o->ctx, WORDS[i]) / 2, CENTRE_Y[i] - TEXT_HALF_HEIGHT);
}

/* port: where foot line i (its text) goes and how wide it is */
static void footer_place(const GameModeOps *o, int32_t i, const char *text, int32_t *x, int32_t *y, int32_t *w)
{
    *w = o->plus_text_width_scaled != NULL ? o->plus_text_width_scaled(o->ctx, text, FOOTER_NUM, FOOTER_DEN) : o->plus_text_width != NULL ? o->plus_text_width(o->ctx, text) : 0;
    *x = FOOTER_X, *y = FOOTER_Y - i * FOOTER_STEP;
}

/* port: the foot lines, in the plain style at the bottom left, the notice above the version */
static void footer_draw(Menu *m, const GameModeOps *o)
{
    if (o->footer == NULL || o->plus_text == NULL) return;
    for (int32_t i = 0; i < FOOTER_LINES; i++) {
        if (!o->footer(o->ctx, i, m->footer[i], sizeof m->footer[i])) m->footer[i][0] = 0;
        if (m->footer[i][0] == 0) continue;
        int32_t x, y, w;
        footer_place(o, i, m->footer[i], &x, &y, &w);
        if (o->plus_text_scaled != NULL) o->plus_text_scaled(o->ctx, 0, m->footer[i], x, y, FOOTER_NUM, FOOTER_DEN);
        else o->plus_text(o->ctx, 0, m->footer[i], x, y);
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

/* port: 1 when the notice is drawn and the pointer is on it (its band: the left 400 columns of its row) */
static int on_notice(const Menu *m, const GameModeOps *o)
{
    enum { NOTICE_WIDTH = 400, NOTICE_HEIGHT = 20 };
    if (o->open_notice == NULL || o->footer == NULL || m->footer[1][0] == 0) return 0;
    int32_t x, y, w;
    footer_place(o, 1, m->footer[1], &x, &y, &w);
    (void)x, (void)w;
    return o->pointer_in_rect(o->ctx, 0, NOTICE_WIDTH, y, y + NOTICE_HEIGHT) != 0;
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

/* the selection moved: its sound, and the page drawn again; leaving the Profile row forgets a mode not applied */
static void redraw_options(Menu *m, const GameModeOps *o)
{
    if (m->previous != m->selected) o->sound(o->ctx, MOVED);
    if (m->previous == PROFILE && m->selected != PROFILE) m->pending = 0;
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

/* port (profiles): the row shows the next (+1) or previous (-1) profile; nothing is changed until it is applied. Coming round to the one in use shows it as it is */
static void cycle_profile(Menu *m, const GameModeOps *o, int step)
{
    const char *text[3];
    if (o->profile == NULL) return;
    enum { REACH = 2 };   /* the other two modes are 1 and 2 steps away; one more is round to the one in use */
    const int want = m->pending + step >= -REACH && m->pending + step <= REACH ? m->pending + step : 0;
    m->pending = want != 0 && o->profile(o->ctx, GAME_MODE_PROFILE_PEEK + want, text) == 2 ? want : 0;
    m->redraw = 1;
}

/* port (modes): Return on the row (or a click on its name): the mode it shows; nothing is lost, each mode keeps its own */
static void apply_profile(Menu *m, const GameModeOps *o)
{
    const char *text[3];
    if (o->profile == NULL || m->pending == 0) return;
    o->profile(o->ctx, GAME_MODE_PROFILE_APPLY + m->pending, text);
    m->pending = 0;
    m->redraw = 1;
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
    case PROFILE:   /* port: a click on the row's left third shows the previous profile, on its right third the next; Return or a click on the name applies it */
        if (st->buttons == LEFT_BUTTON && o->pointer_in_rect(o->ctx, 0, PROFILE_LEFT, BAND_TOP[PROFILE], BAND_BOTTOM[PROFILE])) cycle_profile(m, o, -1);
        else if (st->buttons == LEFT_BUTTON && o->pointer_in_rect(o->ctx, PROFILE_RIGHT, WIDE, BAND_TOP[PROFILE], BAND_BOTTOM[PROFILE])) cycle_profile(m, o, 1);
        else apply_profile(m, o);
        break;
    }
}

/* a key other than the choosing ones: Escape asks, the arrows step */
static void step(Menu *m, int16_t k, int german, const GameModeOps *o)
{
    if (k == ESCAPE) {
        o->sound(o->ctx, CHOSEN);
        ask_exit(m, german, o);
    } else if ((k == LEFT || k == RIGHT) && m->selected == PROFILE && o->profile != NULL) {   /* port: the profile row's arrows */
        cycle_profile(m, o, k == LEFT ? -1 : 1);
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        m->previous = m->selected;
        m->selected = k == UP ? (int16_t)(m->selected - 1) : (int16_t)((m->selected + 1) % OPTIONS);
        if (k == UP && m->selected < 0) m->selected = (int16_t)(OPTIONS - 1);
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
    m.selected = SINGLE;   /* the Profile row is above it */
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
            } else if (m.armed) {
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
