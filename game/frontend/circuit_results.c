#include "game/frontend/circuit_results.h"

#include "game/frontend/records_route.h"
#include "game/sys/free_laps.h"

#include <stdio.h>

enum {
    RECORD = 0x95, NAME = 0, TOTAL = 0x65, DISTANCE = 0x69, TOP = 0x6d, WON = 0x592, WON_TRACKS = 8, FLAGS = 0x5a4,
    RACED = 0x5ba, TEXT = 0x5c, SOUND_MOVE = 0x47, SOUND_SELECT = 0x48, WAIT = 0x2ee0, KEY_SPACE = 0x20,
    KEY_ENTER = 0xd, KEY_ESCAPE = 0x1b, LEFT = 0x131, RIGHT = 0x1d6, PLACE_Y = 0x89, NONE = 0x14, CIRCUITS = 3
};

/* config.dat's image (from 00524df0): the race setup's shorts, then each
 * track's records (TRACK_RECORDS bytes a track, a circuit's best totals
 * BEST_TABLE bytes a difficulty) */
enum {
    RACE_TYPE = 0x6d, TRACK_MUSIC = 0x7b, OPPONENT = 0x91, TRACK = 0x95, CAR = 0x9b, LAPS = 0xb1,
    TRACK_RECORDS = 0xa6b, BEST_TABLE = 0x186, BEST_NAME = 0x19f, BEST_CAR = 0x1ae, BEST_TOTAL = 0x1ba,
    BEST_TOP = 0x1be, BEST_MARK = 0x1c2, TOP_NAME = 0x631, TOP_CAR = 0x640, TOP_SPEED = 0x650, TOP_MARK = 0x654
};

/* the selections: "Done", the lap times, neither */
enum { DONE, LAPS_SHOWN, NEITHER };

/* where the pointer goes for "Done" and the lap times: 0045b68c / 0045b690 */
static const int16_t POINTER_X[2] = { 40, 600 };
static const int16_t POINTER_Y[2] = { 450, 450 };
/* the circuits' pictures, tracks 3..8 */
static const char *const TRACK_TAG[6] = { "rust", "autu", "burn", "mont", "trop", "neod" };
/* the selections' pictures, lit and dimmed */
static const char *const LIT[2] = { "donl", "lapl" };
static const char *const DIM[2] = { "dond", "lapd" };

/* a record's tunes: both records, the top speed, the best total; then a
 * race won's without a record */
static const char *const CHAMPION[4] = { "con8_D16.eas", "con6_C16.eas", "con7_D16.eas", "con5_B16.eas" };
static const char *const WINNER[4] = { "con8_A16.eas", "con6_A16.eas", "con7_A16.eas", "con5_A16.eas" };
static const char *const RECORDS[3] = { "con4_f16.eas", "con1_B16.eas", "con2_B16.eas" };

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
static const uint8_t *best_at(const ResultsInputs *in)
{
    return track_at(in) + BEST_TABLE * records_choice(in->image);   /* port: a free race shows its lap count's table */
}

static const char *dnf(const ResultsInputs *in) { return in->german ? "Abbruch" : "DNF"; }

/* what the place says: the race won (0x634), the rival column's finisher
 * (0x660: 1 the one after the player) */
typedef struct {
    int won, beat;
} Outcome;

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

static void draw_place(const ResultsInputs *in, Outcome *out, const ResultsOps *ops)
{
    const int type = g16(in->image + RACE_TYPE);
    const int dropped = in->dropped[in->player] != 0;
    const int32_t total = g32(player_at(in, in->player) + TOTAL);
    if (g16(in->game + RACED) != 1) {
        ops->text_at(ops->ctx, dnf(in), LEFT, PLACE_Y);
    } else if (type == 2 || type == 3) {
        ops->results_order(ops->ctx);
        out->beat = in->circuit_place == 0;
        if (out->beat) out->won = 1;
        if (dropped) ops->text_at(ops->ctx, dnf(in), LEFT, PLACE_Y);
        else place_text(in->circuit_place, LEFT, ops);
        if (in->dropped[in->rival]) ops->text_at(ops->ctx, dnf(in), RIGHT, PLACE_Y);
        else place_text(in->circuit_rival_place, RIGHT, ops);
    } else if (type == 1) {
        if (dropped) {
            ops->text_at(ops->ctx, dnf(in), LEFT, PLACE_Y);
            return;
        }
        out->beat = total < g32(player_at(in, 1 - in->player) + TOTAL);
        if (out->beat) out->won = 1;
        two_places(out->beat, ops);
    } else {
        two_places(!(total > g32(best_at(in) + BEST_TOTAL)), ops);
    }
}

