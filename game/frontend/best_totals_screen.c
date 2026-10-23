#include "game/frontend/best_totals_screen.h"

#include "game/frontend/records_route.h"

enum {
    TEXT = 0x5c, SOUND_MOVE = 0x47, SOUND_SELECT = 0x48, WAIT = 0x2ee0, KEY_SPACE = 0x20, KEY_ENTER = 0xd,
    KEY_ESCAPE = 0x1b, ROWS = 10, ROW_Y = 0x82, ROW_STEP = 0x15, TOP_Y = 0x177, NAME_X = 7, NAME_WIDTH = 0x11d,
    TIME_X = 0x131, CAR_X = 0x1d6, MARK_X = 0x23f
};

/* config.dat's image (from 00524df0): the race setup's shorts, then each
 * track's records (TRACK_RECORDS bytes a track) */
enum {
    TRACK_MUSIC = 0x7b, TRACK = 0x95, DIFFICULTY = 0xb3, TRACK_RECORDS = 0xa6b, BEST_STRIDE = 0x186,
    BEST_ROW = 0x27, BEST_NAME = 0x19f, BEST_CAR = 0x1ae, BEST_TOTAL = 0x1ba, BEST_MARK = 0x1c2, TOP_NAME = 0x631,
    TOP_CAR = 0x640, TOP_SPEED = 0x650, TOP_MARK = 0x654, TRACKS = 9
};

/* 0045b658: the tracks' pictures */
static const char *const TRACK_TAG[TRACKS] = { "city", "coas", "alpi", "rust", "autu", "burt", "mont", "trop", "neod" };
/* where the pointer goes for "Done" (0) and away from it (1): 0045b650 / 0045b654 */
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

/* a record's gearbox mark: 0 automatic, 1 manual, else none */
static void draw_mark(const ResultsInputs *in, int32_t mark, int32_t y, const ResultsOps *ops)
{
    if (mark != 0 && mark != 1) return;
    const char *text = in->german ? (mark ? "  d" : "  z") : (mark ? "h.h." : "t.t.");
    ops->text_at(ops->ctx, text, MARK_X, y);
}

static void draw_table(const ResultsInputs *in, int32_t bank, int16_t row, int16_t top_beaten, const ResultsOps *ops)
{
    char text[TEXT];
    const int16_t track = g16(in->image + TRACK);
    const uint8_t *records = in->image + TRACK_RECORDS * track;
    const uint8_t *table = records + (track > 2 ? BEST_STRIDE * g16(in->image + DIFFICULTY) : 0);
    ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "bgnd"));
    /* port guard: the original indexes its nine names with any short */
    if ((uint16_t)track < TRACKS) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, TRACK_TAG[track]));
    ops->text_select(ops->ctx, 1);
    for (int i = 0; i < ROWS; i++) {
        const uint8_t *best = table + BEST_ROW * i;
        const int32_t y = ROW_STEP * i + ROW_Y;
        if (i == row) ops->text_select(ops->ctx, 3);
        ops->text_fit(ops->ctx, (const char *)best + BEST_NAME, NAME_X, y, NAME_WIDTH);
        ops->time_text(ops->ctx, text, g32(best + BEST_TOTAL));
        ops->text_at(ops->ctx, text, TIME_X, y);
        ops->text_at(ops->ctx, in->car_names[(uint32_t)g32(best + BEST_CAR)], CAR_X, y);
        results_mark_total(in, i, y, ops);
        draw_mark(in, g32(best + BEST_MARK), y, ops);
        if (i == row) ops->text_select(ops->ctx, 1);
    }
    ops->text_select(ops->ctx, top_beaten ? 3 : 1);
    ops->text_fit(ops->ctx, (const char *)records + TOP_NAME, NAME_X, TOP_Y, NAME_WIDTH);
    ops->speed_text(ops->ctx, text, g32(records + TOP_SPEED));
    ops->text_at(ops->ctx, text, TIME_X, TOP_Y);
    ops->text_at(ops->ctx, in->car_names[(uint32_t)g32(records + TOP_CAR)], CAR_X, TOP_Y);
    results_mark_slot(in, SLOT_TOP, TOP_Y, ops);
    draw_mark(in, g32(records + TOP_MARK), TOP_Y, ops);
}

/* 0045c45c */
void best_totals_screen(int32_t row, int32_t top_beaten, const ResultsInputs *in, ResultsState *st, DialogPointer *p,
                        const ResultsOps *ops)
{
    int quit = 0, keys_armed = 1, selected = 0, redraw = 1, buttons_armed = 1, first = 1, moved = 0;
    uint32_t waited = 0;  /* the ticks summed, compared as a short */
    const int16_t place = (int16_t)row, top = (int16_t)top_beaten;
    uint8_t black[PALETTE_PUT_BYTES], pal[PALETTE_PUT_BYTES];
    ops->flush(ops->ctx);
    palette_put_clear(black);
    ops->pointer_move_to(ops->ctx, POINTER_X[selected], POINTER_Y[selected]);
    const int32_t bank = ops->load_art(ops->ctx, "stats/standbst.qfs");
    st->inverse = ops->load_inverse(ops->ctx, "stats/standbst.inv");
    ops->music_feed(ops->ctx);
    const uint8_t *pal_shape = ops->shape(ops->ctx, bank, "!pal");
    palette_put(pal, 0, 0x100, pal_shape + 0x10, pal_shape[0], st->screen_palette);
    ops->fade_in(ops->ctx, 0, 0x100, pal);
    ops->text_styles(ops->ctx);
    if (p->active) ops->pointer_poll(ops->ctx);
    const int fanfare = g16(in->image + TRACK_MUSIC) != 0 && place < ROWS;
    while (!quit) {
        if (ops->pointer_in_rect(ops->ctx, 0, 0x82, 0x1b3, 0x1e0)) {
            if (selected != 0) selected = 0, moved = 1;
        } else if (selected != 1) {
            selected = 1, moved = 1;
        }
        if (redraw) {
            ops->select(ops->ctx, in->screen);
            if (first) draw_table(in, bank, place, top, ops);
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
                if (fanfare) {
                    ops->track_music(ops->ctx, st->record_music ? (top ? "con4_G16.eas" : "con4_E16.eas")
                                                                : (top ? "con4_D16.eas" : "con4_A16.eas"));
                }
                ops->fade_in(ops->ctx, 0, 0x100, pal);
                first = 0;
                if (fanfare) ops->track_play(ops->ctx);
            }
            ops->delay(ops->ctx, WAIT);
            redraw = 0, moved = 0;
        }
        if (moved) {
            redraw = 1;
            ops->sound(ops->ctx, SOUND_MOVE);
            ops->pointer_hide(ops->ctx);
            ops->select(ops->ctx, in->screen);
            ops->select(ops->ctx, in->screen);
            if (selected == 0) ops->blit(ops->ctx, ops->shape(ops->ctx, bank, "donl"));
            else if (selected == 1) ops->blit_masked(ops->ctx, ops->shape(ops->ctx, bank, "dond"));
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
