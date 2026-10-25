/* port: see extras_menu.h. Two pages of switches drawn with the game's text over the clean configuration background,
 * grouped under the four headers the owner named. Up / Down walk a page's rows and its Done (Done rounds back to the
 * first row); Enter, Space, Left or Right change a switch (Unlock and Start step through their values); Left / Right
 * swap Done and Cancel, Right on Cancel turns to the next page and Left on Done back to the one before; a click on the
 * page indicator or PageDown turns to the next, round to the first. Mirror and Rally exclude each other, unless
 * ExtrasMenuOps.port_combine_variants lets them both stay on. Done saves; Cancel and Escape put the input back. */
#include "game/frontend/extras_menu.h"
#include "game/frontend/page_arrows.h"
#include "game/sys/port_log.h"

#include <stdio.h>

/* the screen's own name, in the words the player clicked to open it */
static const char *const TITLE = "Customize Game";   /* title case, as "Game Mode": the font makes the rest small capitals */

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       PAGE_DOWN = 0x5100 };
enum { MOVED = 0x47, CHOSEN = 0x48, REST = 0x19, WIDE = 0x280, LEFT_BUTTON = 1 };
/* port: the styles, as text.h's TextStyle: 0 cream, 1 yellow, 2 red. A header names its group and is never a row, so
 * it is always red; a row is yellow while the selection is on it and cream otherwise. */
enum { STYLE_CREAM = 0, STYLE_YELLOW = 1, STYLE_RED = 2 };
enum { LABEL_X = 16, VALUE_RIGHT = 600 };
/* port: the one layout both pages share, which is what makes their headers sit in the same place. A page is a list of
 * lines -- one line per header, one per row -- and every line moves by the same pitch, so a header lands where a row
 * would have gone and both pages' headers line up whatever each page's rows are. The second header's line and
 * everything under it carry GROUP_GAP as well, which is the owner's "shorter than a row" on screen: a header takes
 * less height than a row, and the room it leaves reads as a gap rather than as another row.
 *
 * The pitch is 24 because page 1 is the fuller of the two now -- thirteen lines, eleven rows and two headers -- and has
 * to fit between the title's band (config_bgnd.c's BANDS[0], bottom 95) and the background's own DONE/CANCEL (its
 * CORNERS, top 430). At 26 its last line would sit at 426 and end at 446, on the buttons; at 24 it sits at 402 and ends
 * at 422. ROW_TALL, the selection and hit height, is no taller than the pitch, so a selected row's band never reaches
 * the next row.
 *
 * The fallback panel runs that band, 96..422, so it never reaches the words it did not draw. */
enum { ROW_TOP = 100, ROW_PITCH = 24, ROW_TALL = 24, GROUP_GAP = 14 };
enum { PANEL_X0 = 4, PANEL_Y0 = 96, PANEL_X1 = 635, PANEL_Y1 = 422 };
/* port: the screen's own title where the originals' are: under the logo at the left, its letters' tops on Game
 * Mode's (y 65) -- config_bgnd.c patches the burnt-in one out of (0,60)-(124,94) */
enum { TITLE_X = 8, TITLE_Y = 56 };
enum { BUTTON_Y = 445, DONE_X = 40, CANCEL_RIGHT = 600, DONE_X1 = 0x141, CANCEL_X0 = 0x143, BUTTONS_Y0 = 432,
       BUTTONS_Y1 = 476 };


/* the two headers of each page, in the owner's words */
static const char *const HEADERS[EXTRAS_PAGES][EXTRAS_GROUPS] = { { "Cheats", "Unlocks & mods" },
                                                                 { "In-race options", "Miscellaneous" } };

/* where each page's second group begins, as a row of that page: extras_menu.h's order already has each page's first
 * group first, so the second is the first row under its header (Warrior Rival Car on page 1, Start Screen on page 2) */
