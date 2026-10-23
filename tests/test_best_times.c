/* The best times screen (best_times.c) with fake ops: the footer's cheats item (port), its label and the table it shows.
 * The screen runs its loop on a key script; once the script is spent the deadline ends it. */
#include "game/frontend/best_times.h"
#include "game/frontend/track_select.h"
#include "game/frontend/yes_no_box.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdio.h>
#include <string.h>

/* the records' tables are kept in memory here: no file is read or written */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

enum { ENTER = 0xd, ESCAPE = 0x1b, LEFT = 0x4b00, RIGHT = 0x4d00, UP = 0x4800, DOWN = 0x5000, TAB = 0x0f };
enum { STYLE_CREAM = 0, STYLE_YELLOW = 1 };
enum { DRAWN_MAX = 256, TEXT_MAX = 40 };
/* the footer's cheats item: its pointer, the label's centre and text top (best_times.c), and the rect the pointer finds
 * it in. Done's words end at 76 and the left arrow starts at 221; Done's capitals are rows 449..460, so the label's
 * capitals (which sit 6 rows below its top) must be at the same baseline. */
enum { CHEATS_POINTER_X = 148, CHEATS_POINTER_Y = 455, CHEATS_RECT_X0 = 130, CHEATS_RECT_X1 = 200 };
enum { CHEATS_TEXT_TOP = 443, DONE_RIGHT = 76, ARROW_LEFT = 221, MIN_GAP = 12 };
/* the words' widths as the game's font sets them (measured on the capture, 2026-10-23): the gaps left are 33 and 34
 * for "Cheats" and 14 and 15 for "No cheats" */
enum { CHEATS_WIDTH = 78, NO_CHEATS_WIDTH = 116 };

typedef struct Fake {
    const int *keys;
    int key_count, key_next;
    int calls;
    int tab_at;              /* the key call on which Tab is pressed (-1: never) */
    int tab_pending;
    char labels[DRAWN_MAX][TEXT_MAX];
    int32_t label_style[DRAWN_MAX];
    int32_t label_x[DRAWN_MAX], label_y[DRAWN_MAX];
    int label_count;
    char shadowed[DRAWN_MAX][TEXT_MAX];
    int shadowed_count;
    int32_t pointer_x, pointer_y;
    int pointer_on;
    int active;              /* the pointer is the player's (DialogPointer.active) */
    int sounds;
} Fake;

static Fake fk;

