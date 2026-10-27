#include "game/frontend/best_times.h"

#include "game/frontend/records_route.h"
#include "game/sys/free_laps.h"
#include "game/sys/free_records.h"

#include <stdio.h>
#include <string.h>

enum {
    TRACKS = 9, ITEMS = 8, DONE = 0, PREV = 1, NEXT = 2, BACK = 3, NOTHING = 4, CHEATS = 5 /* port: the gap after Done */,
    LAPS_DOWN = 6 /* port (free records): the lap-count's left arrow, on the "Best times" header's row */, LAPS_UP = 7 /* and its right one */,
    MAX_LAPS = 3 + FREE_LAPS_MAX,
    LOST_VEGAS = 8, TOURNAMENT = 3,
    OPPONENT_TOURNAMENT = 10, SOUND_MOVE = 0x47, SOUND_SELECT = 0x48, WAIT = 0x2ee0, REST = 0x19, KEY_SPACE = 0x20,
    KEY_ENTER = 0xd, KEY_ESCAPE = 0x1b, KEY_UP = 0x4800, KEY_LEFT = 0x4b00, KEY_RIGHT = 0x4d00, KEY_DOWN = 0x5000,
    ALL = 0x100, TEXT = 0x50, NAME_X = 8, NAME_WIDTH = 0x10e, TIME_X = 0x118, CAR_X = 0x1e0, SHIFT_X = 0x24e,
    ROW_Y = 0x73, ROW_STEP = 0x16, TOP_Y = 0x17c, MARK_X = 0xe0, LABEL_X = 0x118, LABEL_Y = 0x28
};

/* port: the cheats item's label, centred in the gap between Done's words (to 0x4c) and the left arrow (0xdd), its
 * capitals on Done's baseline (rows 449..460: the text top is 6 rows above its capitals): its pointer, and the hit
 * rect it is found in (between Done's rect and the left arrow's, 0x82..0xc8) */
enum {
    CHEATS_X = 0x94, CHEATS_Y = 0x1bb, CHEATS_POINTER_X = 0x94, CHEATS_RECT_X0 = 0x82, CHEATS_RECT_X1 = 0xc8
};
/* port (free records): the lap-count control on the header's row, right of "Best times": a left arrow, "Laps: N" and a right arrow (the arrows are the
 * items; the text between them is only the value). The header's capitals are on rows 79..90; the text top is 6 rows above its capitals. */
enum {
    LAPS_Y = 0x49, LAPS_X = 0x1d6, LAPS_DOWN_X = 0x194, LAPS_UP_X = 0x20e, LAPS_RECT_Y0 = 0x46, LAPS_RECT_Y1 = 0x5e,
    LAPS_DOWN_RECT_X0 = 0x180, LAPS_DOWN_RECT_X1 = 0x1ae, LAPS_UP_RECT_X0 = 0x1fc, LAPS_UP_RECT_X1 = 0x22a
};

/* the tracks' files (004673f8) and the cars (0046741c) */
static const char *const TRACK_FILE[TRACKS] = { "citybst", "coastbst", "alpbst", "rustbst", "autbst", "brunbst",
                                               "vertbst", "tropbst", "lostbst" };
static const char *const CAR[12] = { "RX-7", "NSX", "SUPRA", "911", "ZR-1", "VIPER", "512TR", "DIABLO", "PACK", "CLOCK",
                                      "CLASS", "WARRIOR" };   /* the car names of 0..11 (the results' names) */
