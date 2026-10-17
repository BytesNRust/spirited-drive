#include "game/frontend/results_screen.h"

#include <stdio.h>

enum {
    RECORD = 0x95, NAME = 0, TIME = 0x59, DISTANCE = 0x5d, SPEED = 0x61, TOTAL = 0x65, FLAGS = 0x5a4,
    RACED = 0x5ba, TEXT = 0x5c, SOUND_MOVE = 0x47, SOUND_SELECT = 0x48, WAIT = 0x2ee0, KEY_SPACE = 0x20,
    KEY_ENTER = 0xd, KEY_ESCAPE = 0x1b, LEFT = 0x131, RIGHT = 0x1d6, PLACE_Y = 0x89
};

/* config.dat's image (from 00524df0): the race setup's shorts, then each
 * track's records (TRACK_RECORDS bytes a track) */
enum {
    RACE_TYPE = 0x6d, TRACK_MUSIC = 0x7b, OPPONENT = 0x91, SEGMENT = 0x93, TRACK = 0x95, CAR = 0x9b,
    TRACK_RECORDS = 0xa6b, SEGMENT_RECORDS = 0x27, REC_NAME = 0xb5, REC_CAR = 0xc4, REC_TIME = 0xd0,
    REC_GEARBOX = 0xd8, LAP_NAME = 0x12a, LAP_CAR = 0x139, LAP_SPEED = 0x149, LAP_GEARBOX = 0x14d,
    BEST_CAR = 0x1ae, BEST = 0x658, BEST_SPEED = 0x664
};

/* where the pointer goes for "Done" (0) and away from it (1): 0045b67c / 0045b680 */
static const int16_t POINTER_X[2] = { 40, 320 };
static const int16_t POINTER_Y[2] = { 450, 240 };

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

static void draw_place(const ResultsInputs *in, int segment, const ResultsOps *ops)
{
    const int type = g16(in->image + RACE_TYPE);
    const int dropped = in->dropped[in->player] != 0;
    if (g16(in->game + RACED) != 1) {
        ops->text_at(ops->ctx, dnf(in), LEFT, PLACE_Y);
    } else if (type == 2 || type == 3) {
        if (dropped) ops->text_at(ops->ctx, dnf(in), LEFT, PLACE_Y);
        else place_text(in->place, LEFT, ops);
        if (in->dropped[in->rival]) ops->text_at(ops->ctx, dnf(in), RIGHT, PLACE_Y);
        else place_text(in->rival_place, RIGHT, ops);
    } else if (type == 1) {
        if (dropped) ops->text_at(ops->ctx, dnf(in), LEFT, PLACE_Y);
        else two_places(g32(player_at(in, in->player) + TIME) < g32(player_at(in, 1 - in->player) + TIME), ops);
    } else {
        two_places(!(g32(player_at(in, in->player) + TOTAL) > g32(track_at(in) + BEST + 4 * segment)), ops);
    }
}

static void draw_heads(const ResultsInputs *in, int segment, const ResultsOps *ops)
{
    char text[TEXT];
    const int type = g16(in->image + RACE_TYPE);
    snprintf(text, sizeof text, in->german ? "ETAPPE %d" : "SEGMENT %d", segment + 1);
    ops->text_at(ops->ctx, text, 8, 0x5a);
    ops->text_fit(ops->ctx, (const char *)player_at(in, in->player) + NAME, LEFT, 0x5a, 0x96);
    ops->text_at(ops->ctx, in->car_names[g16(in->image + CAR)], LEFT, 0x6e);
    if (type == 0) {
        snprintf(text, sizeof text, in->german ? "Bestzeit" : "Best Time");
        ops->text_at(ops->ctx, text, RIGHT, 0x5a);
        ops->text_at(ops->ctx, in->car_names[(uint32_t)g32(track_at(in) + BEST_CAR)], RIGHT, 0x6e);
    } else {
        ops->text_at(ops->ctx, (const char *)player_at(in, in->rival) + NAME, RIGHT, 0x5a);
        ops->text_at(ops->ctx, in->car_names[type == 2 || type == 3 ? in->rival_car : g16(in->image + OPPONENT)],
                     RIGHT, 0x6e);
    }
}

/* the four rows (0x9b, 0xad, 0xcb, 0xdd): time, total, average, top speed */
static void draw_rows(int32_t x, int32_t time, int32_t total, int32_t avg_time, int32_t avg_distance, int32_t speed,
                      const ResultsOps *ops)
{
    char text[TEXT];
    ops->time_text(ops->ctx, text, time);
    ops->text_at(ops->ctx, text, x, 0x9b);
    ops->time_text(ops->ctx, text, total);
    ops->text_at(ops->ctx, text, x, 0xad);
    ops->average_text(ops->ctx, text, avg_time, avg_distance);
    ops->text_at(ops->ctx, text, x, 0xcb);
    ops->speed_text(ops->ctx, text, speed);
    ops->text_at(ops->ctx, text, x, 0xdd);
}

