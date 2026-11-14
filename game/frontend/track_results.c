#include "game/frontend/track_results.h"

#include "game/frontend/records_route.h"

#include <stdio.h>

enum {
    RECORD = 0x95, NAME = 0, TOTAL = 0x65, DISTANCE = 0x69, TOP = 0x6d, WON = 0x592, WON_TRACKS = 8, FLAGS = 0x5a4,
    RACED = 0x5ba, TEXT = 0x5c, SOUND_MOVE = 0x47, SOUND_SELECT = 0x48, WAIT = 0x2ee0, KEY_SPACE = 0x20,
    KEY_ENTER = 0xd, KEY_ESCAPE = 0x1b, LEFT = 0x131, RIGHT = 0x1d6, PLACE_Y = 0x89, NONE = 0x14
};

/* config.dat's image (from 00524df0): the race setup's shorts, then each
 * track's records (TRACK_RECORDS bytes a track) */
enum {
    RACE_TYPE = 0x6d, TRACK_MUSIC = 0x7b, OPPONENT = 0x91, TRACK = 0x95, CAR = 0x9b, TRACK_RECORDS = 0xa6b,
    BEST_NAME = 0x19f, BEST_CAR = 0x1ae, BEST_TOTAL = 0x1ba, BEST_TOP = 0x1be, BEST_MARK = 0x1c2, TOP_NAME = 0x631,
    TOP_CAR = 0x640, TOP_SPEED = 0x650, TOP_MARK = 0x654
};

/* where the pointer goes for "Done" (0) and away from it (1): 0045b684 / 0045b688 */
static const int16_t POINTER_X[2] = { 40, 320 };
static const int16_t POINTER_Y[2] = { 450, 240 };

/* a record's tunes: with both records, the top speed, the best total; the
 * first set after another record tune (0051dd14), the second else; then
 * the pair for the race won without a record */
typedef struct {
    const char *again[3], *first[3], *won_again, *won_first;
} Fanfares;

static const Fanfares CHAMPION = { { "con8_H16.eas", "con6_F16.eas", "con7_H16.eas" },
                                   { "con8_D16.eas", "con6_C16.eas", "con7_D16.eas" }, "con5_D16.eas", "con5_B16.eas" };
static const Fanfares WINNER = { { "con8_G16.eas", "con6_E16.eas", "con7_G16.eas" },
                                 { "con8_A16.eas", "con6_A16.eas", "con7_A16.eas" }, "con5_C16.eas", "con5_A16.eas" };
static const Fanfares RECORDS = { { "con4_C16.eas", "con1_C16.eas", "con2_D16.eas" },
                                  { "con4_F16.eas", "con1_B16.eas", "con2_B16.eas" }, NULL, NULL };

static int16_t g16(const uint8_t *at)
{
    return (int16_t)(uint16_t)(at[0] | at[1] << 8);
}

static int32_t g32(const uint8_t *at)
{
    return (int32_t)((uint32_t)at[0] | (uint32_t)at[1] << 8 | (uint32_t)at[2] << 16 | (uint32_t)at[3] << 24);
}

static const uint8_t *player_at(const ResultsInputs *in, int32_t who) { return in->game + RECORD * who; }
static const uint8_t *track_at(const ResultsInputs *in) { return in->image + TRACK_RECORDS * g16(in->image + TRACK); }

static const char *dnf(const ResultsInputs *in) { return in->german ? "Abbruch" : "DNF"; }

static void place_text(int place, int32_t x, const ResultsOps *ops)
{
    char text[TEXT];
    snprintf(text, sizeof text, "%d / 8", place + 1);
    ops->text_at(ops->ctx, text, x, PLACE_Y);
}

static void two_places(int first, const ResultsOps *ops)
{
    ops->text_at(ops->ctx, first ? "1 / 2" : "2 / 2", LEFT, PLACE_Y);
    ops->text_at(ops->ctx, first ? "2 / 2" : "1 / 2", RIGHT, PLACE_Y);
}

static void draw_place(const ResultsInputs *in, const ResultsOps *ops)
{
    const int type = g16(in->image + RACE_TYPE);
    const int dropped = in->dropped[in->player] != 0;
    const int32_t total = g32(player_at(in, in->player) + TOTAL);
    if (g16(in->game + RACED) != 1) {
        ops->text_at(ops->ctx, dnf(in), LEFT, PLACE_Y);
    } else if (type == 2 || type == 3) {
        if (dropped) ops->text_at(ops->ctx, dnf(in), LEFT, PLACE_Y);
        else place_text(in->place, LEFT, ops);
        if (in->dropped[in->rival]) ops->text_at(ops->ctx, dnf(in), RIGHT, PLACE_Y);
        else place_text(in->rival_place, RIGHT, ops);
    } else if (type == 1) {
        if (dropped) ops->text_at(ops->ctx, dnf(in), LEFT, PLACE_Y);
        else two_places(total < g32(player_at(in, 1 - in->player) + TOTAL), ops);
    } else {
        two_places(!(total > g32(track_at(in) + BEST_TOTAL)), ops);
    }
}

