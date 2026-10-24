/* The results screens with fake ops: the table with cheats is marked (the "Best Time" header, each record row's letters
 * after the car) and the table without is drawn as before (nothing extra). */
#include "game/frontend/best_totals_screen.h"
#include "game/frontend/circuit_results.h"
#include "game/frontend/results_screen.h"
#include "game/frontend/track_results.h"
#include "game/frontend/records_route.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdio.h>
#include <string.h>

/* the records and the extras are kept in memory here: no file is read or written */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path, (void)size; return NULL; }
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

enum { DRAWN_MAX = 512, TEXT_MAX = 40, X_MARKS = 0xd7, Y_SEGMENT = 0x136, Y_TOP = 0x17c, Y_BEST = 0x6e };
enum { CHEATS_BANNER_X = 0x122, CHEATS_BANNER_Y = 0x14 };   /* the header band, right of the logo */
enum { TITLE_X = 232, TITLE_BEST_Y = 283, TITLE_TOP_Y = 354 };   /* where a suffix on the segment screen's art titles would go */
enum { Y_BEST_ROW = 0x11d, Y_TRACK_TOP = 0x177, Y_HEAD = 0x5a, BEST_LEFT = 0x1d6, ROW_Y = 0x82, ROW_STEP = 0x15 };

typedef struct Drawn {
    char text[DRAWN_MAX][TEXT_MAX];
    int32_t x[DRAWN_MAX], y[DRAWN_MAX];
    int count;
} Drawn;

static Drawn drawn;
static uint8_t screen_palette[0x300];

static void f_nothing(void *ctx) { (void)ctx; }
static int32_t f_zero(void *ctx) { (void)ctx; return 0; }
static void f_text_at(void *ctx, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    if (drawn.count >= DRAWN_MAX) return;
    snprintf(drawn.text[drawn.count], TEXT_MAX, "%s", text);
    drawn.x[drawn.count] = x, drawn.y[drawn.count] = y;
    drawn.count++;
}
static void f_text_fit(void *ctx, const char *text, int32_t x, int32_t y, int32_t width)
{
    (void)width;
    f_text_at(ctx, text, x, y);
}
static void f_time_text(void *ctx, char *out, int32_t time) { (void)ctx; (void)time; snprintf(out, 0x5c, "t"); }
static void f_average_text(void *ctx, char *out, int32_t time, int32_t distance)
{
    (void)ctx, (void)time, (void)distance;
    snprintf(out, 0x5c, "a");
}
static void f_speed_text(void *ctx, char *out, int32_t speed) { (void)ctx; (void)speed; snprintf(out, 0x5c, "s"); }
static const uint8_t *f_shape(void *ctx, int32_t bank, const char *tag)
{
    static const uint8_t blank[0x400] = { 0 };
    (void)ctx, (void)bank, (void)tag;
    return blank;
}
static int32_t f_load(void *ctx, const char *name) { (void)ctx; (void)name; return 0; }
static int32_t f_key(void *ctx) { (void)ctx; return 0; }
static void f_sound(void *ctx, int32_t sound) { (void)ctx, (void)sound; }
static void f_move_to(void *ctx, int32_t x, int32_t y) { (void)ctx, (void)x, (void)y; }
static void f_fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)ctx, (void)first, (void)count, (void)rgb;
}
static void f_fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx, (void)first, (void)count; }
static void f_free(void *ctx, int32_t bank) { (void)ctx, (void)bank; }
static void f_blit(void *ctx, const uint8_t *shape) { (void)ctx, (void)shape; }
static void f_select(void *ctx, int32_t picture) { (void)ctx, (void)picture; }
static void f_text_select(void *ctx, int32_t style) { (void)ctx, (void)style; }
static void f_palette_write(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)ctx, (void)first, (void)count, (void)rgb;
}
static int32_t f_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    (void)ctx, (void)x0, (void)x1, (void)y0, (void)y1;
    return 0;
}
static void f_pointer_mark(void *ctx, DialogPointer *p) { (void)ctx, (void)p; }
static void f_track_music(void *ctx, const char *name) { (void)ctx, (void)name; }
static void f_best_totals(void *ctx, int32_t row, int32_t top) { (void)ctx, (void)row, (void)top; }
static void f_blit_word(void *ctx, int32_t shape) { (void)ctx, (void)shape; }
static void f_results_order(void *ctx) { (void)ctx; }
static int32_t f_lap_times(void *ctx, int32_t record, int32_t tune) { (void)ctx, (void)record, (void)tune; return 0; }
static int32_t f_deadline(void *ctx) { (void)ctx; return 1; }