/* a record's car: the names of 0..11, blank for anything else */
static const char *car_name(int32_t car) { return car >= 0 && car < (int32_t)(sizeof CAR / sizeof CAR[0]) ? CAR[car] : ""; }
/* the items lit and not */
static const char *const LIT[4] = { "donl", "larl", "rarl", "desl" };
static const char *const DIM[4] = { "dond", "lard", "rard", "desd" };
/* the arrows' moves (004673d0, 004673da) and where the pointer goes per item (004673e4, 004673ee) */
static const int16_t LEFT[ITEMS] = { 3, 0, 1, 2, 0 };
static const int16_t RIGHT[ITEMS] = { 1, 2, 3, 0, 0 };
static const int16_t POINTER_X[ITEMS] = { 38, 231, 390, 568, 320, CHEATS_POINTER_X, LAPS_DOWN_X + 6, LAPS_UP_X + 6 };
static const int16_t POINTER_Y[ITEMS] = { 455, 455, 455, 455, 240, 455, 85, 85 };
/* port: with a plus the cheats item between Done and the left arrow, the ring runs Done, cheats, left arrow */
static const int16_t LEFT_PLUS[ITEMS] = { 3, CHEATS, 1, 2, 0, DONE, 1 };
static const int16_t RIGHT_PLUS[ITEMS] = { CHEATS, 2, 3, 0, 0, PREV, 2 };
/* port (free records): with the lap-count control the ring (Left / Right) runs Done, cheats, the exe's left arrow, the lap-count's left arrow, its right
 * arrow, the exe's right arrow, Back: the arrow key that used to reach the lap-count item between the exe's arrows reaches its arrows in the same place
 * of the ring. Up and Down keep toggling the cheats. */
static const int16_t LEFT_LAPS[ITEMS] = { 3, CHEATS, LAPS_UP, 2, 0, DONE, 1, LAPS_DOWN };
static const int16_t RIGHT_LAPS[ITEMS] = { CHEATS, LAPS_DOWN, 3, 0, 0, PREV, LAPS_UP, 2 };
/* 004baef8: a tournament's car per track */
static const int16_t TOURNAMENT_CAR[TRACKS] = { 0, 3, 6, 0, 3, 3, 6, 6, 7 };

typedef struct Screen {
    int32_t bank;      /* ebp */
    int16_t item;      /* esp + 0x6ec */
    int16_t was;       /* esp + 0x6e4 */
    int16_t result;    /* esp + 0x6c4 */
    int done;          /* esp + 0x6cc */
    int redraw;        /* esp + 0x6c8 */
    int refresh;       /* esp + 0x6d0: the art drawn whole, the palette faded in */
    int repaint;       /* port: the records drawn again over the art, no fade */
    int laps;          /* port (free records): the lap count shown, 0 until the item steps it (then the menu's) */
    int changed;       /* esp + 0x6d4 */
    int armed;         /* esp + 0x6e0: the keys taken */
    int released;      /* esp + 0x6d8: the buttons seen up */
    int32_t rest;      /* esp + 0x6dc: the ticks summed, compared as a short */
    uint8_t rgb[PALETTE_PUT_BYTES];
} Screen;

static int16_t g16(const uint8_t *config, int at)
{
    return (int16_t)(uint16_t)(config[at] | config[at + 1] << 8);
}

static void p16(uint8_t *config, int at, int v)
{
    config[at] = (uint8_t)v, config[at + 1] = (uint8_t)((unsigned)v >> 8);
}

static int32_t d32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

static int track_of(const BestTimesState *st)
{
    return g16(st->config, TRACK_SELECT_TRACK);
}

/* port (free records): whether the lap-count item is on this screen: a circuit, the flag on */
static int selector(const BestTimesState *st) { return st->plus && st->plus->laps_select && track_of(st) > 2; }

/* the palette from the bank's "!pal", the screens cleared, the palette set */
static void palette_load(Screen *s, BestTimesState *st, const BestTimesOps *ops)
{
    ops->music_feed(ops->ctx);
    const uint8_t *pal = ops->shape(ops->ctx, s->bank, "!pal");
    palette_put(s->rgb, 0, ALL, pal + 0x10, pal[0], st->screen_palette);
    ops->flush(ops->ctx);
    ops->palette_set(ops->ctx, 0, ALL, s->rgb);
    ops->inverse_build(ops->ctx);
    ops->text_styles(ops->ctx);
}