/* the pointer goes where the screen moves it, and is off the footer after pointer_off (step's arrows) */
static void f_move_to(void *ctx, int32_t x, int32_t y)
{
    (void)ctx;
    fk.pointer_on = 1, fk.pointer_x = x, fk.pointer_y = y;
}
static void f_pointer_off(void *ctx)
{
    (void)ctx;
    fk.pointer_on = 0;
}
static int32_t f_load_art(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
static int32_t f_load_inverse(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
static void f_nothing(void *ctx) { (void)ctx; }
/* a picture as the bank gives it, big enough for the palette read from "!pal" (a count byte, then 0x10 header bytes) */
static const uint8_t *f_shape(void *ctx, int32_t bank, const char *tag)
{
    static uint8_t shape[0x400];
    (void)ctx, (void)bank, (void)tag;
    return shape;
}
static void f_palette_set(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)ctx, (void)first, (void)count, (void)rgb;
}
static void f_pointer_mark(void *ctx, DialogPointer *p) { (void)ctx, (void)p; }
static int32_t f_pointer_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    (void)ctx;
    return fk.pointer_on && fk.pointer_x >= x0 && fk.pointer_x < x1 && fk.pointer_y >= y0 && fk.pointer_y < y1;
}
static void f_select(void *ctx, int32_t picture) { (void)ctx, (void)picture; }
static void f_blit(void *ctx, const uint8_t *shape) { (void)ctx, (void)shape; }
static void f_blit_word(void *ctx, int32_t shape) { (void)ctx, (void)shape; }
static void f_text_select(void *ctx, int32_t style) { (void)ctx, (void)style; }
static void f_text_fit(void *ctx, const char *text, int32_t x, int32_t y, int32_t width)
{
    (void)ctx, (void)text, (void)x, (void)y, (void)width;
}
static void f_time_text(void *ctx, char *out, int32_t time) { (void)ctx, (void)time; strcpy(out, "0"); }
static void f_speed_text(void *ctx, char *out, int32_t speed) { (void)ctx, (void)speed; strcpy(out, "0"); }
static void f_text_shadowed(void *ctx, const char *text, int32_t x, int32_t y)
{
    (void)ctx, (void)x, (void)y;
    if (fk.shadowed_count < DRAWN_MAX) snprintf(fk.shadowed[fk.shadowed_count++], TEXT_MAX, "%s", text);
}
static int32_t f_pointer_buttons(void *ctx) { (void)ctx; return 0; }
static void f_fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)ctx, (void)first, (void)count, (void)rgb;
}
static void f_deadline_set(void *ctx, int32_t time) { (void)ctx, (void)time; }
static void f_sound(void *ctx, int32_t sound) { (void)ctx, (void)sound; fk.sounds++; }
static int32_t f_ticks(void *ctx) { (void)ctx; return 100; }
static int32_t f_key(void *ctx)
{
    (void)ctx;
    fk.calls++;
    if (fk.key_next < fk.key_count) return fk.keys[fk.key_next++];
    return 0;
}
static void f_pointer_click(void *ctx) { (void)ctx; }
static void f_car_flags(void *ctx) { (void)ctx; }
static void f_fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx, (void)first, (void)count; }
static void f_free(void *ctx, int32_t bank) { (void)ctx, (void)bank; }
static int32_t f_load_bank(void *ctx, const char *path, int32_t flags)
{
    (void)ctx, (void)path, (void)flags;
    return 1;
}
static int32_t f_deadline_passed(void *ctx)
{
    (void)ctx;
    return fk.key_next >= fk.key_count && fk.calls > 0;
}
static int32_t f_plus_toggle(void *ctx)
{
    (void)ctx;
    if (fk.calls - 1 == fk.tab_at && !fk.tab_pending) return fk.tab_pending = 1;
    return 0;
}
static void f_plus_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    if (fk.label_count >= DRAWN_MAX) return;
    snprintf(fk.labels[fk.label_count], TEXT_MAX, "%s", text);
    fk.label_style[fk.label_count] = style;
    fk.label_x[fk.label_count] = x, fk.label_y[fk.label_count] = y;
    fk.label_count++;
}
static int32_t f_plus_text_width(void *ctx, const char *text)
{
    (void)ctx;
    if (strcmp(text, "Cheats") == 0) return CHEATS_WIDTH;
    if (strcmp(text, "No cheats") == 0) return NO_CHEATS_WIDTH;
    return (int32_t)strlen(text) * 8;
}

/* the label's gaps: clear of Done's words on the left and of the left arrow on the right, by at least MIN_GAP */
static void check_gaps(int i, int32_t width)
{
    CHECK(fk.label_x[i] - DONE_RIGHT >= MIN_GAP);
    CHECK(ARROW_LEFT - (fk.label_x[i] + width) >= MIN_GAP);
    CHECK_EQ_INT(fk.label_y[i], CHEATS_TEXT_TOP);
}

