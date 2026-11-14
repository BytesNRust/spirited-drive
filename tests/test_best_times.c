/* The best times screen (best_times.c) with fake ops: without a plus the exe's screen; with one (Restomod's records) the strip of
 * the table shown and its page, the keys through the strip's fields and the footer, the rows with their cheats and conditions.
 * The screen runs its loop on a key script; once the script is spent the deadline ends it. */
#include "game/frontend/best_times.h"
#include "game/frontend/track_select.h"
#include "game/frontend/yes_no_box.h"
#include "game/frontend/best_times_pages.h"
#include "game/race/weather.h"
#include "game/sys/extras_marks.h"
#include "game/sys/port_settings.h"
#include "game/sys/race_records.h"
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
bool platform_user_file_exists(const char *name)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(name, &size);
    if (data != NULL) platform_free_file(data);
    return data != NULL;
}

enum { ENTER = 0xd, ESCAPE = 0x1b, LEFT = 0x4b00, RIGHT = 0x4d00, UP = 0x4800, DOWN = 0x5000, TAB = 9, PAGE_UP = 0x4900, SHIFT_NOTHING = 0 };
enum { STYLE_CREAM = 0, STYLE_YELLOW = 1 };
enum { DRAWN_MAX = 256, TEXT_MAX = 40 };

typedef struct Fake {
    const int *keys;
    int key_count, key_next;
    int calls;
    char labels[DRAWN_MAX][TEXT_MAX];
    int32_t label_style[DRAWN_MAX];
    int32_t label_x[DRAWN_MAX], label_y[DRAWN_MAX];
    int label_count;
    char shadowed[DRAWN_MAX][TEXT_MAX];
    int shadowed_count;
    char fitted[DRAWN_MAX][TEXT_MAX];   /* the names the screen fits (the rows' first words) */
    int fitted_count;
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
    (void)ctx, (void)x, (void)y, (void)width;
    if (fk.fitted_count < DRAWN_MAX) snprintf(fk.fitted[fk.fitted_count++], TEXT_MAX, "%s", text);
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
    return (int32_t)strlen(text) * 8;
}
static void f_plus_scaled(void *ctx, int32_t style, const char *text, int32_t x, int32_t y, int32_t num, int32_t den)
{
    (void)num, (void)den;
    f_plus_text(ctx, style, text, x, y);
}
static int32_t f_plus_scaled_width(void *ctx, const char *text, int32_t num, int32_t den)
{
    (void)ctx;
    return (int32_t)strlen(text) * 8 * num / den;
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
    .plus_text = f_plus_text,
    .plus_text_width = f_plus_text_width,
    .plus_scaled = f_plus_scaled,
    .plus_scaled_width = f_plus_scaled_width,
};

enum { STAGE = 1, CIRCUIT = 3 };   /* Coastal; Rusty Springs (4, 8, 16 laps) */

static void reset(const int *keys, int count)
{
    memset(&fk, 0, sizeof fk);
    fk.keys = keys, fk.key_count = count;
}

/* the screen on `track`, the menu's laps choice 1 */
static int32_t run(BestTimesPlus *plus, int track)
{
    static uint8_t config[TRACK_SELECT_CONFIG];
    static uint8_t screen_palette[0x300];
    static uint8_t records[BEST_TIMES_RECORDS];
    const BestTimesInputs in = { "", 0, 0, 0, 0, records };
    BestTimesState st = { config, 0, screen_palette, plus };
    DialogPointer p = { 0 };
    p.active = fk.active;
    memset(config, 0, sizeof config);
    config[TRACK_SELECT_TRACK] = (uint8_t)track;
    config[TRACK_SELECT_LAPS_CHOICE] = 1;
    return best_times_screen(&in, &st, &p, &OPS);
}

static int was_drawn(const char *text)
{
    for (int i = 0; i < fk.label_count; i++)
        if (strcmp(fk.labels[i], text) == 0) return 1;
    for (int i = 0; i < fk.shadowed_count; i++)
        if (strcmp(fk.shadowed[i], text) == 0) return 1;
    for (int i = 0; i < fk.fitted_count; i++)
        if (strcmp(fk.fitted[i], text) == 0) return 1;
    return 0;
}

/* without a plus the screen is the original: no words of the port's, and Right from Back goes to Done as before */
static void test_no_plus_is_original(void)
{
    static const int keys[] = { RIGHT, ENTER };
    reset(keys, 2);
    CHECK_EQ_INT(run(NULL, CIRCUIT), 1);
    CHECK_EQ_INT(fk.label_count, 0);
}

/* the strip names the table shown and the page; an empty table says so, the top speed's row too */
static void test_the_strip_names_the_table(void)
{
    race_records_reset();
    BestTimesPlus plus;
    best_times_pages_open(&plus, CIRCUIT, LAYOUT_MIRROR | LAYOUT_RALLY, 0, 0, 1);
    static const int keys[] = { SHIFT_NOTHING };
    reset(keys, 1);
    (void)run(&plus, CIRCUIT);
    CHECK(was_drawn("MIRROR + RALLY"));
    CHECK(was_drawn("NO CHEATS"));
    CHECK(was_drawn("8 LAPS"));
    CHECK(was_drawn("TOP 10"));
    CHECK(was_drawn("NO RECORDS"));
    CHECK(!was_drawn("PAGE 1/2:"));
    /* a stage: open road, no laps */
    best_times_pages_open(&plus, STAGE, 0, 1, 0, 1);
    reset(keys, 1);
    (void)run(&plus, STAGE);
    CHECK(was_drawn("NORMAL"));
    CHECK(was_drawn("CHEATS"));
    CHECK(was_drawn("OPEN ROAD"));
    CHECK(!was_drawn("8 LAPS"));
}

