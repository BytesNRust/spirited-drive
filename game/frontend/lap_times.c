#include "game/frontend/lap_times.h"

#include "game/frontend/stat_text.h"

#include <stdio.h>

enum {
    RECORD = 0x95, LAPS_AT = 9, LAP_COUNT = 20, DISTANCE = 0x5d, TOP = 0x6d, RUST_A = 0x7d, RUST_B = 0x81,
    RUST_DISTANCE = 0x85, RUST_DISTANCE_LONG = 0x89, RUST_TIME = 0x8d, RUST_SPEED = 0x91, TEXT = 0x5c,
    SOUND_MOVE = 0x47, SOUND_SELECT = 0x48, WAIT = 0x2ee0, KEY_SPACE = 0x20, KEY_ENTER = 0xd, KEY_ESCAPE = 0x1b,
    ROW_Y = 0x7d, ROW_STEP = 0x14, ONE_COLUMN = 8, RUSTY_SPRINGS = 3
};

/* config.dat's image (from 00524df0) */
enum {
    RACE_TYPE = 0x6d, TRACK_MUSIC = 0x7b, TRACK = 0x95, LAPS = 0xb1, TRACK_RECORDS = 0xa6b, LAP_NAME = 0xb5,
    LAP_CAR = 0xc4, LAP_TIME = 0xd0, LAP_MARK = 0xd8
};

/* the selections: "Done", "fin", neither */
enum { DONE, FIN, NEITHER };

/* where the pointer goes for "Done" and "fin": 0045b694 / 0045b698 */
static const int16_t POINTER_X[2] = { 40, 600 };
static const int16_t POINTER_Y[2] = { 450, 450 };
/* the circuits' pictures, tracks 3..8 */
static const char *const TRACK_TAG[6] = { "rust", "autu", "burn", "mont", "trop", "neod" };

static int16_t g16(const uint8_t *at)
{
    return (int16_t)(uint16_t)(at[0] | at[1] << 8);
}

static int32_t g32(const uint8_t *at)
{
    return (int32_t)((uint32_t)at[0] | (uint32_t)at[1] << 8 | (uint32_t)at[2] << 16 | (uint32_t)at[3] << 24);
}

static const uint8_t *me_of(const ResultsInputs *in) { return in->game + RECORD * in->player; }
static int32_t lap_of(const ResultsInputs *in, int i) { return g32(me_of(in) + LAPS_AT + 4 * i); }

/* the first smallest lap above 0, or -1 */
static int best_lap(const ResultsInputs *in)
{
    int best = 0;
    for (int i = 0; i < LAP_COUNT; i++)
        if (lap_of(in, i) < lap_of(in, best) && lap_of(in, i) > 0) best = i;
    return lap_of(in, best) > 0 ? best : -1;
}

static void draw_rust(const ResultsInputs *in, const ResultsOps *ops)
{
    char text[TEXT], time[STAT_TEXT], speed[STAT_TEXT];
    const uint8_t *me = me_of(in);
    if (in->german) {
        ops->speed_text(ops->ctx, text, g32(me + TOP));
        ops->text_at(ops->ctx, text, 0x131, 0x172);
        ops->time_text(ops->ctx, text, g32(me + RUST_A));
        ops->text_at(ops->ctx, text, 0x131, 0x186);
        stat_distance_text(text, in->kmh, g32(me + RUST_DISTANCE));
        ops->text_at(ops->ctx, text, 0x131, 0x19a);
        return;
    }
    ops->speed_text(ops->ctx, text, g32(me + TOP));
    ops->text_at(ops->ctx, text, 200, 0x171);
    ops->time_text(ops->ctx, text, g32(me + RUST_A));
    ops->text_at(ops->ctx, text, 200, 0x186);
    ops->time_text(ops->ctx, text, g32(me + RUST_B));
    ops->text_at(ops->ctx, text, 200, 0x19a);
    ops->time_text(ops->ctx, time, g32(me + RUST_TIME));
    stat_speed_number_text(speed, g32(me + RUST_SPEED));
    snprintf(text, sizeof text, "%s / %s", time, speed);
    ops->text_at(ops->ctx, text, 0x1f0, 0x171);
    stat_distance_text(text, in->kmh, g32(me + RUST_DISTANCE));
    ops->text_at(ops->ctx, text, 0x1ef, 0x186);
    stat_distance_long_text(text, in->kmh, g32(me + RUST_DISTANCE_LONG));
    ops->text_at(ops->ctx, text, 0x1ef, 0x19a);
}