static const int SECOND_GROUP[EXTRAS_PAGES] = { ROW_RIVAL - ROW_EXPLODE, ROW_START - ROW_FIRST_P2 };

/* the first switch of each page, in extras_menu.h's flat list */
static int first_of(int page) { return page == 0 ? ROW_EXPLODE : ROW_FIRST_P2; }
/* how many switches a page shows, which is where its Done sits: Cancel is the one past it */
static int rows_of(int page) { return page == 0 ? ROWS_P1 : ROWS_P2; }
static int done_of(int page) { return rows_of(page); }
static int cancel_of(int page) { return rows_of(page) + 1; }
/* Unlock Level is the one switch on page 1 with three values rather than two */
static int unlock_row(void) { return ROW_UNLOCK - ROW_EXPLODE; }
/* Dash box is the other with three values on page 2: Map, Gear lever, None */
static int dash_box_row(void) { return ROW_DASH_BOX - ROW_FIRST_P2; }
/* Speed units is the other, with its own values: mph and km/h, its words the language's until it is chosen. It sits
 * on page 2, the first row. */
static int units_row(void) { return ROW_UNITS - ROW_FIRST_P2; }
static int shows_kmh(const PortExtras *e, int language_kmh)
{
    return e->units == UNITS_KMH || (e->units == UNITS_LANGUAGE && language_kmh);
}

/* port: the switches' words, in extras_menu.h's order, which is the owner's order: page 1 is the first ROWS_P1 of
 * them and page 2 the rest, so a group's rows are a range and a group's header is the one over it. */
static const char *const LABEL[EXTRAS_ROWS] = {
    "Explode mode", "Nitro mode", "Turbo mode", "Warrior Player Car",
    "Warrior Rival Car", "Mirror track mode", "Rally track mode", "Lost Vegas Track", "Lunar Springs Track",
    "Unlock Level", "Time with variants",
    "Speed units", "No driving assists", "Bumper view mirror", "Bumper view police radar",
    "Replay cameras in race mode", "Rival car markers", "3D render stats", "Dash box", "Car colours",
    "Start Screen", "Intro video"
};

/* a switch's field, NULL for the rows with more than two values (Unlock, Start) */
static int *flag_of(PortExtras *e, int page, int row)
{
    if (page == 0) {
        int *const p1[ROWS_P1] = { &e->explode, &e->nitro, &e->turbo, &e->warrior,
                                  &e->warrior_rival, &e->mirror, &e->rally, &e->vegas, &e->lunar_springs,
                                  NULL /* Unlock Level */, &e->variant_time };
        return p1[row];
    }
    int *const p2[ROWS_P2] = { NULL /* Speed units */, &e->no_assists, &e->bumper_mirror, &e->bumper_radar,
                               &e->replay_cameras, &e->car_markers, &e->stats_overlay, NULL /* Dash box */, &e->psx_liveries,
                               NULL /* Start Screen */, &e->no_movies };
    return p2[row];
}

static void value(PortExtras *e, int16_t unlock, int page, int row, int language_kmh, char out[16])
{
    static const char *const START[3] = { "Game Mode", "Network", "Control Central" };
    static const char *const DASH_BOX[3] = { "Map", "Gear lever", "Hidden" };
    const int *const flag = flag_of(e, page, row);
    /* port: the movies switch reads the other way round, "Intro video" On while extras.cfg's no_movies is 0, so the
     * file keeps its meaning */
    const int on = flag != NULL && (flag == &e->no_movies ? !*flag : *flag);
    if (flag != NULL) snprintf(out, 16, "%s", on ? "On" : "Off");
    else if (page == 1 && row == units_row()) snprintf(out, 16, "%s", shows_kmh(e, language_kmh) ? "km/h" : "mph");
    else if (page == 0 && row == unlock_row()) snprintf(out, 16, "%d", unlock);
    else if (page == 1 && row == dash_box_row()) snprintf(out, 16, "%s", DASH_BOX[(unsigned)e->dash_box % 3]);
    else snprintf(out, 16, "%s", START[(unsigned)e->start % 3]);
}