static void move_to(int item, const BestTimesOps *ops)
{
    ops->pointer_move_to(ops->ctx, POINTER_X[item], POINTER_Y[item]);
}

/* the item under the pointer becomes the one lit */
static void follow(Screen *s, const BestTimesState *st, const BestTimesOps *ops)
{
    int16_t now = NOTHING;
    if (ops->pointer_in_rect(ops->ctx, 0, 0x82, 0x1b3, 0x1e0)) now = DONE;
    else if (st->plus && ops->pointer_in_rect(ops->ctx, CHEATS_RECT_X0, CHEATS_RECT_X1, 0x1b3, 0x1e0))
        now = CHEATS;   /* port */
    else if (ops->pointer_in_rect(ops->ctx, 0xc8, 0xfa, 0x1b3, 0x1e0)) now = PREV;
    else if (ops->pointer_in_rect(ops->ctx, 0x172, 0x1a4, 0x1b3, 0x1e0)) now = NEXT;
    else if (ops->pointer_in_rect(ops->ctx, 0x1e0, 0x280, 0x1b3, 0x1df)) now = BACK;
    else if (selector(st) && ops->pointer_in_rect(ops->ctx, LAPS_DOWN_RECT_X0, LAPS_DOWN_RECT_X1, LAPS_RECT_Y0, LAPS_RECT_Y1)) now = LAPS_DOWN;   /* port */
    else if (selector(st) && ops->pointer_in_rect(ops->ctx, LAPS_UP_RECT_X0, LAPS_UP_RECT_X1, LAPS_RECT_Y0, LAPS_RECT_Y1)) now = LAPS_UP;
    if (now == s->item) return;
    s->was = s->item;
    s->item = now;
    s->changed = 1;
}

/* the gearbox's mark: 0 automatic, 1 manual */
static void shift(const BestTimesInputs *in, int32_t gearbox, int32_t y, const BestTimesOps *ops)
{
    if (gearbox == 0) ops->text_shadowed(ops->ctx, in->german ? "  z" : "t.t.", SHIFT_X, y);
    else if (gearbox == 1) ops->text_shadowed(ops->ctx, in->german ? "  d" : "h.h.", SHIFT_X, y);
}

/* port: the letters of the cheats a row was set with, drawn after the car */
static void draw_marks(const BestTimesState *st, int track, int slot, int32_t y, const BestTimesOps *ops)
{
    char text[8];
    if (slot < 0 || !st->plus->cheats || !st->plus->marks[track]) return;
    records_mark_text(st->plus->marks[track][slot], text);
    if (text[0] != '\0') ops->text_shadowed(ops->ctx, text, MARK_X, y);
}

/* the lap counts the item steps through, ascending: the track's three and the free counts with a table in the table shown */
static int lap_counts(const BestTimesState *st, int out[MAX_LAPS])
{
    const int track = track_of(st);
    int free_laps[FREE_LAPS_MAX];
    const int n_free = free_records_laps(track, (RecordVariant)st->plus->variant, st->plus->cheats, free_laps, FREE_LAPS_MAX);
    int n = 0, f = 0, e = 0;
    while (e < 3 || f < n_free) {
        const int exe = e < 3 ? free_laps_exe_laps(track, e) : 1 << 30, free_count = f < n_free ? free_laps[f] : 1 << 30;
        const int next = exe <= free_count ? exe : free_count;
        if (exe <= free_count) e++;
        if (free_count <= next) f++;
        out[n++] = next;
    }
    return n;
}

/* the lap count shown: the item's pick, else the menu's laps choice */
static int shown_laps(const Screen *s, const BestTimesState *st)
{
    return s->laps ? s->laps : free_laps_exe_laps(track_of(st), g16(st->config, TRACK_SELECT_LAPS_CHOICE));
}