/* the screens' ops: nothing but the text is recorded; the deadline ends each screen after its first draw */
static ResultsOps fake_ops(void)
{
    ResultsOps ops;
    memset(&ops, 0, sizeof ops);
    ops.records = (int32_t (*)(void *))f_zero;
    ops.flush = (void (*)(void *))f_nothing;
    ops.pointer_move_to = f_move_to;
    ops.load_art = f_load;
    ops.load_inverse = f_load;
    ops.music_feed = (void (*)(void *))f_nothing;
    ops.shape = f_shape;
    ops.fade_in = f_fade_in;
    ops.text_styles = (void (*)(void *))f_nothing;
    ops.pointer_poll = (void (*)(void *))f_nothing;
    ops.pointer_in_rect = f_in_rect;
    ops.select = f_select;
    ops.blit = f_blit;
    ops.text_select = f_text_select;
    ops.text_at = f_text_at;
    ops.text_fit = f_text_fit;
    ops.time_text = f_time_text;
    ops.average_text = f_average_text;
    ops.speed_text = f_speed_text;
    ops.palette_write = f_palette_write;
    ops.pointer_draw = (void (*)(void *))f_nothing;
    ops.select_default = (void (*)(void *))f_nothing;
    ops.pointer_hide = (void (*)(void *))f_nothing;
    ops.lock = (void (*)(void *))f_nothing;
    ops.blit_word = f_blit_word;
    ops.unlock = (void (*)(void *))f_nothing;
    ops.flip = (void (*)(void *))f_nothing;
    ops.pointer_show = (void (*)(void *))f_nothing;
    ops.pointer_mark = f_pointer_mark;
    ops.pointer_buttons = f_zero;
    ops.track_music = f_track_music;
    ops.track_play = (void (*)(void *))f_nothing;
    ops.delay = f_sound;
    ops.sound = f_sound;
    ops.ticks = f_zero;
    ops.pointer_activate_if_moved = (void (*)(void *))f_nothing;
    ops.key = f_key;
    ops.pointer_click = (void (*)(void *))f_nothing;
    ops.pointer_off = (void (*)(void *))f_nothing;
    ops.music_service = (void (*)(void *))f_nothing;
    ops.deadline_passed = f_deadline;
    ops.track_stop = (void (*)(void *))f_nothing;
    ops.music_free = (void (*)(void *))f_nothing;
    ops.fade_out = f_fade_out;
    ops.free = f_free;
    ops.free_inverse = (void (*)(void *))f_nothing;
    ops.blit_masked = f_blit;
    ops.totals = f_zero;
    ops.top_record = f_zero;
    ops.leads = f_zero;
    ops.best_totals = f_best_totals;
    ops.lap_record = f_zero;
    ops.results_order = f_results_order;
    ops.lap_times = f_lap_times;
    return ops;
}

static const char *const CAR_NAMES[RESULTS_CARS] = { "RX-7", "NSX", "SUPRA", "911", "ZR-1", "VIPER",
                                                     "512TR", "DIABLO", "PACK", "CLOCK", "CLASS", "WARRIOR" };

/* a time trial on City's first segment, the segment's record and top speed set by a cheats race (the marks in slots
 * the records route writes), the best row on the table (laps choice 0) */
enum { TRACK_WORD = 0x95, SEGMENT_WORD = 0x93, DIFFICULTY_WORD = 0xb3, GAME_RACED = 0x5ba };

static void setup_inputs(ResultsInputs *in, uint8_t *game, uint8_t *image, const ResultsMarks *marks)
{
    static const int16_t dropped[2] = { 0, 0 };
    memset(in, 0, sizeof *in);
    memset(game, 0, RESULTS_GAME);
    memset(image, 0, RESULTS_IMAGE);
    game[GAME_RACED] = 1;
    image[SEGMENT_WORD] = 1;
    in->game = game, in->image = image, in->dropped = dropped, in->player = 0, in->car_names = CAR_NAMES;
    in->screen = 0, in->screen_shape = 0, in->marks = marks;
    in->place = 0, in->rival_place = 1, in->rival = 1;
    in->finish = NULL;
}

static void reset(void)
{
    memset(&drawn, 0, sizeof drawn);
}

static int found(const char *text, int32_t x, int32_t y)
{
    for (int i = 0; i < drawn.count; i++)
        if (strcmp(drawn.text[i], text) == 0 && drawn.x[i] == x && drawn.y[i] == y) return 1;
    return 0;
}