/* port: where a line of a page goes. Line 0 is the first group's header, and every line from the second header's own
 * line down carries the extra GROUP_GAP. */
static int line_y(int page, int line)
{
    return ROW_TOP + line * ROW_PITCH + (line > SECOND_GROUP[page] ? GROUP_GAP : 0);
}

/* the header of a group sits on the line before its first row; the first group keeps line 0 for its header */
static int header_line(int page, int group) { return group == 0 ? 0 : SECOND_GROUP[page] + 1; }
static int line_of(int page, int row) { return row < SECOND_GROUP[page] ? row + 1 : row + 2; }

typedef struct Page {
    int page;
    int16_t selected;
    int redraw, armed;
    int released;     /* the buttons seen up since the last click */
    int palette_on;   /* the palette put on the display, once: it is the same one on every redraw */
    int32_t rest;
    int result;   /* 1 Done, 0 Cancel / Escape, -1 still running */
    int nav_lit;  /* port: the page indicator's triangle the pointer is on (page_arrows.h), 0 none */
} Page;

static void select_row(Page *p, int row, const ExtrasMenuOps *o)
{
    if (p->selected == row) return;
    p->selected = (int16_t)row;
    p->redraw = 1;
    o->sound(o->ctx, MOVED);
}

/* the keys take the pointer along to the row they select, so the pointer's band keeps it (as Options' move_to) */
static void pointer_to(const Page *p, const ExtrasMenuOps *o)
{
    enum { HALF_ROW = ROW_TALL / 2, BUTTON_MID = (BUTTONS_Y0 + BUTTONS_Y1) / 2 };
    if (p->selected == done_of(p->page)) o->pointer_move_to(o->ctx, DONE_X + 20, BUTTON_MID);
    else if (p->selected == cancel_of(p->page)) o->pointer_move_to(o->ctx, CANCEL_RIGHT - 30, BUTTON_MID);
    else o->pointer_move_to(o->ctx, LABEL_X + 40, line_y(p->page, line_of(p->page, p->selected)) + HALF_ROW);
}

static void turn(Page *p, int page, int row, const ExtrasMenuOps *o)
{
    p->page = page;
    p->selected = (int16_t)row;
    p->redraw = 1;
    o->sound(o->ctx, CHOSEN);
    pointer_to(p, o);
}

/* port: the page indicator on this screen's text and triangles */
static PageNavOps nav_ops(const ExtrasMenuOps *o)
{
    const PageNavOps n = { o->ctx, o->plus_text_smooth != NULL ? o->plus_text_smooth : o->plus_text,
                           o->plus_text_width, o->pointer_in_rect, o->plus_arrow_size, o->plus_arrow };
    return n;
}

/* the first band the pointer is in becomes the selection; the page indicator selects nothing, but lights the
 * triangle the pointer is on */
static void follow(Page *p, const ExtrasMenuOps *o)
{
    const PageNavOps nav = nav_ops(o);
    const int lit = page_nav_lit(&nav, p->page, EXTRAS_PAGES);
    if (lit != p->nav_lit) p->nav_lit = lit, p->redraw = 1;
    if (o->pointer_in_rect(o->ctx, EXTRAS_NAV_X0, EXTRAS_NAV_X1, EXTRAS_NAV_Y0, EXTRAS_NAV_Y1)) return;
    for (int row = 0; row < rows_of(p->page); row++) {
        const int top = line_y(p->page, line_of(p->page, row));
        if (o->pointer_in_rect(o->ctx, 0, WIDE, top, top + ROW_TALL)) {
            select_row(p, row, o);
            return;
        }
    }
    if (o->pointer_in_rect(o->ctx, 0, DONE_X1, BUTTONS_Y0, BUTTONS_Y1)) select_row(p, done_of(p->page), o);
    else if (o->pointer_in_rect(o->ctx, CANCEL_X0, WIDE, BUTTONS_Y0, BUTTONS_Y1)) select_row(p, cancel_of(p->page), o);
}

