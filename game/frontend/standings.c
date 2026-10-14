#include "game/frontend/standings.h"

#include <stdio.h>

enum {
    RECORD = 0x95, NAME = 0, TIME_B = 0x59, TIME_A = 0x65, STANDINGS = 0x570, SEGMENTS = 0x592, FLAGS = 0x5a4,
    CAR_POINTS_ON = 0x5ba, ROW_Y = 0xa6, ROW_H = 0x1d, PLAYERS = 8, TEXT = 0x28, SOUND_MOVE = 0x47,
    SOUND_SELECT = 0x48, WAIT = 0x2ee0, KEY_SPACE = 0x20, KEY_ENTER = 0xd, KEY_ESCAPE = 0x1b
};

/* where the pointer goes for "Done" (0) and away from it (1): 0045b69c / 0045b6a0 */
static const int16_t POINTER_X[2] = { 40, 320 };
static const int16_t POINTER_Y[2] = { 450, 240 };
/* the tracks' shapes (0045b6a4) */
static const char *const TRACK_TAG[9] = { "city", "coas", "alpi", "rust", "autu", "burn", "mont", "trop", "neod" };

static int16_t g16(const uint8_t *game, int at)
{
    return (int16_t)(uint16_t)(game[at] | game[at + 1] << 8);
}

static int32_t g32(const uint8_t *game, int at)
{
    return (int32_t)((uint32_t)game[at] | (uint32_t)game[at + 1] << 8 | (uint32_t)game[at + 2] << 16 |
                     (uint32_t)game[at + 3] << 24);
}

static void draw_segment(const StandingsInputs *in, int32_t bank, int segment, const StandingsOps *ops)
{
    char text[TEXT];
    ops->blit(ops->ctx, ops->shape(ops->ctx, bank, TRACK_TAG[in->track]));
    ops->text_select(ops->ctx, 2);
    if (in->track < 3) {
        ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "segt"));
        snprintf(text, sizeof text, in->german ? "ETAPPE %d" : "SEGMENT %d", segment);
    } else {
        snprintf(text, sizeof text, in->german ? "%d Rundenrennen" : "%d Lap race", in->laps);
    }
    ops->text_boxed(ops->ctx, text, 8, 0x5a);
    ops->results_order(ops->ctx);
    for (int row = 0; row < PLAYERS; row++) {
        const int y = ROW_Y + ROW_H * row, who = (int16_t)in->finish[row];
        ops->text_select(ops->ctx, 1);
        ops->time_text(ops->ctx, text, g16(in->game, FLAGS + 2 * who) ? 0 : g32(in->game, RECORD * who + TIME_A));
        ops->text_boxed(ops->ctx, text, 0x135, y);
        if (in->track < 3) {
            ops->time_text(ops->ctx, text, g32(in->game, RECORD * who + TIME_B));
            ops->text_boxed(ops->ctx, text, 0x1ef, y);
        }
        ops->text_select(ops->ctx, 3);
        ops->text_boxed(ops->ctx, (const char *)in->game + RECORD * who + NAME, 0x7b, y);
    }
}

static void draw_totals(const StandingsInputs *in, const StandingsOps *ops)
{
    char text[TEXT];
    int order[PLAYERS], swapped;
    for (int i = 0; i < PLAYERS; i++) order[i] = i;
    do {
        swapped = 0;
        for (int i = 0; i < PLAYERS - 1; i++) {
            const int a = order[i], b = order[i + 1];
            if (g16(in->game, STANDINGS + 2 * a) < g16(in->game, STANDINGS + 2 * b)) {
                order[i] = b, order[i + 1] = a;
                swapped = 1;
            }
        }
    } while (swapped);
    for (int row = 0; row < PLAYERS; row++) {
        const int y = ROW_Y + ROW_H * row, who = order[row];
        ops->text_select(ops->ctx, 1);
        snprintf(text, sizeof text, "%d", g16(in->game, STANDINGS + 2 * who));
        ops->text_boxed(ops->ctx, text, 0x1ef, y);
        if (g16(in->game, CAR_POINTS_ON) == 1) snprintf(text, sizeof text, "%d", in->car_points[who]);
        else snprintf(text, sizeof text, "0");
        ops->text_boxed(ops->ctx, text, 0x13e, y);
        ops->text_select(ops->ctx, 3);
        ops->text_boxed(ops->ctx, (const char *)in->game + RECORD * who + NAME, 0x7b, y);
    }
}