static void draw_heads(const ResultsInputs *in, const ResultsOps *ops)
{
    char text[TEXT];
    const int type = g16(in->image + RACE_TYPE);
    ops->text_fit(ops->ctx, (const char *)player_at(in, in->player) + NAME, LEFT, 0x5a, 0x96);
    ops->text_at(ops->ctx, in->car_names[g16(in->image + CAR)], LEFT, 0x6e);
    if (type == 0) {
        snprintf(text, sizeof text, in->german ? "Bestzeit%s" : "Best Time%s", results_cheats(in) ? " (c)" : "");
        ops->text_at(ops->ctx, text, RIGHT, 0x5a);
        ops->text_at(ops->ctx, in->car_names[(uint32_t)g32(track_at(in) + BEST_CAR)], RIGHT, 0x6e);
    } else if (type == 2 || type == 3) {
        ops->text_at(ops->ctx, (const char *)player_at(in, in->rival) + NAME, RIGHT, 0x5a);
        ops->text_at(ops->ctx, in->car_names[in->rival_car], RIGHT, 0x6e);
    } else {
        ops->text_at(ops->ctx, (const char *)player_at(in, 1 - in->player) + NAME, RIGHT, 0x5a);
        ops->text_at(ops->ctx, in->car_names[g16(in->image + OPPONENT)], RIGHT, 0x6e);
    }
}

/* the three rows (0x9d, 0xbc, 0xd0): total, average, top speed */
static void draw_rows(int32_t x, int32_t total, int32_t avg_time, int32_t avg_distance, int32_t speed,
                      const ResultsOps *ops)
{
    char text[TEXT];
    ops->time_text(ops->ctx, text, total);
    ops->text_at(ops->ctx, text, x, 0x9d);
    ops->average_text(ops->ctx, text, avg_time, avg_distance);
    ops->text_at(ops->ctx, text, x, 0xbc);
    ops->speed_text(ops->ctx, text, speed);
    ops->text_at(ops->ctx, text, x, 0xd0);
}

static void draw_totals(const ResultsInputs *in, const ResultsOps *ops)
{
    const uint8_t *me = player_at(in, in->player);
    const int flagged = g16(in->game + FLAGS + 2 * in->player) != 0;
    const int finished = g16(in->game + RACED) == 1 && in->dropped[in->player] == 0;
    ops->text_select(ops->ctx, 1);
    draw_rows(LEFT, flagged ? 0 : g32(me + TOTAL), flagged ? 0 : g32(me + TOTAL), flagged ? 0 : g32(me + DISTANCE),
              finished ? g32(me + TOP) : 0, ops);
    if (g16(in->image + RACE_TYPE) == 0) {
        const uint8_t *best = track_at(in);
        draw_rows(RIGHT, g32(best + BEST_TOTAL), g32(best + BEST_TOTAL), g32(me + DISTANCE), g32(best + BEST_TOP), ops);
    } else {
        char text[TEXT];
        const uint8_t *rival = player_at(in, in->rival);
        ops->time_text(ops->ctx, text, g32(rival + TOTAL));
        draw_rows(RIGHT, g32(rival + TOTAL), g32(rival + TOTAL), g32(me + DISTANCE), g32(rival + TOP), ops);
    }
}