static void draw_records(const Screen *s, const BestTimesInputs *in, const BestTimesState *st, const BestTimesOps *ops)
{
    const int track = track_of(st);
    const uint8_t *top = (st->plus ? st->plus->records[st->plus->cheats] : in->records) + track * BEST_TIMES_TRACK;
    char text[TEXT];
    int choice = g16(st->config, TRACK_SELECT_LAPS_CHOICE);
    const FreeTable *free_table = NULL;   /* port (free records): the table of a lap count the track lacks */
    if (selector(st)) {
        const int laps = shown_laps(s, st), exe = free_laps_choice(track, laps);
        if (exe >= 0) choice = exe;
        else free_table = free_records_find(track, (RecordVariant)st->plus->variant, st->plus->cheats, laps);
    }
    ops->text_select(ops->ctx, 1);
    for (int16_t i = 0; i < BEST_TIMES_ROWS; i++) {
        const uint8_t *row = free_table ? free_table->rows + i * BEST_TIMES_ROW : top + i * BEST_TIMES_ROW;
        if (track > 2 && !free_table) row += choice * BEST_TIMES_CHOICE;
        const int32_t y = i * ROW_STEP + ROW_Y;
        ops->text_fit(ops->ctx, (const char *)row, NAME_X, y, NAME_WIDTH);
        ops->time_text(ops->ctx, text, d32(row + BEST_TIMES_TIME));
        ops->text_shadowed(ops->ctx, text, TIME_X, y);
        ops->text_shadowed(ops->ctx, car_name(d32(row + BEST_TIMES_CAR)), CAR_X, y);
        shift(in, d32(row + BEST_TIMES_SHIFT), y, ops);
        if (free_table) {
            char marks[8];
            records_mark_text(st->plus->cheats ? free_table->marks[i] : 0, marks);
            if (marks[0] != '\0') ops->text_shadowed(ops->ctx, marks, MARK_X, y);
        } else if (st->plus) {
            draw_marks(st, track, records_slot_totals(track, choice, i), y, ops);
        }
    }
    ops->text_select(ops->ctx, 1);
    if (st->plus) ops->text_shadowed(ops->ctx, st->plus->cheats ? "Records (cheats)" : "Records", LABEL_X, LABEL_Y);
    if (!d32(top + BEST_TIMES_TOP_SET)) return;
    ops->text_fit(ops->ctx, (const char *)top + BEST_TIMES_TOP, NAME_X, TOP_Y, NAME_WIDTH);
    ops->speed_text(ops->ctx, text, d32(top + BEST_TIMES_TOP_SPEED));
    ops->text_shadowed(ops->ctx, text, TIME_X, TOP_Y);
    ops->text_shadowed(ops->ctx, car_name(d32(top + BEST_TIMES_TOP_CAR)), CAR_X, TOP_Y);
    shift(in, d32(top + BEST_TIMES_TOP_SHIFT), TOP_Y, ops);
    if (st->plus) draw_marks(st, track, SLOT_TOP, TOP_Y, ops);
}

/* port: the cheats item's words, cream when idle and yellow when lit; "No cheats" while the cheats table is up */
static void cheats_label(const Screen *s, const BestTimesState *st, const BestTimesOps *ops)
{
    const char *const text = st->plus->cheats ? "No cheats" : "Cheats";
    ops->plus_text(ops->ctx, s->item == CHEATS, text, CHEATS_X - ops->plus_text_width(ops->ctx, text) / 2, CHEATS_Y);
}

/* port (free records): the lap-count control's words, cream when idle and yellow when lit: "<" and ">" for the arrows */
static void laps_label(const Screen *s, const BestTimesState *st, const BestTimesOps *ops)
{
    char text[TEXT];
    snprintf(text, sizeof text, "Laps: %d", shown_laps(s, st));
    ops->plus_text(ops->ctx, 0, text, LAPS_X - ops->plus_text_width(ops->ctx, text) / 2, LAPS_Y);
    ops->plus_text(ops->ctx, s->item == LAPS_DOWN, "<", LAPS_DOWN_X, LAPS_Y);
    ops->plus_text(ops->ctx, s->item == LAPS_UP, ">", LAPS_UP_X, LAPS_Y);
}