/* the two columns' x: the lap's number, time and average */
static const int32_t LEFT[3] = { 7, 0x68, 0xc7 }, RIGHT[3] = { 0x148, 0x1a6, 0x205 };

/* lap i on row `row` of a column */
static void draw_lap(const ResultsInputs *in, int i, int row, const int32_t x[3], int best, const ResultsOps *ops)
{
    char text[TEXT];
    const int32_t y = ROW_STEP * row + ROW_Y;
    if (i == best) ops->text_select(ops->ctx, 3);
    snprintf(text, sizeof text, "%d", i + 1);
    ops->text_at(ops->ctx, text, x[0], y);
    ops->time_text(ops->ctx, text, lap_of(in, i));
    ops->text_at(ops->ctx, text, x[1], y);
    ops->average_text(ops->ctx, text, lap_of(in, i), g32(me_of(in) + DISTANCE));
    ops->text_at(ops->ctx, text, x[2], y);
    if (i == best) ops->text_select(ops->ctx, 1);
}

static void draw_laps(const ResultsInputs *in, const ResultsOps *ops)
{
    const int laps = g16(in->image + LAPS), best = best_lap(in);
    if (laps <= ONE_COLUMN) {
        for (int i = 0; i < laps; i++) draw_lap(in, i, i, LEFT, best, ops);
        return;
    }
    const int half = (laps + 1) / 2;
    for (int i = 0; i < half; i++) draw_lap(in, i, i, LEFT, best, ops);
    for (int i = half; i < laps; i++) draw_lap(in, i, i - half, RIGHT, best, ops);
}

static void draw_record(const ResultsInputs *in, int16_t record, const ResultsOps *ops)
{
    char text[TEXT];
    const uint8_t *rec = in->image + TRACK_RECORDS * g16(in->image + TRACK);
    const int32_t mark = g32(rec + LAP_MARK);
    ops->text_select(ops->ctx, record == 1 ? 3 : 1);
    ops->text_fit(ops->ctx, (const char *)rec + LAP_NAME, 7, 0x144, 200);
    ops->time_text(ops->ctx, text, g32(rec + LAP_TIME));
    ops->text_at(ops->ctx, text, 0x131, 0x144);
    ops->text_at(ops->ctx, in->car_names[(uint32_t)g32(rec + LAP_CAR)], 0x1d6, 0x144);
    if (mark == 0 || mark == 1)
        ops->text_at(ops->ctx, in->german ? (mark ? "  d" : "  z") : (mark ? "h.h." : "t.t."), 0x23f, 0x144);
}

static void draw_page(const ResultsInputs *in, int32_t bank, int16_t record, const ResultsOps *ops)
{
    const int16_t track = g16(in->image + TRACK);
    const int time_trial = g16(in->image + RACE_TYPE) == 0;
    ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "bgnd"));
    if ((uint16_t)(track - RUSTY_SPRINGS) <= 5) {
        ops->blit(ops->ctx, ops->shape(ops->ctx, bank, TRACK_TAG[track - RUSTY_SPRINGS]));
        if (track != RUSTY_SPRINGS || !time_trial) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "no5w"));
    }
    ops->text_select(ops->ctx, 1);
    if (track == RUSTY_SPRINGS && time_trial && in->variant_r == 0) draw_rust(in, ops);
    draw_laps(in, ops);
    draw_record(in, record, ops);
}

/* the selection's button lit ("donl" / "finl") or dim ("dond" / "find") */
static void draw_button(int selection, int lit, int32_t bank, const ResultsOps *ops)
{
    if (selection == DONE) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, lit ? "donl" : "dond"));
    else if (selection == FIN) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, lit ? "finl" : "find"));
}