static void draw_records(const ResultsInputs *in, int16_t row, int16_t top, const ResultsOps *ops)
{
    char text[TEXT];
    const uint8_t *rec = track_at(in);
    const int32_t best_mark = g32(rec + BEST_MARK), top_mark = g32(rec + TOP_MARK);
    ops->text_select(ops->ctx, row == 0 ? 3 : 1);
    ops->text_fit(ops->ctx, (const char *)rec + BEST_NAME, 7, 0x11d, 200);
    ops->time_text(ops->ctx, text, g32(rec + BEST_TOTAL));
    ops->text_at(ops->ctx, text, LEFT, 0x11d);
    ops->text_at(ops->ctx, in->car_names[(uint32_t)g32(rec + BEST_CAR)], RIGHT, 0x11d);
    results_mark_total(in, 0, 0x11d, ops);
    if (best_mark == 0 || best_mark == 1)
        ops->text_at(ops->ctx, in->german ? (best_mark ? "  d" : "  z") : (best_mark ? "h.h." : "t.t."), 0x23f, 0x11d);
    ops->text_select(ops->ctx, top == 1 ? 3 : 1);
    ops->text_fit(ops->ctx, (const char *)rec + TOP_NAME, 7, 0x177, 200);
    ops->speed_text(ops->ctx, text, g32(rec + TOP_SPEED));
    ops->text_at(ops->ctx, text, LEFT, 0x177);
    ops->text_at(ops->ctx, in->car_names[(uint32_t)g32(rec + TOP_CAR)], RIGHT, 0x177);
    results_mark_slot(in, SLOT_TOP, 0x177, ops);
    /* the original's English mark is "t.t." for both */
    if (top_mark == 0 || top_mark == 1) ops->text_at(ops->ctx, in->german ? (top_mark ? "  d" : "  z") : "t.t.", 0x23f, 0x177);
}

static void draw_page(const ResultsInputs *in, int32_t bank, int16_t row, int16_t top, const ResultsOps *ops)
{
    static const char *const TRACK_TAG[3] = { "city", "coas", "alpi" };
    const uint16_t track = (uint16_t)g16(in->image + TRACK);
    ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "bgnd"));
    if (track < 3) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, TRACK_TAG[track]));
    ops->text_select(ops->ctx, 1);
    results_header_marks(in, ops);   /* port: "(CHEATS)" and the table line */
    draw_place(in, ops);
    draw_heads(in, ops);
    draw_totals(in, ops);
    draw_records(in, row, top, ops);
}

/* a tune from the set, 0051dd14 set when it was 0; a race won (won) plays its own without a record */
static const char *tune(const Fanfares *f, int with_record, int16_t row, int16_t top, int won, ResultsState *st)
{
    const char *name = NULL;
    if (with_record) {
        const int k = top == 1 && row == 0 ? 0 : top == 1 ? 1 : 2;
        name = st->record_music ? f->again[k] : f->first[k];
    } else if (won) {
        name = st->record_music ? f->won_again : f->won_first;
    }
    if (name != NULL && st->record_music == 0) st->record_music = 1;
    return name;
}

/* the fanfare at the first draw; 1 when one was asked for a race won without a record */
static int fanfare(const ResultsInputs *in, int16_t ask, int16_t row, int16_t top, ResultsState *st,
                   const ResultsOps *ops)
{
    const int with_record = g16(in->image + TRACK_MUSIC) != 0 && (top == 1 || row == 0);
    const char *name;
    int won_tune = 0;
    if (in->place == 0 && g16(in->image + RACE_TYPE) != 0) {
        int won = 0;
        for (int i = 0; i < WON_TRACKS; i++) won += g16(in->game + WON + 2 * i);
        const int all = in->lost_vegas ? won + g16(in->game + WON + 2 * WON_TRACKS) == 9 : won == 8;
        const Fanfares *f = all && ops->leads(ops->ctx) != 0 ? &CHAMPION : &WINNER;
        won_tune = !with_record && ask != 0;
        name = tune(f, with_record, row, top, ask != 0, st);
    } else {
        name = tune(&RECORDS, with_record, row, top, 0, st);
    }
    if (name != NULL) ops->track_music(ops->ctx, name);
    return won_tune;
}