static void draw(Screen *s, const BestTimesInputs *in, BestTimesState *st, DialogPointer *p, const BestTimesOps *ops)
{
    ops->select(ops->ctx, in->screen);
    if (s->refresh || s->repaint) {
        ops->blit(ops->ctx, ops->shape(ops->ctx, s->bank, "bgnd"));
        draw_records(s, in, st, ops);
    }
    if ((uint16_t)s->item < 4) ops->blit(ops->ctx, ops->shape(ops->ctx, s->bank, LIT[s->item]));
    if (st->plus) cheats_label(s, st, ops);   /* port */
    if (selector(st)) laps_label(s, st, ops);
    if (s->refresh) {
        uint8_t black[PALETTE_PUT_BYTES];
        palette_put_clear(black);
        ops->palette_set(ops->ctx, 0, ALL, black);
    }
    ops->select_default(ops->ctx);
    ops->pointer_hide(ops->ctx);
    ops->lock(ops->ctx);
    ops->blit_word(ops->ctx, in->screen_shape);
    ops->unlock(ops->ctx);
    ops->flip(ops->ctx);
    ops->select(ops->ctx, in->screen);
    ops->pointer_show(ops->ctx);
    if (s->refresh) {
        ops->pointer_mark(ops->ctx, p);
        ops->pointer_buttons(ops->ctx);
        ops->fade_in(ops->ctx, 0, ALL, s->rgb);
        ops->text_styles(ops->ctx);
        s->refresh = 0;
    }
    ops->deadline_set(ops->ctx, WAIT);
    s->redraw = 0;
    s->repaint = 0;
}

/* a change of item: the old one dimmed, the new one lit (no flip until the redraw) */
static void draw_change(Screen *s, const BestTimesInputs *in, const BestTimesOps *ops)
{
    ops->sound(ops->ctx, SOUND_MOVE);
    ops->pointer_hide(ops->ctx);
    ops->lock(ops->ctx);
    ops->select(ops->ctx, in->screen);
    s->redraw = 1;
    if ((uint16_t)s->was < BACK) ops->blit(ops->ctx, ops->shape(ops->ctx, s->bank, DIM[s->was]));
    else if (s->was == BACK) ops->blit_masked(ops->ctx, ops->shape(ops->ctx, s->bank, DIM[BACK]));
    if ((uint16_t)s->item < BACK) ops->blit(ops->ctx, ops->shape(ops->ctx, s->bank, LIT[s->item]));
    else if (s->item == BACK) ops->blit_masked(ops->ctx, ops->shape(ops->ctx, s->bank, LIT[BACK]));
    if (s->was == CHEATS || s->item == CHEATS || s->was == LAPS_DOWN || s->item == LAPS_DOWN || s->was == LAPS_UP || s->item == LAPS_UP) s->repaint = 1;   /* port: its words are drawn again over the art */
    s->changed = 0;
}

/* the laps for the choice: tracks 4..7 are the short ones */
static void set_laps(BestTimesState *st)
{
    const int16_t choice = g16(st->config, TRACK_SELECT_LAPS_CHOICE);
    const int track = track_of(st), short_track = track >= 4 && track <= 7;
    p16(st->config, TRACK_SELECT_LAPS, choice == 0 ? (short_track ? 2 : 4)
                                       : choice == 1 ? (short_track ? 6 : 8) : (short_track ? 0xc : 0x10));
}