static int any_text(const char *text)
{
    for (int i = 0; i < drawn.count; i++)
        if (strcmp(drawn.text[i], text) == 0) return 1;
    return 0;
}

static int any_with_suffix(const char *suffix)
{
    for (int i = 0; i < drawn.count; i++) {
        const size_t n = strlen(drawn.text[i]), m = strlen(suffix);
        if (n >= m && strcmp(drawn.text[i] + n - m, suffix) == 0) return 1;
    }
    return 0;
}

static void run_segment(const ResultsMarks *marks)
{
    static uint8_t game[RESULTS_GAME], image[RESULTS_IMAGE];
    ResultsInputs in;
    ResultsState st = { 0, screen_palette, 0 };
    DialogPointer p;
    ResultsOps ops = fake_ops();
    setup_inputs(&in, game, image, marks);
    memset(&p, 0, sizeof p);
    reset();
    results_screen(0, &in, &st, &p, &ops);
}

static void test_segment_normal_table_is_unchanged(void)
{
    run_segment(NULL);
    CHECK(any_text("Best Time"));
    CHECK(!any_with_suffix("(cheats)"));
    CHECK(!found(" (cheats)", TITLE_X, TITLE_BEST_Y));
    CHECK(!found("N", X_MARKS, Y_SEGMENT));
    CHECK(!any_text("(CHEATS)"));
}

/* the "(CHEATS)" mark sits in the header band, right of the Road & Track logo, on every segment screen of a race with
 * cheats (the table's marks or not) */
static void test_segment_cheats_banner_on_any_cheats_race(void)
{
    static uint8_t slots[RECORDS_MARK_SLOTS];
    ResultsMarks marks = { slots, 0 };
    memset(slots, 0, sizeof slots);
    run_segment(&marks);
    CHECK(found("(CHEATS)", CHEATS_BANNER_X, CHEATS_BANNER_Y));
}

/* the segment's top speed row is the segment's own record (SLOT_SPEED_SEGMENT), not the circuit's top (SLOT_TOP) */
static void test_segment_speed_row_marks_come_from_its_own_slot(void)
{
    static uint8_t slots[RECORDS_MARK_SLOTS];
    ResultsMarks marks = { slots, 0 };
    memset(slots, 0, sizeof slots);
    slots[SLOT_TOP] = EXTRAS_MARK_TURBO;
    run_segment(&marks);
    CHECK(!found("T", X_MARKS, Y_TOP));
    memset(slots, 0, sizeof slots);
    slots[SLOT_SPEED_SEGMENT + 0] = EXTRAS_MARK_TURBO;
    run_segment(&marks);
    CHECK(found("T", X_MARKS, Y_TOP));
}

static void test_segment_cheats_title_and_marks(void)
{
    static uint8_t slots[RECORDS_MARK_SLOTS];
    ResultsMarks marks = { slots, 0 };
    memset(slots, 0, sizeof slots);
    slots[SLOT_SEGMENT + 0] = EXTRAS_MARK_NITRO;
    slots[SLOT_SPEED_SEGMENT + 0] = EXTRAS_MARK_TURBO | EXTRAS_MARK_WARRIOR;
    slots[records_slot_totals(0, 0, 0)] = EXTRAS_MARK_EXPLODE;
    run_segment(&marks);
    CHECK(any_text("Best Time (c)"));
    CHECK(found("Best Time (c)", BEST_LEFT, Y_HEAD));
    CHECK(!found(" (cheats)", TITLE_X, TITLE_BEST_Y));   /* the header and the rows mark the table, not the art titles */
    CHECK(!found(" (cheats)", TITLE_X, TITLE_TOP_Y));
    CHECK(found("N", X_MARKS, Y_SEGMENT));
    CHECK(found("TW", X_MARKS, Y_TOP));
    CHECK(found("(CHEATS)", CHEATS_BANNER_X, CHEATS_BANNER_Y));
}

static void test_segment_no_mark_when_slot_is_clear(void)
{
    static uint8_t slots[RECORDS_MARK_SLOTS];
    ResultsMarks marks = { slots, 0 };
    memset(slots, 0, sizeof slots);
    run_segment(&marks);
    CHECK(any_text("Best Time (c)"));
    CHECK(!found("N", X_MARKS, Y_SEGMENT));
    CHECK(!found("TW", X_MARKS, Y_TOP));
}