/* Tab and Page Down turn the page on, Page Up back, round both ways: a circuit's best lap, a stage's segments */
static void test_tab_turns_the_page(void)
{
    race_records_reset();
    BestTimesPlus plus;
    best_times_pages_open(&plus, CIRCUIT, 0, 0, 0, 1);
    static const int one[] = { TAB, SHIFT_NOTHING };
    reset(one, 2);
    (void)run(&plus, CIRCUIT);
    CHECK_EQ_INT(plus.page, 1);
    CHECK(was_drawn("BEST LAP"));
    static const int back[] = { PAGE_UP, PAGE_UP, SHIFT_NOTHING };
    reset(back, 3);
    (void)run(&plus, CIRCUIT);
    CHECK_EQ_INT(plus.page, 1);   /* 1 -> 0 -> 1 round */
    best_times_pages_open(&plus, STAGE, 0, 0, 0, 1);
    reset(one, 2);
    (void)run(&plus, STAGE);
    CHECK(was_drawn("SEGMENTS"));
    CHECK(was_drawn("SEGMENT 3"));
}

/* Up from the footer climbs the strip (page, laps, cheats, layout); Left and Right step the field the keys are on; Enter steps it on */
static void test_the_keys_step_the_fields(void)
{
    race_records_reset();
    BestTimesPlus plus;
    best_times_pages_open(&plus, CIRCUIT, 0, 0, 0, 1);
    static const int keys[] = { UP, RIGHT, UP, LEFT, UP, RIGHT, UP, LEFT, SHIFT_NOTHING };
    reset(keys, sizeof keys / sizeof keys[0]);
    (void)run(&plus, CIRCUIT);
    CHECK_EQ_INT(plus.page, 1);
    CHECK_EQ_INT(plus.laps, 4);
    CHECK_EQ_INT(plus.cheats, 1);
    CHECK_EQ_INT(plus.layout, RACE_RECORDS_LAYOUTS - 1);   /* round from NORMAL */
    CHECK_EQ_INT(plus.focus, BEST_FOCUS_LAYOUT);
    static const int enter[] = { DOWN, ENTER, SHIFT_NOTHING };   /* down to the cheats: Enter turns them over */
    reset(enter, 3);
    (void)run(&plus, CIRCUIT);
    CHECK_EQ_INT(plus.cheats, 0);
}

/* on the footer Left and Right are the exe's: Right from Back to Done, Enter leaves; the strip as it was */
static void test_the_footer_keeps_the_exes_keys(void)
{
    race_records_reset();
    BestTimesPlus plus;
    best_times_pages_open(&plus, CIRCUIT, 0, 0, 0, 1);
    const BestTimesPlus before = plus;
    static const int keys[] = { RIGHT, ENTER };
    reset(keys, 2);
    CHECK_EQ_INT(run(&plus, CIRCUIT), 1);
    CHECK(memcmp(&plus, &before, sizeof plus) == 0);
}

/* a row: its name, its cheats' letters (the Warrior's W), its conditions' words, its car */
static void test_a_row_shows_its_marks(void)
{
    race_records_reset();
    RecordTable *t = race_records_make(CIRCUIT, 0, 1, 8);
    CHECK(t != NULL);
    if (t == NULL) return;
    RecordRow *r = &t->totals[0];
    race_records_row_empty(r);
    snprintf((char *)r->row, RECORD_ROW_NAME_MAX, "%s", "ACE");
    const int32_t car = 11, set = 1, time = 9000;
    memcpy(r->row + RECORD_ROW_CAR, &car, 4);
    memcpy(r->row + RECORD_ROW_SET, &set, 4);
    memcpy(r->row + RECORD_ROW_TIME, &time, 4);
    r->marks = race_records_conditions_of(WEATHER_RAIN, 0, 0, LIGHT_TIME_DUSK, 2, 2, 2);
    r->marks.cheats = EXTRAS_MARK_WARRIOR | EXTRAS_MARK_LUNAR;
    BestTimesPlus plus;
    best_times_pages_open(&plus, CIRCUIT, 0, 1, 0, 1);
    static const int keys[] = { SHIFT_NOTHING };
    reset(keys, 1);
    (void)run(&plus, CIRCUIT);
    CHECK(was_drawn("ACE"));
    CHECK(was_drawn("WL"));
    CHECK(was_drawn("RAIN DUSK H"));
    CHECK(was_drawn("WARRIOR"));
    race_records_reset();
}

int main(void)
{
    test_no_plus_is_original();
    test_the_strip_names_the_table();
    test_tab_turns_the_page();
    test_the_keys_step_the_fields();
    test_the_footer_keeps_the_exes_keys();
    test_a_row_shows_its_marks();
    return TEST_REPORT();
}