static void draw_heads(const ResultsInputs *in, const ResultsOps *ops)
{
    char text[TEXT];
    const int type = g16(in->image + RACE_TYPE);
    snprintf(text, sizeof text, in->german ? "%d Rundenrennen" : "%d Lap race", free_laps_or(g16(in->image + LAPS)));
    ops->text_at(ops->ctx, text, 8, 0x5f);
    ops->text_fit(ops->ctx, (const char *)player_at(in, in->player) + NAME, LEFT, 0x5a, 0x96);
    ops->text_at(ops->ctx, in->car_names[g16(in->image + CAR)], LEFT, 0x6e);
    if (type == 0) {
        snprintf(text, sizeof text, in->german ? "Bestzeit%s" : "Best Time%s", results_cheats(in) ? " (c)" : "");
        ops->text_at(ops->ctx, text, RIGHT, 0x5a);
        ops->text_at(ops->ctx, in->car_names[(uint32_t)g32(best_at(in) + BEST_CAR)], RIGHT, 0x6e);
    } else if (type == 2 || type == 3) {
        snprintf(text, sizeof text, "%s", (const char *)player_at(in, in->rival) + NAME);
        ops->text_at(ops->ctx, text, RIGHT, 0x5a);
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

static void draw_totals(const ResultsInputs *in, const Outcome *out, const ResultsOps *ops)
{
    const uint8_t *me = player_at(in, in->player);
    const int type = g16(in->image + RACE_TYPE);
    const int flagged = g16(in->game + FLAGS + 2 * in->player) != 0;
    const int finished = g16(in->game + RACED) == 1 && in->dropped[in->player] == 0;
    const int32_t distance = g32(me + DISTANCE);
    ops->text_select(ops->ctx, 1);
    draw_rows(LEFT, flagged ? 0 : g32(me + TOTAL), flagged ? 0 : g32(me + TOTAL), flagged ? 0 : distance,
              finished ? g32(me + TOP) : 0, ops);
    if (type == 0) {
        const uint8_t *best = best_at(in);
        draw_rows(RIGHT, g32(best + BEST_TOTAL), g32(best + BEST_TOTAL), distance, g32(best + BEST_TOP), ops);
    } else if (finished) {
        const uint8_t *rival = player_at(in, type == 1 ? 1 - in->player : in->finish[out->beat]);
        draw_rows(RIGHT, g32(rival + TOTAL), g32(rival + TOTAL), distance, g32(rival + TOP), ops);
    } else {
        draw_rows(RIGHT, 0, 0, 0, 0, ops);
    }
}

static const char *mark_text(const ResultsInputs *in, int32_t mark)
{
    return in->german ? (mark ? "  d" : "  z") : (mark ? "h.h." : "t.t.");
}

static void draw_records(const ResultsInputs *in, int16_t row, int16_t top, const ResultsOps *ops)
{
    char text[TEXT];
    const uint8_t *best = best_at(in), *rec = track_at(in);
    const int32_t best_mark = g32(best + BEST_MARK), top_mark = g32(rec + TOP_MARK);
    ops->text_select(ops->ctx, row == 0 ? 3 : 1);
    ops->text_fit(ops->ctx, (const char *)best + BEST_NAME, 7, 0x11d, 200);
    ops->time_text(ops->ctx, text, g32(best + BEST_TOTAL));
    ops->text_at(ops->ctx, text, LEFT, 0x11d);
    ops->text_at(ops->ctx, in->car_names[(uint32_t)g32(best + BEST_CAR)], RIGHT, 0x11d);
    results_mark_total(in, 0, 0x11d, ops);
    if (best_mark == 0 || best_mark == 1) ops->text_at(ops->ctx, mark_text(in, best_mark), 0x23f, 0x11d);
    ops->text_select(ops->ctx, top == 1 ? 3 : 1);
    ops->text_fit(ops->ctx, (const char *)rec + TOP_NAME, 7, 0x177, 200);
    ops->speed_text(ops->ctx, text, g32(rec + TOP_SPEED));
    ops->text_at(ops->ctx, text, LEFT, 0x177);
    ops->text_at(ops->ctx, in->car_names[(uint32_t)g32(rec + TOP_CAR)], RIGHT, 0x177);
    results_mark_slot(in, SLOT_TOP, 0x177, ops);
    if (top_mark == 0 || top_mark == 1) ops->text_at(ops->ctx, mark_text(in, top_mark), 0x23f, 0x177);
}

static void draw_page(const ResultsInputs *in, int32_t bank, int16_t row, int16_t top, Outcome *out,
                      const ResultsOps *ops)
{
    const uint16_t circuit = (uint16_t)(g16(in->image + TRACK) - CIRCUITS);
    ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "bgnd"));
    if (circuit < 6) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, TRACK_TAG[circuit]));
    ops->text_select(ops->ctx, 1);
    results_header_marks(in, ops);   /* port: "(CHEATS)" and the table line */
    draw_place(in, out, ops);
    draw_heads(in, ops);
    draw_totals(in, out, ops);
    draw_records(in, row, top, ops);
}