/* another track: a tournament's car, the laps for the choice, the art loaded again under the data path */
static void change_track(Screen *s, const BestTimesInputs *in, BestTimesState *st, int track,
                         const BestTimesOps *ops)
{
    char path[TEXT];
    p16(st->config, TRACK_SELECT_TRACK, track);
    if (g16(st->config, TRACK_SELECT_RACE_TYPE) == TOURNAMENT) {
        p16(st->config, TRACK_SELECT_OPPONENT, OPPONENT_TOURNAMENT);
        p16(st->config, TRACK_SELECT_CAR, TOURNAMENT_CAR[track_of(st)]);
        ops->car_flags(ops->ctx);
    }
    set_laps(st);
    s->laps = 0;
    ops->fade_out(ops->ctx, 0, ALL);
    ops->pointer_hide(ops->ctx);
    ops->music_feed(ops->ctx);
    ops->free_inverse(ops->ctx);
    ops->free(ops->ctx, s->bank);
    snprintf(path, sizeof path, "%stracksel/%s.qfs", in->paths, TRACK_FILE[track_of(st)]);
    s->bank = ops->load_bank(ops->ctx, path, 0);
    snprintf(path, sizeof path, "tracksel/%s.inv", TRACK_FILE[track_of(st)]);
    st->inverse = ops->load_inverse(ops->ctx, path);
    palette_load(s, st, ops);
    s->redraw = 1;
    s->refresh = 1;
}

/* port: the other table shown */
static void toggle(Screen *s, BestTimesState *st, const BestTimesOps *ops)
{
    st->plus->cheats ^= 1;
    if (s->laps && selector(st)) {   /* port (free records): a count the other table lacks falls back to the menu's */
        int counts[MAX_LAPS], found = 0;
        const int n = lap_counts(st, counts);
        for (int i = 0; i < n; i++) found |= counts[i] == s->laps;
        if (!found) s->laps = 0;
    }
    s->redraw = s->repaint = 1;
    ops->sound(ops->ctx, SOUND_MOVE);
}

/* port: the item the arrows move to; with a plus the cheats item is in the ring */
static int16_t neighbour(const BestTimesState *st, uint16_t key, int16_t item)
{
    if (selector(st)) return (key == KEY_LEFT ? LEFT_LAPS : RIGHT_LAPS)[item];
    if (st->plus) return (key == KEY_LEFT ? LEFT_PLUS : RIGHT_PLUS)[item];
    return (key == KEY_LEFT ? LEFT : RIGHT)[item];
}

static void act(Screen *s, const BestTimesInputs *in, BestTimesState *st, const BestTimesOps *ops)
{
    const int track = track_of(st);
    switch ((uint16_t)s->item) {
    case DONE:
        s->result = 1;
        s->done = 1;
        break;
    case CHEATS:   /* port */
        toggle(s, st, ops);
        break;
    case LAPS_DOWN:
    case LAPS_UP: {   /* port (free records): the previous / next lap count that has a table, round */
        int counts[MAX_LAPS], at = -1;
        const int n = lap_counts(st, counts), now = shown_laps(s, st);
        for (int i = 0; i < n; i++)
            if (counts[i] == now) at = i;
        s->laps = counts[s->item == LAPS_UP ? (at + 1) % n : at < 0 ? n - 1 : (at + n - 1) % n];
        s->redraw = s->repaint = 1;
        ops->sound(ops->ctx, SOUND_MOVE);
        break;
    }
    case PREV: {
        int16_t to = (int16_t)(track - 1);
        if (to < 0) to = in->lost_vegas ? LOST_VEGAS : 7;
        change_track(s, in, st, to, ops);
        break;
    }
    case NEXT:
        change_track(s, in, st, (int16_t)((track + 1) % (in->lost_vegas ? 9 : 8)), ops);
        break;
    case BACK:
        s->result = 0;
        s->done = 1;
        break;
    }
}