/* 00461540 */
void standings_screen(int segment_mode, const StandingsInputs *in, StandingsState *st, DialogPointer *p,
                      const StandingsOps *ops)
{
    int quit = 0, keys_armed = 1, selected = 0, redraw = 1, buttons_armed = 1, first = 1, moved = 0;
    uint32_t waited = 0;  /* the ticks summed, compared as a short */
    const int segment = in->segment ? in->segment : 3;
    uint8_t black[PALETTE_PUT_BYTES], pal[PALETTE_PUT_BYTES];
    ops->flush(ops->ctx);
    palette_put_clear(black);
    ops->pointer_move_to(ops->ctx, POINTER_X[selected], POINTER_Y[selected]);
    int run = 0;
    for (int i = 0; i < 9; i++) run += g16(in->game, SEGMENTS + 2 * i);
    const int final = run == 9 || (run == 8 && !in->level);
    const int32_t bank = ops->load_art(ops->ctx, segment_mode ? "stats/sgmtstnd.qfs"
                                                 : final ? "stats/finstnd.qfs" : "stats/trmtstnd.qfs");
    st->inverse = ops->load_inverse(ops->ctx, "stats/standbst.inv");
    ops->music_feed(ops->ctx);
    const uint8_t *pal_shape = ops->shape(ops->ctx, bank, "!pal");
    palette_put(pal, 0, 0x100, pal_shape + 0x10, pal_shape[0], st->screen_palette);
    ops->palette_set(ops->ctx, 0, 0x100, pal);
    ops->inverse_build(ops->ctx);
    ops->text_styles(ops->ctx);
    ops->palette_write(ops->ctx, 0, 0x100, black);
    if (p->active) ops->pointer_poll(ops->ctx);
    while (!quit) {
        if (ops->pointer_in_rect(ops->ctx, 0, 0x82, 0x1b3, 0x1e0)) {
            if (selected != 0) selected = 0, moved = 1;
        } else if (selected != 1) {
            selected = 1, moved = 1;
        }
        if (redraw) {
            ops->select(ops->ctx, in->screen);
            if (first) {
                ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "bgnd"));
                if (selected == 0) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "Donl"));
                if (segment_mode) draw_segment(in, bank, segment, ops);
                else draw_totals(in, ops);
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
                ops->fade_in(ops->ctx, 0, 0x100, pal);
                first = 0;
            }
            ops->delay(ops->ctx, WAIT);
            redraw = 0, moved = 0;
        }
        if (moved) {
            ops->sound(ops->ctx, SOUND_MOVE);
            redraw = 1;
            ops->pointer_hide(ops->ctx);
            ops->select(ops->ctx, in->screen);
            if (selected == 0) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "Donl"));
            else if (selected == 1) ops->blit_masked(ops->ctx, ops->shape(ops->ctx, bank, "Dond"));
            ops->pointer_draw(ops->ctx);
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
            ops->ticks(ops->ctx);
            buttons_armed = 0, waited = 0;
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
        if (ops->deadline_passed(ops->ctx) || (p->buttons == 2 && buttons_armed) || in->unknown_50a5b6 == 1) {
            ops->sound(ops->ctx, SOUND_SELECT);
            quit = 1;
        }
    }
    ops->fade_out(ops->ctx, 0, 0x100, bank);
    ops->music_feed(ops->ctx);
    ops->free(ops->ctx, bank);
    ops->free_inverse(ops->ctx);
    ops->pointer_mark(ops->ctx, p);
}