/* a set's tune for the records beaten, NULL for none */
static const char *pick(const char *const set[], int16_t row, int16_t top)
{
    return top == 1 && row == 0 ? set[0] : top == 1 ? set[1] : row == 0 ? set[2] : NULL;
}

/* the fanfare at a first draw; 1 when it asked for a race won's tune */
static int fanfare(const ResultsInputs *in, int16_t asked, int16_t row, int16_t top, int won, int music_off,
                   ResultsState *st, const ResultsOps *ops)
{
    const int music = g16(in->image + TRACK_MUSIC) != 0, with_record = top == 1 || row == 0;
    const char *name = NULL;
    int won_tune = 0;
    if (won || g16(in->image + RACE_TYPE) == 3) {
        int sum = 0;
        for (int i = 0; i < WON_TRACKS; i++) sum += g16(in->game + WON + 2 * i);
        const int all = in->lost_vegas ? sum + g16(in->game + WON + 2 * WON_TRACKS) == 9 : sum == 8;
        const int champion = all && !music_off && ops->leads(ops->ctx) != 0;
        const char *const *set = champion ? CHAMPION : WINNER;
        st->record_music = 1;
        if (!music_off && (champion || won)) {
            if (music && with_record) name = pick(set, row, top);
            else if (asked != 0) name = set[3], won_tune = 1;
        }
    } else if (music && with_record && !music_off) {
        st->record_music = 1;
        name = pick(RECORDS, row, top);
    }
    if (name != NULL) ops->track_music(ops->ctx, name);
    return won_tune;
}

/* the screen's art after the palette is cleared: its bank, the palette faded in */
static int32_t load_art(ResultsState *st, uint8_t pal[], const ResultsOps *ops)
{
    const int32_t bank = ops->load_art(ops->ctx, "stats/standlo1.qfs");
    st->inverse = ops->load_inverse(ops->ctx, "stats/standlo1.inv");
    ops->music_feed(ops->ctx);
    const uint8_t *pal_shape = ops->shape(ops->ctx, bank, "!pal");
    palette_put(pal, 0, 0x100, pal_shape + 0x10, pal_shape[0], st->screen_palette);
    ops->fade_in(ops->ctx, 0, 0x100, pal);
    ops->text_styles(ops->ctx);
    return bank;
}

/* the music when a fanfare is on */
static int playing(const ResultsInputs *in, int16_t row, int16_t top, int won_tune, int music_off)
{
    return g16(in->image + TRACK_MUSIC) != 0 && (top == 1 || row == 0 || won_tune) && !music_off;
}