/* a switch row changed by `step` (+1 or -1): flags flip, Unlock and Start step through their three values */
static void change(Page *p, PortExtras *e, int16_t *unlock, int step, const ExtrasMenuOps *o)
{
    int *const flag = flag_of(e, p->page, p->selected);
    if (flag != NULL) {
        *flag = !*flag;
        /* port: with the two allowed together, turning one on leaves the other as it is; the exe's screen clears
         * it, so the pair stays behind the flag. */
        if (o->port_combine_variants == 0) {
            if (*flag && flag == &e->mirror) e->rally = 0;
            if (*flag && flag == &e->rally) e->mirror = 0;
        }
    } else if (p->page == 0 && p->selected == unlock_row()) {
        *unlock = (int16_t)((*unlock + step + 3) % 3);
    } else if (p->page == 1 && p->selected == dash_box_row()) {
        e->dash_box = (DashBox)((e->dash_box + step + 3) % 3);
    } else if (p->page == 1 && p->selected == units_row()) {
        e->units = shows_kmh(e, o->language_kmh) ? UNITS_MPH : UNITS_KMH;
    } else {
        e->start = (StartScreen)((e->start + step + 3) % 3);
    }
    p->redraw = 1;
    o->sound(o->ctx, CHOSEN);
}

/* Space, Enter or the left button */
static void choose(Page *p, PortExtras *e, int16_t *unlock, const ExtrasMenuOps *o)
{
    o->pointer_click(o->ctx);
    if (o->pointer_in_rect(o->ctx, EXTRAS_NAV_X0, EXTRAS_NAV_X1, EXTRAS_NAV_Y0, EXTRAS_NAV_Y1)) {
        turn(p, (p->page + 1) % EXTRAS_PAGES, 0, o);
    } else if (p->selected == done_of(p->page)) {
        o->sound(o->ctx, CHOSEN);
        o->extras_save(o->ctx, e, *unlock);
        p->result = 1;
    } else if (p->selected == cancel_of(p->page)) {
        o->sound(o->ctx, CHOSEN);
        p->result = 0;
    } else {
        change(p, e, unlock, 1, o);
    }
}

/* a key other than the choosing ones */
static void step(Page *p, uint16_t k, PortExtras *e, int16_t *unlock, const ExtrasMenuOps *o)
{
    const int done = done_of(p->page), cancel = cancel_of(p->page);
    if (k == ESCAPE) {
        o->sound(o->ctx, CHOSEN);
        p->result = 0;
    } else if (k == PAGE_DOWN) {
        turn(p, (p->page + 1) % EXTRAS_PAGES, 0, o);
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        /* the walk is the exe's: Down runs the page's switches and stops at Done, then round to the first switch, and
         * Up climbs back the same way, from Cancel to the last switch. Left / Right are what move between the two
         * buttons and across the pages. */
        const int to = k == DOWN ? (p->selected >= done ? 0 : p->selected + 1)
                                 : (p->selected == cancel ? done - 1 : (p->selected + done) % (done + 1));
        select_row(p, to, o);
        pointer_to(p, o);
    } else if (k == RIGHT && p->selected == cancel && p->page + 1 < EXTRAS_PAGES) {
        turn(p, p->page + 1, 0, o);
    } else if (k == LEFT && p->selected == done && p->page > 0) {
        turn(p, p->page - 1, cancel_of(p->page - 1), o);
    } else if ((k == LEFT || k == RIGHT) && p->selected >= done) {
        o->pointer_off(o->ctx);
        select_row(p, p->selected == done ? cancel : done, o);
        pointer_to(p, o);
    } else if (k == LEFT || k == RIGHT) {
        change(p, e, unlock, k == RIGHT ? 1 : -1, o);
    }
}