static const BestTimesOps OPS = {
    .pointer_move_to = f_move_to,
    .load_art = f_load_art,
    .load_inverse = f_load_inverse,
    .music_feed = f_nothing,
    .shape = f_shape,
    .flush = f_nothing,
    .palette_set = f_palette_set,
    .inverse_build = f_nothing,
    .text_styles = f_nothing,
    .pointer_poll = f_nothing,
    .pointer_mark = f_pointer_mark,
    .pointer_in_rect = f_pointer_in_rect,
    .select = f_select,
    .blit = f_blit,
    .blit_masked = f_blit,
    .blit_word = f_blit_word,
    .text_select = f_text_select,
    .text_fit = f_text_fit,
    .time_text = f_time_text,
    .speed_text = f_speed_text,
    .text_shadowed = f_text_shadowed,
    .select_default = f_nothing,
    .pointer_hide = f_nothing,
    .lock = f_nothing,
    .unlock = f_nothing,
    .flip = f_nothing,
    .pointer_show = f_nothing,
    .pointer_buttons = f_pointer_buttons,
    .fade_in = f_fade_in,
    .deadline_set = f_deadline_set,
    .sound = f_sound,
    .ticks = f_ticks,
    .pointer_activate_if_moved = f_nothing,
    .key = f_key,
    .pointer_click = f_pointer_click,
    .car_flags = f_car_flags,
    .fade_out = f_fade_out,
    .free_inverse = f_nothing,
    .free = f_free,
    .load_bank = f_load_bank,
    .pointer_off = f_pointer_off,
    .music = f_nothing,
    .deadline_passed = f_deadline_passed,
    .music_fade_out = f_nothing,
    .music_end = f_nothing,
    .plus_toggle = f_plus_toggle,
    .plus_text = f_plus_text,
    .plus_text_width = f_plus_text_width,
};

/* a fresh run: the keys to press (the call on which Tab goes down, or -1), the table shown first */
static void reset(const int *keys, int count, int tab_at)
{
    memset(&fk, 0, sizeof fk);
    fk.keys = keys, fk.key_count = count, fk.tab_at = tab_at;
}

/* the screen on track 0 with empty tables: the plain one and, with a plus, the one with cheats shown first or not */
static int32_t run(BestTimesPlus *plus, int cheats)
{
    static uint8_t config[TRACK_SELECT_CONFIG];
    static uint8_t screen_palette[0x300];
    static uint8_t records[BEST_TIMES_RECORDS];
    static uint8_t cheat_records[BEST_TIMES_RECORDS];
    const BestTimesInputs in = { "", 0, 0, 0, 0, records };
    BestTimesState st = { config, 0, screen_palette, plus };
    DialogPointer p = { 0 };
    p.active = fk.active;   /* an active pointer keeps where the test put it (no move to Back at the start) */
    memset(config, 0, sizeof config);
    if (plus) {
        plus->cheats = cheats;
        plus->records[0] = records, plus->records[1] = cheat_records;
        for (int t = 0; t < 9; t++) plus->marks[t] = NULL;
    }
    return best_times_screen(&in, &st, &p, &OPS);
}

static int label_index(const char *text)
{
    for (int i = fk.label_count - 1; i >= 0; i--)
        if (strcmp(fk.labels[i], text) == 0) return i;
    return -1;
}

/* without a plus the screen is the original: no label, and Right from Back goes to Done as before */
static void test_no_plus_is_original(void)
{
    static const int keys[] = { RIGHT, ENTER };
    reset(keys, 2, -1);
    CHECK_EQ_INT(run(NULL, 0), 1);
    CHECK_EQ_INT(fk.label_count, 0);
}

/* with a plus the label shows what the Tab would show: "Cheats" while the plain table is up */
static void test_label_says_cheats_when_plain(void)
{
    static const int keys[] = { ESCAPE };
    BestTimesPlus plus;
    reset(keys, 1, -1);
    CHECK_EQ_INT(run(&plus, 0), 0);
    const int i = label_index("Cheats");
    CHECK(i >= 0);
    if (i >= 0) {
        CHECK_EQ_INT(fk.label_style[i], STYLE_CREAM);
        check_gaps(i, CHEATS_WIDTH);
    }
}

/* the label changes with the table: "No cheats" once the cheats table is up, and that table's name is drawn */
static void test_label_says_no_cheats_when_cheats(void)
{
    static const int keys[] = { ESCAPE };
    BestTimesPlus plus;
    reset(keys, 1, -1);
    CHECK_EQ_INT(run(&plus, 1), 0);
    const int i = label_index("No cheats");
    CHECK(i >= 0);
    if (i >= 0) check_gaps(i, NO_CHEATS_WIDTH);
    CHECK(label_index("Cheats") < 0);
    int named = 0;
    for (int i = 0; i < fk.shadowed_count; i++) named |= strcmp(fk.shadowed[i], "Records (cheats)") == 0;
    CHECK(named);
}