/* 0046067c */
int32_t lap_times_screen(int32_t record, int32_t tune, const ResultsInputs *in, ResultsState *st, DialogPointer *p,
                         const ResultsOps *ops)
{
    int quit = 0, keys_armed = 1, selected = FIN, previous = FIN, redraw = 1, buttons_armed = 1, first = 1, moved = 0;
    int16_t result = 0;
    uint32_t waited = 0;  /* the ticks summed, compared as a short */
    const int16_t lit = (int16_t)record;
    const int fanfare = g16(in->image + TRACK_MUSIC) != 0 && lit == 1 && (int16_t)tune == lit;
    uint8_t black[PALETTE_PUT_BYTES], pal[PALETTE_PUT_BYTES];
    ops->flush(ops->ctx);
    palette_put_clear(black);
    ops->pointer_move_to(ops->ctx, POINTER_X[selected], POINTER_Y[selected]);
    const int32_t bank = ops->load_art(ops->ctx, "stats/standlo2.qfs");
    st->inverse = ops->load_inverse(ops->ctx, "stats/standlo2.inv");
    ops->music_feed(ops->ctx);
    const uint8_t *pal_shape = ops->shape(ops->ctx, bank, "!pal");
    palette_put(pal, 0, 0x100, pal_shape + 0x10, pal_shape[0], st->screen_palette);
    ops->fade_in(ops->ctx, 0, 0x100, pal);
    ops->text_styles(ops->ctx);
    if (p->active) ops->pointer_poll(ops->ctx);
    while (!quit) {
        const int over = ops->pointer_in_rect(ops->ctx, 0, 0x82, 0x1b3, 0x1e0) ? DONE
                       : ops->pointer_in_rect(ops->ctx, 400, 0x280, 0x1b3, 0x1e0) ? FIN : NEITHER;
        if (selected != over) previous = selected, selected = over, moved = 1;
        if (redraw) {
            ops->select(ops->ctx, in->screen);
            if (first) draw_page(in, bank, lit, ops);
            draw_button(selected, 1, bank, ops);
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
                if (fanfare) {
                    if (st->record_music == 0) st->record_music = 1;
                    ops->track_music(ops->ctx, "con2_C16.eas");
                }
                ops->fade_in(ops->ctx, 0, 0x100, pal);
                first = 0;
                if (fanfare) ops->track_play(ops->ctx);
            }
            ops->delay(ops->ctx, WAIT);
            redraw = 0, moved = 0;
        }
        if (moved) {
            ops->sound(ops->ctx, SOUND_MOVE);
            ops->pointer_hide(ops->ctx);
            ops->lock(ops->ctx);
            redraw = 1;
            ops->select(ops->ctx, in->screen);
            draw_button(previous, 0, bank, ops);
            draw_button(selected, 1, bank, ops);
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
                if (selected == DONE) result = 1, quit = 1;
                else if (selected == FIN) result = 0, quit = 1;
            }
            buttons_armed = 0, keys_armed = 0, waited = 0;
            ops->ticks(ops->ctx);
        } else if (key != 0 && keys_armed) {
            if (key == KEY_ESCAPE) {
                ops->sound(ops->ctx, SOUND_SELECT);
                quit = 1;
            } else if ((key == 0x4800 || key == 0x5000) && selected == NEITHER) {
                ops->pointer_off(ops->ctx);
                selected = DONE;
                ops->pointer_move_to(ops->ctx, POINTER_X[selected], POINTER_Y[selected]);
                moved = 1;
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
    if (fanfare) {
        ops->track_stop(ops->ctx);
        ops->music_free(ops->ctx);
    }
    ops->fade_out(ops->ctx, 0, 0x100);
    ops->music_feed(ops->ctx);
    ops->free(ops->ctx, bank);
    ops->free_inverse(ops->ctx);
    ops->pointer_mark(ops->ctx, p);
    return result;
}