/* port: the configuration screens' clean background (config_bgnd.c), composed once as the screen opens and kept for
 * as long as it may be up -- 300 KB, so it is not on the frame. Without it the screen runs over the caller's picture,
 * as it did before. */
typedef struct Bgnd {
    int ready;
    uint8_t pixels[EXTRAS_BGND_W * EXTRAS_BGND_H];
    uint8_t palette[768];
} Bgnd;

static void background(Bgnd *b, const ExtrasMenuOps *o)
{
    if (o->plus_bgnd == NULL || o->plus_bgnd_shape == NULL) return;
    if (!o->plus_bgnd(o->ctx, b->pixels, b->palette)) {
        port_log("customize: no clean configuration background, the caller's picture stays\n");
        return;
    }
    b->ready = 1;
}

/* port: every word on the page goes through the draw that puts the background back under it, the one Options' rows
 * and the Graphics screen already use. The screen repaints its whole page on each redraw -- it blits the clean
 * background whole and then writes over it -- so a word drawn straight onto the picture leaves its old edges behind
 * when the pointer moves a row past it. A screen wired without that op falls back to the plain draw. */
static void text(const ExtrasMenuOps *o, int32_t style, const char *word, int32_t x, int32_t y)
{
    if (o->plus_text_smooth != NULL)
        o->plus_text_smooth(o->ctx, style, word, x, y);
    else
        o->plus_text(o->ctx, style, word, x, y);
}

static void text_right(const ExtrasMenuOps *o, int lit, const char *word, int right, int y)
{
    text(o, lit ? STYLE_YELLOW : STYLE_CREAM, word, right - o->plus_text_width(o->ctx, word), y);
}

/* the clean background is a picture of the screen, so it is blitted whole and the words go straight onto it, as on
 * the Graphics screen. Without it (no CD screens to compose it from) the caller's picture is behind, and a dark panel
 * keeps the words readable over it. */
static void draw_page(Page *p, PortExtras *e, int16_t unlock, int32_t bank, const Bgnd *b, const ExtrasMenuOps *o)
{
    o->select(o->ctx, 0);   /* the caller's picture behind */
    if (b->ready) {
        o->plus_bgnd_shape(o->ctx, b->pixels, b->palette);
    } else {
        o->plus_panel(o->ctx, NULL, PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, 1);
    }
    /* the page's two headers: they name the group under them and are never the selection. Underlined across the
     * rows, as the credits head and close off their sections */
    for (int group = 0; group < EXTRAS_GROUPS; group++) {
        const int y = line_y(p->page, header_line(p->page, group));
        if (o->plus_header != NULL) o->plus_header(o->ctx, HEADERS[p->page][group], LABEL_X, y, VALUE_RIGHT - LABEL_X);
        else text(o, STYLE_RED, HEADERS[p->page][group], LABEL_X, y);
    }
    for (int row = 0; row < rows_of(p->page); row++) {
        char v[16];
        const int lit = p->selected == row;
        value(e, unlock, p->page, row, o->language_kmh, v);
        const int y = line_y(p->page, line_of(p->page, row));
        text(o, lit ? STYLE_YELLOW : STYLE_CREAM, LABEL[first_of(p->page) + row], LABEL_X, y);
        text_right(o, lit, v, VALUE_RIGHT, y);
    }
    /* the bottom row's Done and Cancel: the exe's Options look, both always drawn (lit when selected, dark
     * otherwise) the way Options' rows paint their labels. The art replaces the flat panel and the words. Names are
     * capitalised as OPTIONS.QFS ships them -- shape_bank_find uses memcmp, so the lowercase spellings miss. */
    o->blit_own(o->ctx, o->shape(o->ctx, bank, p->selected == done_of(p->page) ? "Donl" : "Dond"));
    o->blit_own(o->ctx, o->shape(o->ctx, bank, p->selected == cancel_of(p->page) ? "Canl" : "Cand"));
    const PageNavOps nav = nav_ops(o);
    page_nav_draw(&nav, p->page, EXTRAS_PAGES, p->nav_lit);
    if (o->plus_title != NULL) o->plus_title(o->ctx, TITLE, TITLE_X, TITLE_Y);
    else text(o, STYLE_CREAM, TITLE, TITLE_X, TITLE_Y);
    o->select_default(o->ctx);
    /* port: the palette that goes with the page, once, and only once the whole page is drawn -- palette_set fills the
     * screen's own palette and never the display, and Options does not fade the display out before it opens this one,
     * so without this the page is drawn on black. Fading it in any earlier paints the menu that opened this screen in
     * this screen's palette for the frames between the two, and the Game Mode art comes out pale for a moment. */
    if (!p->palette_on && b->ready) {
        /* the caller's picture out in its own palette, the page put up on the black display, then faded in: fade_in
         * ramps whatever is showing, so before the flip it would ramp the caller's picture up again first */
        if (o->fade_out != NULL) o->fade_out(o->ctx, 0, 0x100);
        o->flip(o->ctx);
        o->fade_in(o->ctx, 0, 0x100, b->palette);
        /* the styles are a ramp matched to the palette on the display, so they can only be built now that it is on */
        o->text_styles(o->ctx);
        p->palette_on = 1;
        p->redraw = 0;
        return;
    }
    o->flip(o->ctx);
    p->redraw = 0;
}