static void draw_times(const ResultsInputs *in, int segment, const ResultsOps *ops)
{
    const uint8_t *me = player_at(in, in->player);
    const int dropped = in->dropped[in->player] != 0;
    const int16_t flagged = g16(in->game + FLAGS + 2 * in->player);
    ops->text_select(ops->ctx, 1);
    draw_rows(LEFT, g32(me + TIME), flagged ? 0 : g32(me + TOTAL), dropped ? 0 : g32(me + TIME),
              dropped ? 0 : g32(me + DISTANCE), g16(in->game + RACED) == 1 && !dropped ? g32(me + SPEED) : 0, ops);
    if (g16(in->image + RACE_TYPE) == 0) {
        const uint8_t *best = track_at(in) + BEST + 4 * segment;
        const int32_t split = segment == 0 ? g32(best) : (int32_t)((uint32_t)g32(best) - (uint32_t)g32(best - 4));
        draw_rows(RIGHT, split, g32(best), split, g32(me + DISTANCE), g32(best + BEST_SPEED - BEST), ops);
    } else {
        const uint8_t *rival = player_at(in, in->rival);
        draw_rows(RIGHT, g32(rival + TIME), g32(rival + TOTAL), g32(rival + TIME), g32(me + DISTANCE),
                  g32(rival + SPEED), ops);
    }
}

/* a record's gearbox mark at 0x23f: 0 automatic, 1 manual, else none */
static void draw_gearbox(const ResultsInputs *in, int32_t gearbox, int32_t y, const char *manual,
                         const ResultsOps *ops)
{
    if (gearbox != 0 && gearbox != 1) return;
    const char *mark = in->german ? (gearbox ? "  d" : "  z") : (gearbox ? manual : "t.t.");
    ops->text_at(ops->ctx, mark, 0x23f, y);
}

static void draw_records(const ResultsInputs *in, int segment, int beaten, const ResultsOps *ops)
{
    char text[TEXT];
    const uint8_t *rec = track_at(in) + SEGMENT_RECORDS * segment;
    ops->text_select(ops->ctx, beaten == 1 || beaten == 3 ? 3 : 1);
    ops->text_fit(ops->ctx, (const char *)rec + REC_NAME, 7, 0x136, 200);
    ops->time_text(ops->ctx, text, g32(rec + REC_TIME));
    ops->text_at(ops->ctx, text, LEFT, 0x136);
    ops->text_at(ops->ctx, in->car_names[(uint32_t)g32(rec + REC_CAR)], RIGHT, 0x136);
    draw_gearbox(in, g32(rec + REC_GEARBOX), 0x136, "h.h.", ops);
    ops->text_select(ops->ctx, beaten == 2 || beaten == 3 ? 3 : 1);
    ops->text_fit(ops->ctx, (const char *)rec + LAP_NAME, 7, 0x17c, 200);
    ops->speed_text(ops->ctx, text, g32(rec + LAP_SPEED));
    ops->text_at(ops->ctx, text, LEFT, 0x17c);
    ops->text_at(ops->ctx, in->car_names[(uint32_t)g32(rec + LAP_CAR)], RIGHT, 0x17c);
    draw_gearbox(in, g32(rec + LAP_GEARBOX), 0x17c, "h.h", ops);
}

static void draw_page(const ResultsInputs *in, int32_t bank, int segment, int beaten, const ResultsOps *ops)
{
    static const char *const TRACK_TAG[3] = { "city", "coas", "alpi" };
    const uint16_t track = (uint16_t)g16(in->image + TRACK);
    ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "bgnd"));
    if (track < 3) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, TRACK_TAG[track]));
    ops->text_select(ops->ctx, 1);
    draw_place(in, segment, ops);
    draw_heads(in, segment, ops);
    draw_times(in, segment, ops);
    draw_records(in, segment, beaten, ops);
}

/* 0045cea0 */
void results_screen(int32_t ask_records, const ResultsInputs *in, ResultsState *st, DialogPointer *p,
                    const ResultsOps *ops)
{
    int quit = 0, keys_armed = 1, selected = 0, redraw = 1, buttons_armed = 1, first = 1, moved = 0;
    uint32_t waited = 0;  /* the ticks summed, compared as a short */
    int16_t beaten = 0;
    const int16_t segment_word = g16(in->image + SEGMENT);
    const int segment = segment_word ? (int16_t)(segment_word - 1) : 2;
    uint8_t black[PALETTE_PUT_BYTES], pal[PALETTE_PUT_BYTES];
    st->record_music = 0;
    if ((int16_t)ask_records != 0) beaten = (int16_t)ops->records(ops->ctx);
    const int fanfare = g16(in->image + TRACK_MUSIC) != 0 && beaten > 0;
    ops->flush(ops->ctx);
    palette_put_clear(black);
    ops->pointer_move_to(ops->ctx, POINTER_X[selected], POINTER_Y[selected]);
    const int32_t bank = ops->load_art(ops->ctx, "stats/standseg.qfs");
    st->inverse = ops->load_inverse(ops->ctx, "stats/standseg.inv");
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
            if (first) draw_page(in, bank, segment, beaten, ops);
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
                ops->pointer_buttons(ops->ctx);
                if (fanfare) {
                    st->record_music = 1;
                    ops->track_music(ops->ctx, beaten == 3 ? "con4_B16.eas" : beaten == 2 ? "con1_A16.eas"
                                                                                      : "con2_A16.eas");
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
            redraw = 1;
            ops->pointer_hide(ops->ctx);
            ops->lock(ops->ctx);
            ops->select(ops->ctx, in->screen);
            if (selected == 0) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "donl"));
            else if (selected == 1) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "dond"));
            ops->pointer_draw(ops->ctx);
            moved = 0;
            ops->delay(ops->ctx, WAIT);
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
            } else if ((key == 0x4800 || key == 0x4b00 || key == 0x4d00 || key == 0x5000) && selected == 1) {
                selected = 0;
                ops->pointer_off(ops->ctx);
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
}