/* 0045eff4 */
void circuit_results_screen(int32_t ask, const ResultsInputs *in, ResultsState *st, DialogPointer *p,
                            const ResultsOps *ops)
{
    int quit = 0, keys_armed = 1, buttons_armed = 1, redraw = 1, first = 1, moved = 0, won_tune = 0;
    int selected = DONE, previous = DONE;
    uint32_t waited = 0;  /* the ticks summed, compared as a short */
    const int16_t asked = (int16_t)ask;
    int music_off = asked == 0;
    int16_t row = NONE, top = NONE, lap = NONE, result = 0, shown = 0;
    Outcome out = { 0, 0 };
    uint8_t black[PALETTE_PUT_BYTES], pal[PALETTE_PUT_BYTES];
    st->record_music = 0;
    if (asked != 0) {
        row = (int16_t)ops->totals(ops->ctx);
        top = (int16_t)ops->top_record(ops->ctx);
        lap = (int16_t)ops->lap_record(ops->ctx);
    }
    ops->flush(ops->ctx);
    palette_put_clear(black);
    ops->pointer_move_to(ops->ctx, POINTER_X[DONE], POINTER_Y[DONE]);
    int32_t bank = load_art(st, pal, ops);
    if (p->active) ops->pointer_poll(ops->ctx);
    while (!quit) {
        if (ops->pointer_in_rect(ops->ctx, 0, 0x82, 0x1b3, 0x1e0)) {
            if (selected != DONE) previous = selected, moved = 1, selected = DONE;
        } else if (ops->pointer_in_rect(ops->ctx, 400, 0x280, 0x1b3, 0x1e0)) {
            if (selected != LAPS_SHOWN) previous = selected, moved = 1, selected = LAPS_SHOWN;
        } else if (selected != NEITHER) {
            previous = selected, moved = 1, selected = NEITHER;
        }
        if (redraw) {
            ops->select(ops->ctx, in->screen);
            if (first) draw_page(in, bank, row, top, &out, ops);
            if (selected != NEITHER) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, LIT[selected]));
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
                if (fanfare(in, asked, row, top, out.won, music_off, st, ops)) won_tune = 1;
                ops->fade_in(ops->ctx, 0, 0x100, pal);
                first = 0;
                if (playing(in, row, top, won_tune, music_off)) ops->track_play(ops->ctx);
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
            if (previous != NEITHER) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, DIM[previous]));
            if (selected != NEITHER) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, LIT[selected]));
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
        if (key == KEY_SPACE || key == KEY_ENTER || (p->buttons == 1 && buttons_armed)) {
            const int armed = keys_armed;
            ops->pointer_click(ops->ctx);
            if (armed) {
                ops->sound(ops->ctx, SOUND_SELECT);
                if (selected == DONE) {
                    quit = 1;
                } else if (selected == LAPS_SHOWN) {
                    if (playing(in, row, top, won_tune, music_off)) {
                        ops->track_stop(ops->ctx);
                        ops->music_free(ops->ctx);
                        music_off = 1;
                    }
                    ops->free(ops->ctx, bank);
                    ops->free_inverse(ops->ctx);
                    ops->fade_out(ops->ctx, 0, 0x100);
                    ops->music_feed(ops->ctx);
                    result = (int16_t)ops->lap_times(ops->ctx, lap, ++shown);
                    if (result != 0) {
                        quit = 1;
                    } else {
                        ops->flush(ops->ctx);
                        palette_put_clear(black);
                        bank = load_art(st, pal, ops);
                        selected = previous = LAPS_SHOWN;
                        if (!p->active) ops->pointer_move_to(ops->ctx, POINTER_X[LAPS_SHOWN], POINTER_Y[LAPS_SHOWN]);
                        ops->pointer_hide(ops->ctx);
                        redraw = 1, first = 1;
                    }
                }
            }
            ops->ticks(ops->ctx);
            buttons_armed = 0, keys_armed = 0, waited = 0;
        } else if (key != 0 && keys_armed) {
            if (key == KEY_ESCAPE) {
                ops->sound(ops->ctx, SOUND_SELECT);
                quit = 1;
            } else if (key == 0x4800 || key == 0x5000) {
                if (selected == NEITHER) {
                    ops->pointer_off(ops->ctx);
                    ops->pointer_move_to(ops->ctx, POINTER_X[DONE], POINTER_Y[DONE]);
                    moved = 1, selected = DONE;
                }
            } else if (key == 0x4b00 || key == 0x4d00) {
                ops->pointer_off(ops->ctx);
                previous = selected;
                selected = selected == NEITHER ? DONE : selected ^ 1;
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
    if (result == 0) {
        if (playing(in, row, top, won_tune, music_off)) {
            ops->track_stop(ops->ctx);
            ops->music_free(ops->ctx);
        }
        ops->fade_out(ops->ctx, 0, 0x100);
        ops->music_feed(ops->ctx);
        ops->free(ops->ctx, bank);
        ops->free_inverse(ops->ctx);
    }
    if (asked == 1) ops->best_totals(ops->ctx, row, top);
    ops->pointer_mark(ops->ctx, p);
}