/* Tab still toggles, and the label follows the table (an idle frame passes so the screen draws the change) */
static void test_tab_toggles_label(void)
{
    static const int keys[] = { 0, ESCAPE };
    BestTimesPlus plus;
    reset(keys, 2, 0);
    CHECK_EQ_INT(run(&plus, 0), 0);
    CHECK_EQ_INT(plus.cheats, 1);
    CHECK(label_index("No cheats") >= 0);
}

/* the arrows reach the cheats item from Done (Right), it lights in yellow, and Enter toggles the table */
static void test_arrows_reach_cheats_and_enter_toggles(void)
{
    static const int keys[] = { RIGHT, RIGHT, ENTER, ESCAPE };
    BestTimesPlus plus;
    reset(keys, 4, -1);
    CHECK_EQ_INT(run(&plus, 0), 0);
    CHECK_EQ_INT(plus.cheats, 1);
    int lit = 0;
    for (int i = 0; i < fk.label_count; i++) lit |= fk.label_style[i] == STYLE_YELLOW;
    CHECK(lit);
}

/* the pointer over the item lights it; Enter then acts on it */
static void test_pointer_lights_cheats_item(void)
{
    static const int keys[] = { ENTER, ESCAPE };
    BestTimesPlus plus;
    reset(keys, 2, -1);
    fk.active = 1, fk.pointer_on = 1, fk.pointer_x = CHEATS_POINTER_X, fk.pointer_y = CHEATS_POINTER_Y;
    CHECK_EQ_INT(run(&plus, 0), 0);
    CHECK_EQ_INT(plus.cheats, 1);
    CHECK(CHEATS_RECT_X0 < CHEATS_POINTER_X && CHEATS_POINTER_X < CHEATS_RECT_X1);
}

/* Left from the left arrow steps onto the cheats item (lit), and Enter toggles it */
static void test_left_from_arrow_reaches_cheats(void)
{
    static const int keys[] = { LEFT, LEFT, LEFT, ENTER, ESCAPE };   /* back, next, the left arrow, cheats */
    BestTimesPlus plus;
    reset(keys, 5, -1);
    CHECK_EQ_INT(run(&plus, 0), 0);
    CHECK_EQ_INT(plus.cheats, 1);
}

/* Up and Down on the cheats item toggle the table too (the item is lit when they are pressed) */
static void test_up_and_down_toggle_on_cheats(void)
{
    static const int keys[] = { RIGHT, RIGHT, UP, DOWN, ESCAPE };
    BestTimesPlus plus;
    reset(keys, 5, -1);
    CHECK_EQ_INT(run(&plus, 0), 0);
    CHECK_EQ_INT(plus.cheats, 0);   /* up, then down: back where it was */
    static const int once[] = { RIGHT, RIGHT, DOWN, ESCAPE };
    reset(once, 4, -1);
    CHECK_EQ_INT(run(&plus, 0), 0);
    CHECK_EQ_INT(plus.cheats, 1);
}

/* the lit label is the last one drawn in yellow once the item is selected */
static void test_lit_label_is_yellow(void)
{
    static const int keys[] = { RIGHT, RIGHT, ESCAPE };
    BestTimesPlus plus;
    reset(keys, 3, -1);
    CHECK_EQ_INT(run(&plus, 0), 0);
    CHECK(fk.label_count > 0);
    if (fk.label_count > 0) CHECK_EQ_INT(fk.label_style[fk.label_count - 1], STYLE_YELLOW);
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

int main(void)
{
    test_no_plus_is_original();
    test_label_says_cheats_when_plain();
    test_label_says_no_cheats_when_cheats();
    test_tab_toggles_label();
    test_arrows_reach_cheats_and_enter_toggles();
    test_pointer_lights_cheats_item();
    test_left_from_arrow_reaches_cheats();
    test_up_and_down_toggle_on_cheats();
    test_lit_label_is_yellow();
    return TEST_REPORT();
}