int extras_menu_run(PortExtras *e, int16_t *unlock, int32_t bank, const ExtrasMenuOps *o)
{
    const PortExtras input = *e;
    const int16_t input_unlock = *unlock;
    Page p = { 0, ROW_EXPLODE, 1, 0, 0, 0, 0, -1, 0 };   /* not released: the click that opened it may still be down */
    static Bgnd bgnd;   /* 300 KB: not on the stack, the web build's is 64 KB */
    bgnd.ready = 0;
    background(&bgnd, o);
    /* port: the styles are a ramp matched to the palette on the display, so they are not asked for here: the screen's
     * own palette has not gone on yet (draw_page does both, once the page is drawn). What is asked for is the way out
     * -- the display is faded out, and the caller's palette with it, so whatever its menu draws next is drawn with a
     * ramp built from that menu's own palette. Options fades this screen's picture out behind it when its palette goes
     * up again; Game Mode does not fade at all, so without this its next frame is drawn in the palette this screen
     * just put there. */
    pointer_to(&p, o);
    while (p.result < 0) {
        follow(&p, o);
        if (p.redraw) draw_page(&p, e, *unlock, bank, &bgnd, o);
        if (!p.armed) {
            p.rest += o->ticks(o->ctx);
            if ((int16_t)p.rest > REST) p.armed = 1;
        }
        const int32_t buttons = o->pointer_buttons(o->ctx);
        if (buttons == 0) p.released = 1;
        o->pointer_activate_if_moved(o->ctx);
        const uint16_t k = (uint16_t)o->key(o->ctx);
        const int clicked = buttons == LEFT_BUTTON && p.released;
        if ((k == SPACE || k == ENTER || clicked) && p.armed) choose(&p, e, unlock, o);
        else if (k != 0 && p.armed) step(&p, k, e, unlock, o);
        if (clicked) p.released = 0;
        if (k != 0 || clicked) p.armed = 0, p.rest = 0;
        o->music(o->ctx);
    }
    if (p.result != 1) *e = input, *unlock = input_unlock;
    /* port: the display faded out as this leaves, the way 00423d28's close_screen does, after the last thing drawn --
     * a fade_out earlier would black the screen out while it was still up. Only with a clean background: without one
     * the screen never put a palette of its own on the display, so there is nothing here but the caller's picture to
     * fade, and fading it out would hand the caller back a black screen. */
    if (bgnd.ready) {
        if (o->fade_out != NULL) o->fade_out(o->ctx, 0, 0x100);
        o->text_styles(o->ctx);
    }
    return p.result;
}