/* 0045de08 */
void track_results_screen(int32_t ask, const ResultsInputs *in, ResultsState *st, DialogPointer *p,
                          const ResultsOps *ops)
{
    int quit = 0, keys_armed = 1, selected = 0, redraw = 1, buttons_armed = 1, first = 1, moved = 0, won_tune = 0;
    uint32_t waited = 0;  /* the ticks summed, compared as a short */
    const int16_t asked = (int16_t)ask;
    int16_t row = NONE, top = NONE;
    uint8_t black[PALETTE_PUT_BYTES], pal[PALETTE_PUT_BYTES];
    if (asked != 0) {
        row = (int16_t)ops->totals(ops->ctx);
        top = (int16_t)ops->top_record(ops->ctx);
    }
    const int music = g16(in->image + TRACK_MUSIC) != 0, with_record = top == 1 || row == 0;
    ops->flush(ops->ctx);
    palette_put_clear(black);
    ops->pointer_move_to(ops->ctx, POINTER_X[selected], POINTER_Y[selected]);
    const int32_t bank = ops->load_art(ops->ctx, "stats/standtrk.qfs");
    st->inverse = ops->load_inverse(ops->ctx, "stats/standtrk.inv");
    ops->music_feed(ops->ctx);
    const uint8_t *pal_shape = ops->shape(ops->ctx, bank, "!pal");
    palette_put(pal, 0, 0x100, pal_shape + 0x10, pal_shape[0], st->screen_palette);
    ops->fade_in(ops->ctx, 0, 0x100, pal);
    ops->text_styles(ops->ctx);
    if (p->active) ops->pointer_poll(ops->ctx);
    while (!quit) {
        if (ops->pointer_in_rect(ops->ctx, 0, 0x82, 0x1b3, 0x1e0)) {
            if (selected != 0) selected = 0, moved = 1;
        } else if (selected != 1) {
            selected = 1, moved = 1;
        }
        if (redraw) {
            ops->select(ops->ctx, in->screen);
            if (first) draw_page(in, bank, row, top, ops);
            if (selected == 0) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "donl"));
            if (first) {
                ops->palette_write(ops->ctx, 0, 0x100, black);
                ops->pointer_draw(ops->ctx);
            }
            ops->select_default(ops->ctx);
            ops->pointer_hide(ops->ctx);
            ops->lock(ops->ctx);
            ops->blit_word(ops->ctx, in->screen_shape);
            ops->pointer_draw(ops->ctx);
            ops->unlock(ops->ctx);
            ops->flip(ops->ctx);
            ops->select(ops->ctx, in->screen);
            ops->pointer_show(ops->ctx);
            if (first) {
                ops->pointer_mark(ops->ctx, p);
                if (p->active) ops->pointer_buttons(ops->ctx);
                won_tune = fanfare(in, asked, row, top, st, ops);
                ops->fade_in(ops->ctx, 0, 0x100, pal);
                first = 0;
                if (music && (with_record || won_tune)) ops->track_play(ops->ctx);
            }
            ops->delay(ops->ctx, WAIT);
            redraw = 0, moved = 0;
        }
        if (moved) {
            ops->sound(ops->ctx, SOUND_MOVE);
            redraw = 1;
            ops->pointer_hide(ops->ctx);
            ops->lock(ops->ctx);
            ops->select(ops->ctx, in->screen);
            if (selected == 0) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "donl"));
            else if (selected == 1) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "dond"));
            ops->pointer_draw(ops->ctx);
            ops->delay(ops->ctx, WAIT);
            moved = 0;
        }
        if (!keys_armed) {
            waited += (uint32_t)ops->ticks(ops->ctx);
            if ((int16_t)(uint16_t)waited > 0x19) keys_armed = 1;
        }
        if (p->active) p->buttons = (int16_t)ops->pointer_buttons(ops->ctx);
        if (!p->buttons) buttons_armed = 1;
        ops->pointer_activate_if_moved(ops->ctx);
        const uint16_t key = (uint16_t)ops->key(ops->ctx);
        if (key == KEY_SPACE || key == KEY_ENTER || (p->buttons == 1 && buttons_armed && selected == 0)) {
            const int armed = keys_armed;
            ops->pointer_click(ops->ctx);
            if (armed) {
                ops->sound(ops->ctx, SOUND_SELECT);
                break;
            }
            buttons_armed = 0, waited = 0;
            ops->ticks(ops->ctx);
        } else if (key != 0 && keys_armed) {
            if (key == KEY_ESCAPE) {
                ops->sound(ops->ctx, SOUND_SELECT);
                quit = 1;
            } else if (key == 0x4800 || key == 0x4b00 || key == 0x4d00 || key == 0x5000) {
                ops->pointer_off(ops->ctx);
                selected ^= 1;
                ops->pointer_move_to(ops->ctx, POINTER_X[selected], POINTER_Y[selected]);
                moved = 1;
            }
            keys_armed = 0, waited = 0;
            ops->ticks(ops->ctx);
        } else if (key == 0) {
            keys_armed = 1;
        }
        ops->music_service(ops->ctx);
        if (first) continue;
        if (ops->deadline_passed(ops->ctx) || (p->buttons == 2 && buttons_armed) || in->unknown_50a5b6 == 1) {
            ops->sound(ops->ctx, SOUND_SELECT);
            quit = 1;
        }
    }
    if (music && (with_record || won_tune)) {
        ops->track_stop(ops->ctx);
        ops->music_free(ops->ctx);
    }
    ops->fade_out(ops->ctx, 0, 0x100);
    ops->music_feed(ops->ctx);
    ops->free(ops->ctx, bank);
    ops->free_inverse(ops->ctx);
    ops->pointer_mark(ops->ctx, p);
    if (asked == 1) ops->best_totals(ops->ctx, row, top);
    ops->pointer_mark(ops->ctx, p);
}