/* a key other than the acting ones: Escape goes back, the arrows step */
static void step(Screen *s, uint16_t key, BestTimesState *st, const BestTimesOps *ops)
{
    if (key == KEY_ESCAPE) {
        ops->sound(ops->ctx, SOUND_SELECT);
        s->done = 1;
    } else if (key == KEY_LEFT || key == KEY_RIGHT) {
        ops->pointer_off(ops->ctx);
        s->was = s->item;
        s->item = neighbour(st, key, s->item);
        s->changed = 1;
        move_to(s->item, ops);
    } else if (st->plus && (key == KEY_UP || key == KEY_DOWN)) {
        toggle(s, st, ops);   /* port */
    } else if ((key == KEY_UP || key == KEY_DOWN) && s->item == NOTHING) {
        ops->pointer_off(ops->ctx);
        s->item = DONE;
        s->was = DONE;
        s->changed = 1;
        move_to(DONE, ops);
    }
    ops->ticks(ops->ctx);
    s->armed = 0;
    s->rest = 0;
}

int32_t best_times_screen(const BestTimesInputs *in, BestTimesState *st, DialogPointer *p, const BestTimesOps *ops)
{
    static Screen zero;
    Screen s = zero;
    char path[TEXT];
    s.item = s.was = BACK;
    s.redraw = s.refresh = s.armed = s.released = 1;
    if (!p->active) move_to(BACK, ops);
    snprintf(path, sizeof path, "tracksel/%s.qfs", TRACK_FILE[track_of(st)]);
    s.bank = ops->load_art(ops->ctx, path);
    snprintf(path, sizeof path, "tracksel/%s.inv", TRACK_FILE[track_of(st)]);
    st->inverse = ops->load_inverse(ops->ctx, path);
    palette_load(&s, st, ops);
    if (p->active) ops->pointer_poll(ops->ctx);
    ops->pointer_mark(ops->ctx, p);
    while (!s.done) {
        follow(&s, st, ops);
        if (s.redraw) draw(&s, in, st, p, ops);
        if (s.changed) draw_change(&s, in, ops);
        if (!s.armed) {
            s.rest += ops->ticks(ops->ctx);
            if ((int16_t)s.rest > REST) s.armed = 1;
        }
        if (p->active) p->buttons = (int16_t)ops->pointer_buttons(ops->ctx);
        if (p->buttons == 0) s.released = 1;
        ops->pointer_activate_if_moved(ops->ctx);
        const uint16_t key = (uint16_t)ops->key(ops->ctx);
        if (st->plus && ops->plus_toggle && ops->plus_toggle(ops->ctx)) toggle(&s, st, ops);   /* port: Tab */
        if (key == KEY_SPACE || key == KEY_ENTER || (p->buttons == 1 && s.released)) {
            ops->pointer_click(ops->ctx);
            if (s.armed) {
                ops->sound(ops->ctx, SOUND_SELECT);
                act(&s, in, st, ops);
            }
            ops->ticks(ops->ctx);
            s.released = 0;
            s.armed = 0;
            s.rest = 0;
        } else if (key != 0 && s.armed) {
            step(&s, key, st, ops);
        } else if (key == 0) {
            s.armed = 1;
        }
        const int refresh = s.refresh;
        ops->music(ops->ctx);
        if (st->plus && p->buttons == 2 && s.released) {   /* port: the right button turns the table */
            toggle(&s, st, ops);
            s.released = 0;
        } else if (!refresh && (ops->deadline_passed(ops->ctx) || (p->buttons == 2 && s.released))) {
            ops->sound(ops->ctx, SOUND_SELECT);
            s.done = 1;
        }
    }
    if (s.result) {
        ops->music_fade_out(ops->ctx);
        ops->fade_out(ops->ctx, 0, ALL);
        ops->music_end(ops->ctx);
    } else {
        ops->fade_out(ops->ctx, 0, ALL);
        ops->music_feed(ops->ctx);
    }
    ops->free(ops->ctx, s.bank);
    ops->free_inverse(ops->ctx);
    ops->pointer_mark(ops->ctx, p);
    return s.result;
}