static void test_best_totals_marks_each_row(void)
{
    static uint8_t game[RESULTS_GAME], image[RESULTS_IMAGE];
    static uint8_t slots[RECORDS_MARK_SLOTS];
    ResultsMarks marks = { slots, 0 };
    ResultsInputs in;
    ResultsState st = { 0, screen_palette, 0 };
    DialogPointer p;
    ResultsOps ops = fake_ops();
    memset(slots, 0, sizeof slots);
    slots[records_slot_totals(0, 0, 3)] = EXTRAS_MARK_NITRO;
    slots[SLOT_TOP] = EXTRAS_MARK_TURBO;
    setup_inputs(&in, game, image, &marks);
    memset(&p, 0, sizeof p);
    reset();
    best_totals_screen(0, 0, &in, &st, &p, &ops);
    CHECK(!any_with_suffix("(cheats)"));
    CHECK(found("N", X_MARKS, ROW_STEP * 3 + ROW_Y));
    CHECK(found("T", X_MARKS, Y_TRACK_TOP));
    CHECK(!found("E", X_MARKS, ROW_Y));
}

static void test_best_totals_normal_table_has_no_marks(void)
{
    static uint8_t game[RESULTS_GAME], image[RESULTS_IMAGE];
    ResultsInputs in;
    ResultsState st = { 0, screen_palette, 0 };
    DialogPointer p;
    ResultsOps ops = fake_ops();
    setup_inputs(&in, game, image, NULL);
    memset(&p, 0, sizeof p);
    reset();
    best_totals_screen(0, 0, &in, &st, &p, &ops);
    CHECK(!any_with_suffix("(cheats)"));
    CHECK(!found("N", X_MARKS, ROW_Y));
}

static void test_track_results_header_and_best_row(void)
{
    static uint8_t game[RESULTS_GAME], image[RESULTS_IMAGE];
    static uint8_t slots[RECORDS_MARK_SLOTS];
    ResultsMarks marks = { slots, 0 };
    ResultsInputs in;
    ResultsState st = { 0, screen_palette, 0 };
    DialogPointer p;
    ResultsOps ops = fake_ops();
    memset(slots, 0, sizeof slots);
    slots[records_slot_totals(0, 0, 0)] = EXTRAS_MARK_WARRIOR;
    setup_inputs(&in, game, image, &marks);
    memset(&p, 0, sizeof p);
    reset();
    track_results_screen(0, &in, &st, &p, &ops);
    CHECK(any_text("Best Time (c)"));
    CHECK(found("W", X_MARKS, Y_BEST_ROW));
}

static void test_track_results_normal_table_has_no_marks(void)
{
    static uint8_t game[RESULTS_GAME], image[RESULTS_IMAGE];
    ResultsInputs in;
    ResultsState st = { 0, screen_palette, 0 };
    DialogPointer p;
    ResultsOps ops = fake_ops();
    setup_inputs(&in, game, image, NULL);
    memset(&p, 0, sizeof p);
    reset();
    track_results_screen(0, &in, &st, &p, &ops);
    CHECK(any_text("Best Time"));
    CHECK(!any_with_suffix("(cheats)"));
}

static void test_circuit_results_header_and_best_row(void)
{
    static uint8_t game[RESULTS_GAME], image[RESULTS_IMAGE];
    static uint8_t slots[RECORDS_MARK_SLOTS];
    ResultsMarks marks = { slots, 1 };
    ResultsInputs in;
    ResultsState st = { 0, screen_palette, 0 };
    DialogPointer p;
    ResultsOps ops = fake_ops();
    memset(slots, 0, sizeof slots);
    slots[records_slot_totals(3, 1, 0)] = EXTRAS_MARK_NITRO | EXTRAS_MARK_EXPLODE;
    setup_inputs(&in, game, image, &marks);
    image[TRACK_WORD] = 3, image[DIFFICULTY_WORD] = 1;
    memset(&p, 0, sizeof p);
    reset();
    circuit_results_screen(0, &in, &st, &p, &ops);
    CHECK(any_text("Best Time (c)"));
    CHECK(found("NE", X_MARKS, Y_BEST_ROW));
}

int main(void)
{
    test_segment_normal_table_is_unchanged();
    test_segment_cheats_title_and_marks();
    test_segment_no_mark_when_slot_is_clear();
    test_segment_cheats_banner_on_any_cheats_race();
    test_segment_speed_row_marks_come_from_its_own_slot();
    test_best_totals_marks_each_row();
    test_best_totals_normal_table_has_no_marks();
    test_track_results_header_and_best_row();
    test_track_results_normal_table_has_no_marks();
    test_circuit_results_header_and_best_row();
    return TEST_REPORT();
}
